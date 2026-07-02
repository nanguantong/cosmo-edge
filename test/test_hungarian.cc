#include <vector>

#include "catch_amalgamated.hpp"
#include "nn/utils/tracker/hungarian.h"

using cosmo::nn::HungarianAlgorithm;

// Regression: an empty cost matrix must not trigger out-of-bounds/UB (P0).
// Covers the three empty shapes + one normal case to confirm no collateral damage.
TEST_CASE("HungarianAlgorithm::Solve tolerates empty cost matrices", "[nn][tracker][hungarian]") {
    HungarianAlgorithm algo;
    std::vector<int> assignment;

    SECTION("0x0 matrix") {
        std::vector<std::vector<double>> matrix;
        REQUIRE(algo.Solve(matrix, assignment) == 0.0);
        REQUIRE(assignment.empty());
    }

    SECTION("0xN matrix (no rows, e.g. utrkNum==0)") {
        std::vector<std::vector<double>> matrix;  // resize(0, vector<double>(N, 0))
        REQUIRE(algo.Solve(matrix, assignment) == 0.0);
        REQUIRE(assignment.empty());
    }

    SECTION("Mx0 matrix (rows but no cols, e.g. lowDetNum==0)") {
        const unsigned int rows = 3;
        std::vector<std::vector<double>> matrix(rows);  // 3 rows, each empty
        REQUIRE(algo.Solve(matrix, assignment) == 0.0);
        REQUIRE(assignment.size() == rows);
        for (int a : assignment) {
            REQUIRE(a == -1);
        }
    }

    SECTION("normal 2x2 still solves optimally") {
        std::vector<std::vector<double>> matrix = {{1.0, 2.0}, {2.0, 1.0}};
        REQUIRE(algo.Solve(matrix, assignment) == 2.0);
        REQUIRE(assignment.size() == 2);
    }
}
