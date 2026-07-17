// LiveStreamService implementation
// Business logic migrated from flow/stream/StreamViewerMng.

#include "service/media/impl/LiveStreamServiceImpl.h"

#include "flow/stream/StreamViewer.h"
#include "service/camera/ICameraChannelQuery.h"
#include "service/detail/ServiceRegistry.h"
#include "util/EnvUtil.h"
#include "util/ErrorCode.h"
#include "util/FormatString.h"
#include "util/Log.h"
#include "util/TimingConstants.h"
#include "util/dto/CameraMsgTypes.h"

namespace cosmo::service {

namespace {
    // Stream connection constants
    static constexpr int kDefaultKeepAliveInterval = 10;
    static constexpr const char* kKeepAliveUrl     = "streamkeepalive";
    static constexpr int kDefaultHttpPort          = 8080;
    static constexpr int kDefaultRtcApiPort        = 1985;

    std::string BuildStreamName(const std::string& channelId, const std::string& algCode) {
        return algCode.empty() ? channelId : COSMO_FORMAT("{}_{}", channelId, algCode);
    }

    void PopulateStreamInfo(LiveStream::LiveStreamInfo& info, const std::string& channelId,
                            const std::string& algCode) {
        const std::string streamName = BuildStreamName(channelId, algCode);
        const std::string playMode   = util::GetEnvOrDefault("COSMO_STREAM_PLAY_MODE", "srs");

        info.httpPort          = util::GetEnvIntOrDefault("COSMO_STREAM_HTTP_PORT", kDefaultHttpPort);
        info.rtcApiPort        = util::GetEnvIntOrDefault("COSMO_STREAM_RTC_API_PORT", kDefaultRtcApiPort);
        info.webrtcUrl         = COSMO_FORMAT("/rtc/v1/whep/?app={}&stream={}", "live", streamName);
        info.flvUrl            = COSMO_FORMAT("/live/{}.flv", streamName);
        info.hlsUrl            = COSMO_FORMAT("/live/{}.m3u8", streamName);
        info.keepAliveInterval = kDefaultKeepAliveInterval;
        info.keepAliveUrl      = kKeepAliveUrl;

        if (playMode == "srs" || playMode == "webrtc") {
            info.protocol = "webrtc";
            info.port     = info.rtcApiPort;
            info.url      = info.webrtcUrl;
        } else if (playMode == "srs-flv" || playMode == "httpflv-srs") {
            info.protocol = "httpflv";
            info.port     = info.httpPort;
            info.url      = info.flvUrl;
        } else {
            info.protocol = "httpflv";
            info.port     = info.httpPort;
            info.url      = COSMO_FORMAT("/live?app={}&stream={}", "live", streamName);
        }
    }
}  // namespace

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

LiveStreamServiceImpl::LiveStreamServiceImpl() : is_running_(true) {
    watchdog_thread_ = std::thread(&LiveStreamServiceImpl::HeartBeatWatchdog, this);
    LOG_INFO("{}", "LiveStreamServiceImpl Init");
}

LiveStreamServiceImpl::~LiveStreamServiceImpl() {
    LiveStreamServiceImpl::Stop();
    LOG_INFO("{}", "LiveStreamServiceImpl Delete");
}

void LiveStreamServiceImpl::Stop() {
    std::lock_guard<std::mutex> stop_lock(stop_mtx_);
    if (stopped_) {
        return;
    }
    stopping_.store(true, std::memory_order_release);

    // Wait for any request that entered before stopping_ was published. New
    // requests take a shared lock, observe stopping_, and fail without work.
    std::unique_lock<std::shared_mutex> lifecycle_lock(lifecycle_mtx_);
    is_running_.store(false, std::memory_order_release);
    watchdog_cv_.notify_all();
    if (watchdog_thread_.joinable()) {
        watchdog_thread_.join();
    }

    // StreamViewer teardown stops queues, encoders, and RTMP pushers and may
    // acquire channel locks. Move ownership under mtx_, then destruct only
    // after releasing it to avoid lock inversion with worker callbacks.
    std::vector<cosmo::StreamViewerPtr> viewers;
    {
        std::unique_lock<std::shared_mutex> lock(mtx_);
        viewers.swap(viewers_);
    }
    viewers.clear();
    stopped_ = true;
}

// ---------------------------------------------------------------------------
// HeartBeat watchdog thread — replaces StreamViewerMng::run()
// ---------------------------------------------------------------------------

void LiveStreamServiceImpl::HeartBeatWatchdog() {
    int index = 0;
    std::unique_lock<std::mutex> wait_lock(watchdog_mtx_);
    while (is_running_.load(std::memory_order_acquire)) {
        wait_lock.unlock();
        // Check every 10 seconds for viewers without heartbeat and close them
        if (0 == index % 10) {
            CheckAliveTasks();
        }
        index++;
        wait_lock.lock();
        watchdog_cv_.wait_for(wait_lock, timing::kOneSecondInterval,
                              [this]() { return !is_running_.load(std::memory_order_acquire); });
    }
    LOG_INFO("{}", "LiveStreamServiceImpl watchdog thread stopped");
}

// ---------------------------------------------------------------------------
// ViewerCreate
// ---------------------------------------------------------------------------

cosmo::util::ErrorEnum LiveStreamServiceImpl::ViewerCreate(const std::string& channelId,
                                                           const std::string& algCode,
                                                           LiveStream::LiveStreamInfo& streamInfo) {
    std::shared_lock<std::shared_mutex> lifecycle_lock(lifecycle_mtx_);
    if (stopping_.load(std::memory_order_acquire)) {
        return cosmo::util::ErrorEnum::SysErr;
    }
    auto channel_inst =
        service::ServiceRegistry::Instance().Get<service::ICameraChannelQuery>().GetChannelInst(channelId);
    if (!channel_inst) {
        return cosmo::util::ErrorEnum::CameraNotExist;
    }
    cosmo::util::ErrorEnum channelState = channel_inst->GetUrlStatus();
    if (cosmo::util::ErrorEnum::Success != channelState) {
        LOG_WARN("Channel Stream State {}", channelState);
        return cosmo::util::ErrorEnum::DemuxNoData;
    }

    cosmo::MsgCameraAttr attr;
    if (!channel_inst->GetAttr(attr)) {
        return cosmo::util::ErrorEnum::CameraNotOnline;
    }
    if (cosmo::ChannelStatus::ChannelStatusOnline != attr.channelStatus) {
        return cosmo::util::ErrorEnum::CameraNotOnline;
    }

    // Acquire exclusive lock before encoder count check to prevent TOCTOU race
    std::unique_lock<std::shared_mutex> lock(mtx_);
    if (!((algCode.empty()) && (attr.codec == "H264"))) {
        auto count_dec = ViewerEncoderCountLocked();
        if (count_dec >= view_counts_.load()) {
            LOG_INFO("view sum already {} {}", view_counts_.load(), count_dec);
            return cosmo::util::ErrorEnum::EncodeFailed;
        }
    }

    auto it = FindViewer(channelId, algCode);
    if (it != viewers_.end()) {
        // Increment concurrent viewer count for this stream
        (*it)->UpViewerNum();
        LOG_INFO("view number is {} channelId is {}", (*it)->GetViewerNum(), (*it)->GetChannelId());
        // Populate stream info for existing viewer
        PopulateStreamInfo(streamInfo, channelId, algCode);
        return cosmo::util::ErrorEnum::Success;
    }
    auto viewer = std::make_shared<cosmo::StreamViewer>(channel_inst, channelId, algCode);
    viewers_.push_back(viewer);

    // Release lock before waiting — stream readiness is handled asynchronously.
    // The frontend flv.js player has exponential backoff retry to handle the case
    // where the media server hasn't received the first I-frame yet.
    lock.unlock();

    // Wait for the RTMP stream header to be pushed (first I-frame)
    // so that SRS can serve the stream to the browser.
    constexpr auto kRawStreamReadyTimeout = std::chrono::milliseconds(5000);
    constexpr auto kAlgStreamReadyTimeout = std::chrono::milliseconds(15000);
    const auto stream_ready_timeout       = algCode.empty() ? kRawStreamReadyTimeout : kAlgStreamReadyTimeout;
    if (!viewer->WaitReady(stream_ready_timeout)) {
        LOG_WARN("{}/{} Stream not ready within timeout", channelId, algCode);
        // Clean up ghost viewer on timeout
        std::unique_lock<std::shared_mutex> cleanup_lock(mtx_);
        auto cleanup_it = FindViewer(channelId, algCode);
        if (cleanup_it != viewers_.end()) {
            viewers_.erase(cleanup_it);
        }
        return cosmo::util::ErrorEnum::DemuxNoData;
    }

    // Populate stream info with URLs and ports
    PopulateStreamInfo(streamInfo, channelId, algCode);
    return cosmo::util::ErrorEnum::Success;
}

// ---------------------------------------------------------------------------
// ViewerDelete
// ---------------------------------------------------------------------------

bool LiveStreamServiceImpl::ViewerDelete(const std::string& channelId, const std::string& algCode) {
    std::shared_lock<std::shared_mutex> lifecycle_lock(lifecycle_mtx_);
    if (stopping_.load(std::memory_order_acquire)) {
        return true;
    }

    cosmo::StreamViewerPtr removed;
    {
        std::lock_guard<std::shared_mutex> lock(mtx_);
        auto it = FindViewer(channelId, algCode);

        if (it != viewers_.end()) {
            LOG_INFO("{}/{} viewer num is {}", channelId, algCode, (*it)->GetViewerNum());
            (*it)->DelViewerNum();
            if ((*it)->GetViewerNum() <= 0) {
                LOG_INFO("{}/{} Delete", channelId, algCode);
                removed = std::move(*it);
                viewers_.erase(it);
            }
        } else {
            LOG_INFO("{}/{} Not Exist", channelId, algCode);
        }
    }
    removed.reset();
    return true;
}

// ---------------------------------------------------------------------------
// ViewerHeartBeat
// ---------------------------------------------------------------------------

cosmo::util::ErrorEnum LiveStreamServiceImpl::ViewerHeartBeat(const std::string& channelId,
                                                              const std::string& algCode) {
    std::shared_lock<std::shared_mutex> lifecycle_lock(lifecycle_mtx_);
    if (stopping_.load(std::memory_order_acquire)) {
        return cosmo::util::ErrorEnum::SysErr;
    }
    auto channel_inst =
        service::ServiceRegistry::Instance().Get<service::ICameraChannelQuery>().GetChannelInst(channelId);
    if (!channel_inst) {
        return cosmo::util::ErrorEnum::CameraNotExist;
    }
    cosmo::util::ErrorEnum channelState = channel_inst->GetUrlStatus();
    if (cosmo::util::ErrorEnum::Success != channelState) {
        LOG_WARN("Channel Stream State {}", channelState);
        return channelState;
    }
    std::lock_guard<std::shared_mutex> lock(mtx_);
    LOG_DEBUG("alive channel size {} {}", viewers_.size(), channelId);
    auto it = FindViewer(channelId, algCode);
    if (it != viewers_.end()) {
        (*it)->HeartBeat();
        return cosmo::util::ErrorEnum::Success;
    }

    // A successful keepalive for a missing viewer leaves the browser believing
    // that a stream removed by the watchdog (or a service restart) is still
    // healthy. Return a normal stream error so the client can recreate it.
    return cosmo::util::ErrorEnum::DemuxNoData;
}

// ---------------------------------------------------------------------------
// SetViewCounts
// ---------------------------------------------------------------------------

void LiveStreamServiceImpl::SetViewCounts(int view_num) {
    std::shared_lock<std::shared_mutex> lifecycle_lock(lifecycle_mtx_);
    if (stopping_.load(std::memory_order_acquire)) {
        return;
    }
    view_counts_.store(view_num);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

int LiveStreamServiceImpl::ViewerEncoderCountLocked() const {
    // Caller must hold mtx_ (exclusive or shared)
    return static_cast<int>(std::count_if(viewers_.begin(), viewers_.end(),
                                          [](const auto& viewer) { return viewer->HaveEncoder(); }));
}

void LiveStreamServiceImpl::CheckAliveTasks() {
    std::vector<cosmo::StreamViewerPtr> expired;
    {
        std::lock_guard<std::shared_mutex> lock(mtx_);
        for (auto it = viewers_.begin(); it != viewers_.end();) {
            if ((*it)->HeartBeatCheck()) {
                expired.push_back(std::move(*it));
                it = viewers_.erase(it);
            } else {
                ++it;
            }
        }
    }
    expired.clear();
}

std::vector<cosmo::StreamViewerPtr>::iterator LiveStreamServiceImpl::FindViewer(const std::string& channelId,
                                                                                const std::string& algCode) {
    return std::find_if(viewers_.begin(), viewers_.end(), [&](const cosmo::StreamViewerPtr& viewer) {
        return viewer->GetChannelId() == channelId && viewer->GetAlgId() == algCode;
    });
}

}  // namespace cosmo::service
