#include "catch_amalgamated.hpp"

#include "util/RtspUrlUtil.h"

using cosmo::util::NormalizeRtspUrl;
using cosmo::util::RedactRtspUrl;

TEST_CASE("RtspUrlUtil: normalize unescaped at signs in credentials", "[rtsp-url]") {
    REQUIRE(NormalizeRtspUrl("rtsp://admin:plain@192.168.1.1:554/live") ==
            "rtsp://admin:plain@192.168.1.1:554/live");
    REQUIRE(NormalizeRtspUrl("rtsp://admin:Jdzlab@2026@192.168.1.117:554/Streaming/Channels/101") ==
            "rtsp://admin:Jdzlab%402026@192.168.1.117:554/Streaming/Channels/101");
    REQUIRE(NormalizeRtspUrl("rtsp://admin:p@a@s@s@camera.local/live") ==
            "rtsp://admin:p%40a%40s%40s@camera.local/live");
}

TEST_CASE("RtspUrlUtil: preserve encoded and unrelated URLs", "[rtsp-url]") {
    REQUIRE(NormalizeRtspUrl("rtsp://admin:p%40ss@camera.local/live") ==
            "rtsp://admin:p%40ss@camera.local/live");
    REQUIRE(NormalizeRtspUrl("rtsp://192.168.1.1:554/live") == "rtsp://192.168.1.1:554/live");
    REQUIRE(NormalizeRtspUrl("http://admin:p@ss@example.com/live") ==
            "http://admin:p@ss@example.com/live");
    REQUIRE(NormalizeRtspUrl("").empty());
}

TEST_CASE("RtspUrlUtil: redact passwords for logs", "[rtsp-url]") {
    REQUIRE(RedactRtspUrl("rtsp://admin:Jdzlab@2026@192.168.1.117:554/live") ==
            "rtsp://admin:***@192.168.1.117:554/live");
    REQUIRE(RedactRtspUrl("rtsp://admin@192.168.1.1/live") ==
            "rtsp://admin@192.168.1.1/live");
    REQUIRE(RedactRtspUrl("rtsp://192.168.1.1/live") == "rtsp://192.168.1.1/live");
}
