#include "catch_amalgamated.hpp"
/*
 * test_limited_type.cc — LimitedType unit tests
 *
 * Tests String<Min,Max>, RangeInt<Min,Max>, and RangeValue<T>
 * range validation, operators, and error handling.
 */
#include "util/Exception.h"
#include "util/LimitedType.h"

using namespace cosmo::util;

// ---------------------------------------------------------------------------
// String<MinSize, MaxSize>
// ---------------------------------------------------------------------------

TEST_CASE("LimitedType/String: valid construction", "[limited-type]") {
    SECTION("Within range - lvalue") {
        REQUIRE_NOTHROW(String<1, 10>("hello"));
    }

    SECTION("Within range - rvalue") {
        std::string s = "world";
        REQUIRE_NOTHROW(String<1, 10>(std::move(s)));
    }

    SECTION("Exact minimum length") {
        REQUIRE_NOTHROW(String<3, 10>("abc"));
    }

    SECTION("Exact maximum length") {
        REQUIRE_NOTHROW(String<1, 5>("abcde"));
    }

    SECTION("Zero minimum allows empty") {
        REQUIRE_NOTHROW(String<0, 10>(""));
    }
}

TEST_CASE("LimitedType/String: invalid construction throws", "[limited-type]") {
    SECTION("Too short") {
        REQUIRE_THROWS_AS((String<3, 10>("ab")), ErrorMessage);
    }

    SECTION("Too long") {
        REQUIRE_THROWS_AS((String<1, 3>("abcdef")), ErrorMessage);
    }

    SECTION("Empty when min > 0") {
        REQUIRE_THROWS_AS((String<1, 10>("")), ErrorMessage);
    }
}

TEST_CASE("LimitedType/String: assignment", "[limited-type]") {
    String<1, 10> s("init");

    SECTION("Valid assignment") {
        REQUIRE_NOTHROW(s = "new_val");
        REQUIRE(s.ToString() == "new_val");
    }

    SECTION("Invalid assignment throws") {
        REQUIRE_THROWS_AS(s = "this is way too long for 10", ErrorMessage);
    }

    SECTION("Move assignment") {
        std::string mv = "moved";
        REQUIRE_NOTHROW(s = std::move(mv));
        REQUIRE(s.ToString() == "moved");
    }
}

TEST_CASE("LimitedType/String: accessors", "[limited-type]") {
    String<1, 20> s("hello");

    SECTION("empty() returns false for non-empty") {
        REQUIRE_FALSE(s.empty());
    }

    SECTION("c_str() matches") {
        REQUIRE(std::string(s.c_str()) == "hello");
    }

    SECTION("ToString() returns value") {
        REQUIRE(s.ToString() == "hello");
    }

    SECTION("ToRefString() returns reference") {
        const std::string& ref = s.ToRefString();
        REQUIRE(ref == "hello");
    }

    SECTION("Implicit string conversion") {
        std::string str = static_cast<const std::string&>(s);
        REQUIRE(str == "hello");
    }
}

TEST_CASE("LimitedType/String: comparison operators", "[limited-type]") {
    String<1, 20> a("abc");
    String<1, 20> b("abc");
    String<1, 20> c("xyz");

    SECTION("Equal strings") {
        REQUIRE(a == b);
    }

    SECTION("Not equal strings") {
        REQUIRE(a != c);
    }

    SECTION("Compare with std::string") {
        REQUIRE(a == std::string("abc"));
        REQUIRE(a != std::string("xyz"));
    }
}

TEST_CASE("LimitedType/String: UTF-8 length counting", "[limited-type]") {
    // Chinese chars count as 1 character each in UTF-8 length
    SECTION("Chinese characters within range") {
        REQUIRE_NOTHROW(String<1, 5>("你好"));  // 2 UTF-8 chars
    }

    SECTION("Mixed ASCII and Chinese") {
        REQUIRE_NOTHROW(String<1, 10>("hi你好"));  // 4 UTF-8 chars
    }
}

// ---------------------------------------------------------------------------
// RangeInt<MinValue, MaxValue>
// ---------------------------------------------------------------------------

TEST_CASE("LimitedType/RangeInt: valid construction", "[limited-type]") {
    SECTION("Within range") {
        REQUIRE_NOTHROW(RangeInt<0, 100>(50));
    }

    SECTION("At minimum") {
        REQUIRE_NOTHROW(RangeInt<-10, 10>(-10));
    }

    SECTION("At maximum") {
        REQUIRE_NOTHROW(RangeInt<-10, 10>(10));
    }
}

