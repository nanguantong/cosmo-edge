#include <limits>
#include <memory>
#include <string>

#include "catch_amalgamated.hpp"
#include "media/PixelFormatUtils.h"
#include "media/VideoFrame.h"
#include "util/VideoInfo.h"

#ifdef COSMO_NN_USE_SOPHON_BACKEND
#include "media/IOsdTextRenderer.h"
#include "media/VideoFrameProcSophon.h"
#include "mem/DeviceContext.h"
#include "mem/IDeviceContext.h"
#include "service/detail/ServiceRegistry.h"
#include "service/infra/impl/MemoryPoolServiceImpl.h"
#endif

namespace cosmo::media {

TEST_CASE("Frame size calculation rejects unsafe dimensions", "[video-frame-safety]") {
    REQUIRE_FALSE(PixelFormatUtils::CalculateFrameSize(-1, 1080, PixelFormat::PIXEL_I420));
    REQUIRE_FALSE(PixelFormatUtils::CalculateFrameSize(1920, 0, PixelFormat::PIXEL_I420));
    REQUIRE_FALSE(PixelFormatUtils::CalculateFrameSize(1920, 1080, PixelFormat::PIXEL_UNKNOWN));
    REQUIRE_FALSE(PixelFormatUtils::CalculateFrameSize(1919, 1080, PixelFormat::PIXEL_I420));
    REQUIRE_FALSE(PixelFormatUtils::CalculateFrameSize(1920, 1079, PixelFormat::PIXEL_NV12));
    REQUIRE_FALSE(PixelFormatUtils::CalculateFrameSize(1919, 1080, PixelFormat::PIXEL_YUYV));
    REQUIRE_FALSE(PixelFormatUtils::CalculateFrameSize(
        std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), PixelFormat::PIXEL_RGB32F));

    const auto max_rgb_size =
        PixelFormatUtils::CalculateFrameSize(kVideoMaxWidth, kVideoMaxHeight, PixelFormat::PIXEL_RGB8);
    REQUIRE(max_rgb_size);
    REQUIRE(*max_rgb_size == static_cast<size_t>(kVideoFrameMaxSize));
    REQUIRE_FALSE(
        PixelFormatUtils::CalculateFrameSize(kVideoMaxWidth, kVideoMaxHeight, PixelFormat::PIXEL_RGBA8));

    const auto i420_size = PixelFormatUtils::CalculateFrameSize(1920, 1080, PixelFormat::PIXEL_I420);
    REQUIRE(i420_size);
    REQUIRE(*i420_size == 3110400);
}

TEST_CASE("VideoFrame rejects unsafe dimensions before allocation", "[video-frame-safety]") {
    VideoFrame negative_width(-1, 1080, PixelFormat::PIXEL_I420);
    REQUIRE_FALSE(negative_width.Active());
    REQUIRE(negative_width.GetWidth() == 0);
    REQUIRE(negative_width.GetHeight() == 0);
    REQUIRE(negative_width.GetSize() == 0);

    VideoFrame oversized(kVideoMaxWidth, kVideoMaxHeight, PixelFormat::PIXEL_RGBA8);
    REQUIRE_FALSE(oversized.Active());
    REQUIRE(oversized.GetWidth() == 0);
    REQUIRE(oversized.GetHeight() == 0);
    REQUIRE(oversized.GetSize() == 0);
}

TEST_CASE("Crop ROI normalization safely clips and aligns recoverable input", "[video-frame-safety]") {
    constexpr int kMinDimension = 16;
    constexpr int kMaxDimension = 8192;
    const auto normalize        = [=](const util::Box& roi) {
        return PixelFormatUtils::NormalizeCropRoi(64, 64, PixelFormat::PIXEL_I420, roi, kMinDimension,
                                                         kMaxDimension);
    };

    REQUIRE(normalize({-1, 0, 16, 16}) == util::Box(0, 0, 16, 16));
    REQUIRE(normalize({0, -1, 16, 16}) == util::Box(0, 0, 16, 16));
    REQUIRE(normalize({0, 0, 6, 16}) == util::Box(0, 0, 16, 16));
    REQUIRE(normalize({0, 0, 15, 15}) == util::Box(0, 0, 16, 16));
    REQUIRE(normalize({30, 30, 1, 1}) == util::Box(23, 23, 16, 16));
    REQUIRE(normalize({60, 0, 8, 8}) == util::Box(48, 0, 16, 16));
    REQUIRE(normalize({0, 60, 8, 8}) == util::Box(0, 48, 16, 16));
    REQUIRE_FALSE(normalize({0, 0, 0, 16}));
    REQUIRE_FALSE(normalize({64, 0, 16, 16}));
    REQUIRE_FALSE(normalize({std::numeric_limits<int>::max(), 0, 16, 16}));
    REQUIRE_FALSE(normalize({1, 1, std::numeric_limits<int>::max(), 16}));
}

