#include "catch_amalgamated.hpp"
/*
 * test_video_frame_service_impl.cc — VideoFrameServiceImpl unit tests (DEBT-T01)
 *
 * Strategy: VideoFrameServiceImpl wraps VPU image processing (bmcv).
 * Construction creates VideoFrameProc which requires bmlib device init.
 * All tests tagged [.device].
 */
#include <memory>
#include <stdexcept>
#include <string>

#include "media/IOsdTextRenderer.h"
#include "mem/DeviceContext.h"
#include "mem/IDeviceContext.h"
#include "service/detail/ServiceRegistry.h"
#include "service/media/impl/VideoFrameServiceImpl.h"

using namespace cosmo::service;

namespace {

class StubOsdTextRenderer final : public cosmo::media::IOsdTextRenderer {
public:
    bool Init(const std::string&) override {
        return true;
    }

    bool IsReady() const override {
        return true;
    }

    TextBitmap RenderString(const std::string&, float) const override {
        return {};
    }

    OutlinedTextBitmap RenderStringWithOutline(const std::string&, float) const override {
        return {};
    }
};

class ScopedDeviceContext {
public:
    ScopedDeviceContext() {
        if (ServiceRegistry::Instance().Has<cosmo::mem::IDeviceContext>()) {
            throw std::logic_error("device context already registered by another test");
        }
        device_context_ = std::make_unique<cosmo::mem::DeviceContext>();
        ServiceRegistry::Instance().Set<cosmo::mem::IDeviceContext>(device_context_.get());
    }

    ~ScopedDeviceContext() {
        ServiceRegistry::Instance().Set<cosmo::mem::IDeviceContext>(nullptr);
    }

    ScopedDeviceContext(const ScopedDeviceContext&)            = delete;
    ScopedDeviceContext& operator=(const ScopedDeviceContext&) = delete;

private:
    std::unique_ptr<cosmo::mem::DeviceContext> device_context_;
};

class ScopedOsdRegistration {
public:
    explicit ScopedOsdRegistration(cosmo::media::IOsdTextRenderer& osd) {
        if (ServiceRegistry::Instance().Has<cosmo::media::IOsdTextRenderer>()) {
            throw std::logic_error("OSD renderer already registered by another test");
        }
        ServiceRegistry::Instance().Set<cosmo::media::IOsdTextRenderer>(&osd);
    }

    ~ScopedOsdRegistration() {
        ServiceRegistry::Instance().Set<cosmo::media::IOsdTextRenderer>(nullptr);
    }

    ScopedOsdRegistration(const ScopedOsdRegistration&)            = delete;
    ScopedOsdRegistration& operator=(const ScopedOsdRegistration&) = delete;
};

class VideoFrameServiceFixture {
public:
    VideoFrameServiceFixture() : osd_registration_(osd_) {}

    VideoFrameServiceFixture(const VideoFrameServiceFixture&)            = delete;
    VideoFrameServiceFixture& operator=(const VideoFrameServiceFixture&) = delete;

    VideoFrameServiceImpl& Service() {
        return service_;
    }

private:
    ScopedDeviceContext device_context_;
    StubOsdTextRenderer osd_;
    ScopedOsdRegistration osd_registration_;
    VideoFrameServiceImpl service_;
};

}  // namespace

TEST_CASE("VideoFrameServiceImpl: construction and destruction", "[VideoFrameService][.device]") {
    REQUIRE_NOTHROW([]() { VideoFrameServiceFixture fixture; }());
}

TEST_CASE("VideoFrameServiceImpl: EncodeJpeg with null frame returns empty", "[VideoFrameService][.device]") {
    VideoFrameServiceFixture fixture;
    auto& sut = fixture.Service();
    VideoFramePtr nullFrame;
    auto data = sut.EncodeJpeg(nullFrame);
    REQUIRE(data.empty());
}

TEST_CASE("VideoFrameServiceImpl: DecodeJpeg with empty data returns null", "[VideoFrameService][.device]") {
    VideoFrameServiceFixture fixture;
    auto& sut = fixture.Service();
    std::vector<uint8_t> emptyData;
    auto frame = sut.DecodeJpeg(emptyData);
    REQUIRE(frame == nullptr);
}

TEST_CASE("VideoFrameServiceImpl: EnsureHostData with null frame", "[VideoFrameService][.device]") {
    VideoFrameServiceFixture fixture;
    auto& sut = fixture.Service();
    VideoFramePtr nullFrame;
    REQUIRE(sut.EnsureHostData(nullFrame) == false);
}

TEST_CASE("VideoFrameServiceImpl: Crop with null frame", "[VideoFrameService][.device]") {
    VideoFrameServiceFixture fixture;
    auto& sut = fixture.Service();
    VideoFramePtr nullFrame;
    cosmo::util::Box roi{0, 0, 100, 100};
    auto result = sut.Crop(nullFrame, roi);
    REQUIRE(result == nullptr);
}