TEST_CASE("LimitedType/RangeInt: out of range throws", "[limited-type]") {
    SECTION("Below minimum") {
        REQUIRE_THROWS_AS((RangeInt<0, 100>(-1)), ErrorMessage);
    }

    SECTION("Above maximum") {
        REQUIRE_THROWS_AS((RangeInt<0, 100>(101)), ErrorMessage);
    }
}

TEST_CASE("LimitedType/RangeInt: assignment", "[limited-type]") {
    RangeInt<0, 100> ri(50);

    SECTION("Valid assignment") {
        ri = 75;
        REQUIRE(static_cast<int>(ri) == 75);
    }

    SECTION("Invalid assignment throws") {
        REQUIRE_THROWS_AS(ri = 200, ErrorMessage);
    }
}

TEST_CASE("LimitedType/RangeInt: implicit int conversion", "[limited-type]") {
    RangeInt<0, 100> ri(42);
    int val = ri;
    REQUIRE(val == 42);
}

TEST_CASE("LimitedType/RangeInt: comparison operators", "[limited-type]") {
    RangeInt<0, 100> a(10);
    RangeInt<0, 200> b(10);
    RangeInt<0, 100> c(20);

    SECTION("Equal values across different ranges") {
        REQUIRE(a == b);
    }

    SECTION("Compare with int") {
        REQUIRE(a == 10);
    }
}

// ---------------------------------------------------------------------------
// RangeValue<T>
// ---------------------------------------------------------------------------

TEST_CASE("LimitedType/RangeValue: construction with explicit range", "[limited-type]") {
    SECTION("Within range") {
        REQUIRE_NOTHROW(RangeValue<float>(0.0f, 1.0f, 0.5f));
    }

    SECTION("At boundaries") {
        REQUIRE_NOTHROW(RangeValue<float>(0.0f, 1.0f, 0.0f));
        REQUIRE_NOTHROW(RangeValue<float>(0.0f, 1.0f, 1.0f));
    }

    SECTION("Out of range throws") {
        REQUIRE_THROWS_AS(RangeValue<float>(0.0f, 1.0f, 1.5f), ErrorMessage);
        REQUIRE_THROWS_AS(RangeValue<float>(0.0f, 1.0f, -0.1f), ErrorMessage);
    }
}

TEST_CASE("LimitedType/RangeValue: default range uses numeric limits", "[limited-type]") {
    // Single arg ctor uses numeric_limits as range — should never throw for normal values
    REQUIRE_NOTHROW(RangeValue<int>(0));
    REQUIRE_NOTHROW(RangeValue<int>(1000000));
    REQUIRE_NOTHROW(RangeValue<int>(-1000000));
}

TEST_CASE("LimitedType/RangeValue: assignment", "[limited-type]") {
    RangeValue<double> rv(0.0, 100.0, 50.0);

    SECTION("Valid assignment") {
        rv = 75.0;
        REQUIRE(static_cast<double>(rv) == 75.0);
    }

    SECTION("Out of range assignment throws") {
        REQUIRE_THROWS_AS(rv = 101.0, ErrorMessage);
    }
}

TEST_CASE("LimitedType/RangeValue: arithmetic operators", "[limited-type]") {
    RangeValue<int> a(10);
    RangeValue<int> b(3);

    SECTION("Addition") {
        int result = a + b;
        REQUIRE(result == 13);
    }

    SECTION("Subtraction") {
        int result = a - b;
        REQUIRE(result == 7);
    }

    SECTION("Multiplication with scalar") {
        int result = a * 2;
        REQUIRE(result == 20);
    }
}

TEST_CASE("LimitedType/RangeValue: min/max functions", "[limited-type]") {
    RangeValue<int> a(10);
    RangeValue<int> b(20);

    SECTION("min of two RangeValues") {
        REQUIRE(cosmo::util::min(a, b) == 10);
    }

    SECTION("max of two RangeValues") {
        REQUIRE(cosmo::util::max(a, b) == 20);
    }

    SECTION("min with raw T") {
        REQUIRE(cosmo::util::min(a, 5) == 5);
    }

    SECTION("max with raw T") {
        REQUIRE(cosmo::util::max(a, 15) == 15);
    }
}
