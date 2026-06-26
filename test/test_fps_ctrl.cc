#include "catch_amalgamated.hpp"
/*
 * test_fps_ctrl.cc — FpsCtrl unit tests
 *
 * Tests frame rate control logic: IsFilter behavior with different
 * real/ctrl fps combinations.
 */
#include "util/FpsCtrl.h"

using namespace cosmo::util;

TEST_CASE("FpsCtrl: zero real fps filters everything", "[fps-ctrl]") {
    FpsCtrl ctrl(0.0f, 10.0f);

    SECTION("Frame 0 is filtered") {
        REQUIRE(ctrl.IsFilter(0) == true);
    }

    SECTION("Frame 100 is filtered") {
        REQUIRE(ctrl.IsFilter(100) == true);
    }
}

TEST_CASE("FpsCtrl: zero ctrl fps does not filter", "[fps-ctrl]") {
    FpsCtrl ctrl(25.0f, 0.0f);

    SECTION("Frame 0 passes") {
        REQUIRE(ctrl.IsFilter(0) == false);
    }

    SECTION("Frame 10 passes") {
        REQUIRE(ctrl.IsFilter(10) == false);
    }
}

TEST_CASE("FpsCtrl: real fps <= ctrl fps does not filter", "[fps-ctrl]") {
    FpsCtrl ctrl(25.0f, 30.0f);

    REQUIRE(ctrl.IsFilter(0) == false);
    REQUIRE(ctrl.IsFilter(5) == false);
    REQUIRE(ctrl.IsFilter(24) == false);
}

TEST_CASE("FpsCtrl: downsampling from 25fps to 5fps", "[fps-ctrl]") {
    FpsCtrl ctrl(25.0f, 5.0f);

    // Count how many frames pass out of 25
    int pass_count = 0;
    for (size_t i = 0; i < 25; ++i) {
        if (!ctrl.IsFilter(i)) {
            ++pass_count;
        }
    }

    SECTION("Approximately 5 frames pass per 25") {
        REQUIRE(pass_count == 5);
    }
}

TEST_CASE("FpsCtrl: downsampling from 30fps to 10fps", "[fps-ctrl]") {
    FpsCtrl ctrl(30.0f, 10.0f);

    int pass_count = 0;
    for (size_t i = 0; i < 30; ++i) {
        if (!ctrl.IsFilter(i)) {
            ++pass_count;
        }
    }

    REQUIRE(pass_count == 10);
}

TEST_CASE("FpsCtrl: ChangeFps", "[fps-ctrl]") {
    FpsCtrl ctrl(25.0f, 25.0f);
    // Initially no filtering
    REQUIRE(ctrl.IsFilter(0) == false);

    // Change to downsample
    ctrl.ChangeFps(25.0f, 5.0f);
    int pass_count = 0;
    for (size_t i = 0; i < 25; ++i) {
        if (!ctrl.IsFilter(i)) {
            ++pass_count;
        }
    }
    REQUIRE(pass_count == 5);
}

TEST_CASE("FpsCtrl: GetRealFps and SetRealFps", "[fps-ctrl]") {
    FpsCtrl ctrl(25.0f, 10.0f);
    REQUIRE(ctrl.GetRealFps() == Catch::Approx(25.0));

    ctrl.SetRealFps(30.0f);
    REQUIRE(ctrl.GetRealFps() == Catch::Approx(30.0));
}
