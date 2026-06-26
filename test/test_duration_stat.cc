#include "catch_amalgamated.hpp"
/*
 * test_duration_stat.cc — DurationStat unit tests
 *
 * Tests ring-buffer based duration statistics for performance profiling.
 */
#include <thread>

#include "util/DurationStat.h"

using namespace cosmo::util;

TEST_CASE("DurationStat: initial state", "[duration-stat]") {
    DurationStat ds("test");
    auto stats = ds.ComputeStats();

    SECTION("Name is set") {
        REQUIRE(stats.name == "test");
    }

    SECTION("Count is zero initially") {
        REQUIRE(stats.count == 0);
    }

    SECTION("Duration is zero initially") {
        REQUIRE(stats.duration_ns == 0);
    }
}

TEST_CASE("DurationStat: single sample", "[duration-stat]") {
    DurationStat ds("single");

    ds.BeginSample();
    // Small busy wait to ensure non-zero duration
    volatile int x = 0;
    for (int i = 0; i < 1000; ++i)
        x += i;
    (void)x;
    ds.EndSample();

    auto stats = ds.ComputeStats(60000);  // 60s window

    SECTION("Count is 1") {
        REQUIRE(stats.count == 1);
    }

    SECTION("Duration is positive") {
        REQUIRE(stats.duration_ns > 0);
    }

    SECTION("Max and min are equal for single sample") {
        REQUIRE(stats.duration_max_ns == stats.duration_min_ns);
    }

    SECTION("All-time max equals window max") {
        REQUIRE(stats.cost_max_ns == stats.duration_max_ns);
    }
}

TEST_CASE("DurationStat: multiple samples", "[duration-stat]") {
    DurationStat ds("multi");

    for (int i = 0; i < 5; ++i) {
        ds.BeginSample();
        volatile int x = 0;
        for (int j = 0; j < 100; ++j)
            x += j;
        (void)x;
        ds.EndSample();
    }

    auto stats = ds.ComputeStats(60000);

    SECTION("Count matches sample count") {
        REQUIRE(stats.count == 5);
    }

    SECTION("Total duration is positive") {
        REQUIRE(stats.duration_ns > 0);
    }

    SECTION("Max >= Min") {
        REQUIRE(stats.duration_max_ns >= stats.duration_min_ns);
    }
}

TEST_CASE("DurationStat: all-time min/max tracking", "[duration-stat]") {
    DurationStat ds("minmax");

    // Sample 1: short
    ds.BeginSample();
    ds.EndSample();

    // Sample 2: longer
    ds.BeginSample();
    volatile int x = 0;
    for (int i = 0; i < 10000; ++i)
        x += i;
    (void)x;
    ds.EndSample();

    auto stats = ds.ComputeStats(60000);

    SECTION("All-time max >= all-time min") {
        REQUIRE(stats.cost_max_ns >= stats.cost_min_ns);
    }
}
