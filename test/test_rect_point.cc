#include "catch_amalgamated.hpp"
/*
 * test_rect_point.cc — Box_ and Point_ unit tests
 *
 * Tests construction, operators, geometric queries (contains, area,
 * intersection, union), and utility functions (Perpendicular, Length,
 * LengthSquare, LineDirection).
 */
#include "util/Rect.h"

using namespace cosmo::util;

// ---------------------------------------------------------------------------
// Point_
// ---------------------------------------------------------------------------

TEST_CASE("Point: default construction", "[point]") {
    Point p;
    REQUIRE(p.x == 0);
    REQUIRE(p.y == 0);
}

TEST_CASE("Point: parameterized construction", "[point]") {
    Point2f p(3.0f, 4.0f);
    REQUIRE(p.x == 3.0f);
    REQUIRE(p.y == 4.0f);
}

TEST_CASE("Point: arithmetic operators", "[point]") {
    Point2i a(10, 20);
    Point2i b(3, 5);

    SECTION("Addition") {
        auto c = a + b;
        REQUIRE(c.x == 13);
        REQUIRE(c.y == 25);
    }

    SECTION("Subtraction") {
        auto c = a - b;
        REQUIRE(c.x == 7);
        REQUIRE(c.y == 15);
    }

    SECTION("Multiplication by scalar") {
        auto c = a * 2;
        REQUIRE(c.x == 20);
        REQUIRE(c.y == 40);
    }

    SECTION("Division by scalar") {
        auto c = a / 2;
        REQUIRE(c.x == 5);
        REQUIRE(c.y == 10);
    }

    SECTION("Division by zero returns zero point") {
        auto c = a / 0;
        REQUIRE(c.x == 0);
        REQUIRE(c.y == 0);
    }
}

TEST_CASE("Point: Perpendicular", "[point]") {
    Point2i p(3, 4);
    auto perp = Perpendicular(p);
    REQUIRE(perp.x == -4);
    REQUIRE(perp.y == 3);
}

TEST_CASE("Point: LengthSquare", "[point]") {
    Point2i p(3, 4);
    REQUIRE(LengthSquare(p) == 25);
}

TEST_CASE("Point: Length", "[point]") {
    Point2d p(3.0, 4.0);
    REQUIRE(Length(p) == Catch::Approx(5.0));
}

TEST_CASE("Point: LineDirection", "[point]") {
    SECTION("Non-zero vector") {
        Point2d p(3.0, 4.0);
        auto dir = LineDirection(p, 10.0);
        REQUIRE(dir.x == Catch::Approx(6.0));
        REQUIRE(dir.y == Catch::Approx(8.0));
    }

    SECTION("Zero vector returns zero") {
        Point2d p(0.0, 0.0);
        auto dir = LineDirection(p, 10.0);
        REQUIRE(dir.x == 0.0);
        REQUIRE(dir.y == 0.0);
    }
}

// ---------------------------------------------------------------------------
// Box_
// ---------------------------------------------------------------------------

TEST_CASE("Box: default construction", "[box]") {
    Box b;
    REQUIRE(b.x == 0);
    REQUIRE(b.y == 0);
    REQUIRE(b.width == 0);
    REQUIRE(b.height == 0);
    REQUIRE(b.empty());
}

TEST_CASE("Box: parameterized construction", "[box]") {
    Box b(10, 20, 100, 50);
    REQUIRE(b.x == 10);
    REQUIRE(b.y == 20);
    REQUIRE(b.width == 100);
    REQUIRE(b.height == 50);
}

TEST_CASE("Box: two-point construction", "[box]") {
    Point p1(10, 20);
    Point p2(110, 70);
    Box b(p1, p2);
    REQUIRE(b.x == 10);
    REQUIRE(b.y == 20);
    REQUIRE(b.width == 100);
    REQUIRE(b.height == 50);
}

TEST_CASE("Box: tl() and br()", "[box]") {
    Box b(10, 20, 100, 50);
    auto tl = b.tl();
    auto br = b.br();
    REQUIRE(tl.x == 10);
    REQUIRE(tl.y == 20);
    REQUIRE(br.x == 110);
    REQUIRE(br.y == 70);
}

TEST_CASE("Box: empty()", "[box]") {
    SECTION("Zero width") {
        Box b(0, 0, 0, 10);
        REQUIRE(b.empty());
    }

    SECTION("Zero height") {
        Box b(0, 0, 10, 0);
        REQUIRE(b.empty());
    }

    SECTION("Non-empty") {
        Box b(0, 0, 10, 10);
        REQUIRE_FALSE(b.empty());
    }
}

TEST_CASE("Box: area()", "[box]") {
    Box b(0, 0, 10, 20);
    REQUIRE(b.area() == 200);
}

TEST_CASE("Box: contains()", "[box]") {
    Box b(10, 10, 100, 100);

    SECTION("Point inside") {
        REQUIRE(b.contains(Point(50, 50)));
    }

    SECTION("Point on top-left corner") {
        REQUIRE(b.contains(Point(10, 10)));
    }

    SECTION("Point on bottom-right edge is excluded") {
        REQUIRE_FALSE(b.contains(Point(110, 110)));
    }

    SECTION("Point outside") {
        REQUIRE_FALSE(b.contains(Point(0, 0)));
    }
}

TEST_CASE("Box: operator== and operator!=", "[box]") {
    Box a(1, 2, 3, 4);
    Box b(1, 2, 3, 4);
    Box c(5, 6, 7, 8);

    REQUIRE(a == b);
    REQUIRE(a != c);
}

TEST_CASE("Box: translation with Point", "[box]") {
    Box b(10, 20, 100, 50);
    Point offset(5, 10);

    SECTION("operator+") {
        auto moved = b + offset;
        REQUIRE(moved.x == 15);
        REQUIRE(moved.y == 30);
        REQUIRE(moved.width == 100);
        REQUIRE(moved.height == 50);
    }

    SECTION("operator-") {
        auto moved = b - offset;
        REQUIRE(moved.x == 5);
        REQUIRE(moved.y == 10);
    }

    SECTION("operator+=") {
        b += offset;
        REQUIRE(b.x == 15);
        REQUIRE(b.y == 30);
    }

    SECTION("operator-=") {
        b -= offset;
        REQUIRE(b.x == 5);
        REQUIRE(b.y == 10);
    }
}

TEST_CASE("Box: intersection (operator&)", "[box]") {
    Box a(0, 0, 100, 100);
    Box b(50, 50, 100, 100);

    SECTION("Overlapping boxes") {
        auto inter = a & b;
        REQUIRE(inter.x == 50);
        REQUIRE(inter.y == 50);
        REQUIRE(inter.width == 50);
        REQUIRE(inter.height == 50);
    }

    SECTION("Non-overlapping boxes") {
        Box c(200, 200, 50, 50);
        auto inter = a & c;
        REQUIRE(inter.empty());
    }
}

TEST_CASE("Box: union (operator|)", "[box]") {
    Box a(10, 10, 50, 50);
    Box b(30, 30, 50, 50);

    SECTION("Overlapping union") {
        auto u = a | b;
        REQUIRE(u.x == 10);
        REQUIRE(u.y == 10);
        REQUIRE(u.width == 70);
        REQUIRE(u.height == 70);
    }

    SECTION("Union with empty box returns other") {
        Box empty;
        auto u = empty | a;
        REQUIRE(u == a);
    }
}
