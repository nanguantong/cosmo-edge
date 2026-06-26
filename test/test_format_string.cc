#include "catch_amalgamated.hpp"
/*
 * test_format_string.cc — FormatString unit tests
 *
 * Tests the format string macros.
 */

#include "util/FormatString.h"

TEST_CASE("FormatString: COSMO_FORMAT", "[format-string]") {
    SECTION("Format simple string") {
        auto result = COSMO_FORMAT("Hello {} {}", "World", 42);
        REQUIRE(result == "Hello World 42");
    }

    SECTION("Format complex string") {
        auto result = COSMO_FORMAT("Score: {:.2f}, Name: {}", 99.5, "Alice");
        REQUIRE(result == "Score: 99.50, Name: Alice");
    }
}
