#include "catch_amalgamated.hpp"
/*
 * test_safe_parse.cc — SafeParse unit tests
 *
 * Tests ParseInt, ParseDouble, ParseFloat with valid inputs,
 * invalid inputs, edge cases, and default values.
 */
#include "util/SafeParse.h"

using namespace cosmo::util;

// ---------------------------------------------------------------------------
// ParseInt
// ---------------------------------------------------------------------------

TEST_CASE("SafeParse/ParseInt: valid integers", "[safe-parse]") {
    SECTION("Simple positive int") {
        REQUIRE(ParseInt("42") == 42);
    }

    SECTION("Negative int") {
        REQUIRE(ParseInt("-7") == -7);
    }

    SECTION("Zero") {
        REQUIRE(ParseInt("0") == 0);
    }

    SECTION("Leading whitespace") {
        REQUIRE(ParseInt("  123") == 123);
    }

    SECTION("Leading plus sign") {
        REQUIRE(ParseInt("+99") == 99);
    }

    SECTION("Tabs and newlines") {
        REQUIRE(ParseInt("\t\n 42") == 42);
    }
}

TEST_CASE("SafeParse/ParseInt: invalid inputs return default", "[safe-parse]") {
    SECTION("Empty string") {
        REQUIRE(ParseInt("") == 0);
    }

    SECTION("Non-numeric string") {
        REQUIRE(ParseInt("abc") == 0);
    }

    SECTION("Null pointer") {
        REQUIRE(ParseInt(static_cast<const char*>(nullptr)) == 0);
    }

    SECTION("All whitespace") {
        REQUIRE(ParseInt("   ") == 0);
    }

    SECTION("Custom default value") {
        REQUIRE(ParseInt("abc", -1) == -1);
    }
}

TEST_CASE("SafeParse/ParseInt: different integer types", "[safe-parse]") {
    SECTION("long long") {
        REQUIRE(ParseInt<long long>("9999999999") == 9999999999LL);
    }

    SECTION("uint16_t") {
        REQUIRE(ParseInt<uint16_t>("65535") == 65535);
    }

    SECTION("int8_t") {
        REQUIRE(ParseInt<int8_t>("127") == 127);
    }
}

TEST_CASE("SafeParse/ParseInt: std::string overload", "[safe-parse]") {
    std::string s = "256";
    REQUIRE(ParseInt(s) == 256);

    std::string empty;
    REQUIRE(ParseInt(empty) == 0);
}

// ---------------------------------------------------------------------------
// ParseDouble
// ---------------------------------------------------------------------------

TEST_CASE("SafeParse/ParseDouble: valid doubles", "[safe-parse]") {
    SECTION("Simple decimal") {
        REQUIRE(ParseDouble("3.14") == Catch::Approx(3.14));
    }

    SECTION("Negative double") {
        REQUIRE(ParseDouble("-2.5") == Catch::Approx(-2.5));
    }

    SECTION("Integer as double") {
        REQUIRE(ParseDouble("42") == Catch::Approx(42.0));
    }

    SECTION("Scientific notation") {
        REQUIRE(ParseDouble("1e3") == Catch::Approx(1000.0));
    }
}

TEST_CASE("SafeParse/ParseDouble: invalid inputs return default", "[safe-parse]") {
    SECTION("Empty string") {
        REQUIRE(ParseDouble("") == 0.0);
    }

    SECTION("Null pointer") {
        REQUIRE(ParseDouble(static_cast<const char*>(nullptr)) == 0.0);
    }

    SECTION("Non-numeric string") {
        REQUIRE(ParseDouble("xyz") == 0.0);
    }

    SECTION("Custom default") {
        REQUIRE(ParseDouble("xyz", -1.0) == -1.0);
    }
}

TEST_CASE("SafeParse/ParseDouble: std::string overload", "[safe-parse]") {
    std::string s = "2.718";
    REQUIRE(ParseDouble(s) == Catch::Approx(2.718));
}

// ---------------------------------------------------------------------------
// ParseFloat
// ---------------------------------------------------------------------------

TEST_CASE("SafeParse/ParseFloat: valid floats", "[safe-parse]") {
    SECTION("Simple decimal") {
        REQUIRE(ParseFloat("1.5") == Catch::Approx(1.5f));
    }

    SECTION("Negative float") {
        REQUIRE(ParseFloat("-0.5") == Catch::Approx(-0.5f));
    }
}

TEST_CASE("SafeParse/ParseFloat: invalid inputs return default", "[safe-parse]") {
    SECTION("Empty string") {
        REQUIRE(ParseFloat("") == 0.0f);
    }

    SECTION("Null pointer") {
        REQUIRE(ParseFloat(static_cast<const char*>(nullptr)) == 0.0f);
    }

    SECTION("Non-numeric") {
        REQUIRE(ParseFloat("not_a_float", 9.9f) == 9.9f);
    }
}

TEST_CASE("SafeParse/ParseFloat: std::string overload", "[safe-parse]") {
    std::string s = "3.14";
    REQUIRE(ParseFloat(s) == Catch::Approx(3.14f));
}