#ifdef COSMO_NN_USE_SOPHON_BACKEND
namespace {

    class StubOsdTextRenderer final : public IOsdTextRenderer {
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

    class SophonCropFixture {
    public:
        SophonCropFixture() {
            device_context_ = std::make_unique<mem::DeviceContext>();
            service::ServiceRegistry::Instance().Set<mem::IDeviceContext>(device_context_.get());
            memory_pool_ = std::make_unique<service::MemoryPoolServiceImpl>();
            frame_proc_  = std::make_unique<VideoFrameProcSophon>(device_context_->GetMediaHandle(), osd_);
        }

        ~SophonCropFixture() {
            frame_proc_.reset();
            memory_pool_.reset();
            service::ServiceRegistry::Instance().Set<mem::IDeviceContext>(nullptr);
            device_context_.reset();
        }

        SophonCropFixture(const SophonCropFixture&)            = delete;
        SophonCropFixture& operator=(const SophonCropFixture&) = delete;

        VideoFrameProcSophon& FrameProc() {
            return *frame_proc_;
        }

    private:
        StubOsdTextRenderer osd_;
        std::unique_ptr<mem::DeviceContext> device_context_;
        std::unique_ptr<service::MemoryPoolServiceImpl> memory_pool_;
        std::unique_ptr<VideoFrameProcSophon> frame_proc_;
    };

}  // namespace

TEST_CASE("Sophon crop rejects unsafe ROIs before VPP", "[sophon-crop][.device]") {
    SophonCropFixture fixture;
    auto source = std::make_shared<VideoFrame>(64, 64, PixelFormat::PIXEL_I420, 42, 123456);
    REQUIRE(VideoFrameValid(source, true));

    const util::Box invalid_rois[] = {
        {0, 0, 0, 16},
        {0, 0, 16, -2},
        {64, 0, 16, 16},
        {0, 64, 16, 16},
        {std::numeric_limits<int>::max(), 0, 16, 16},
        {0, std::numeric_limits<int>::max(), 16, 16},
        {1, 1, std::numeric_limits<int>::max(), 16},
        {1, 1, 16, std::numeric_limits<int>::max()},
    };

    for (const auto& roi : invalid_rois) {
        INFO("ROI x=" << roi.x << " y=" << roi.y << " width=" << roi.width << " height=" << roi.height);
        REQUIRE(fixture.FrameProc().Crop(source, roi) == nullptr);
    }
}

TEST_CASE("Sophon crop normalizes recoverable ROIs", "[sophon-crop][.device]") {
    SophonCropFixture fixture;
    auto source = std::make_shared<VideoFrame>(64, 64, PixelFormat::PIXEL_I420, 42, 123456);
    REQUIRE(VideoFrameValid(source, true));

    struct CropCase {
        util::Box roi;
        size_t expected_width;
        size_t expected_height;
    };
    const CropCase crop_cases[] = {
        {{1, 3, 15, 15}, 16, 16},
        {{60, 60, 8, 8}, 16, 16},
        {{-1, 0, 16, 16}, 16, 16},
        {{48, 48, 16, 16}, 16, 16},
    };

    for (const auto& crop_case : crop_cases) {
        INFO("ROI x=" << crop_case.roi.x << " y=" << crop_case.roi.y << " width=" << crop_case.roi.width
                      << " height=" << crop_case.roi.height);
        auto result = fixture.FrameProc().Crop(source, crop_case.roi);
        REQUIRE(VideoFrameValid(result, true));
        REQUIRE(result->GetWidth() == crop_case.expected_width);
        REQUIRE(result->GetHeight() == crop_case.expected_height);
        REQUIRE(result->GetFrameIndex() == source->GetFrameIndex());
        REQUIRE(result->GetTimestamp() == source->GetTimestamp());
    }
}
#endif

}  // namespace cosmo::media
