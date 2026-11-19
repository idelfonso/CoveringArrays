#include <covarray/array.hpp>
#include <covarray/coverage.hpp>
#include <covarray/kleitman_spencer.hpp>

#include <cstddef>
#include <stdexcept>

#include <gtest/gtest.h>

// Expected values below were independently verified (not just derived)
// against a brute-force maximum-clique search over the true pairwise
// compatibility graph for n up to 7, and the construction itself was
// checked by direct pairwise-coverage inspection for n up to 11, before
// being written into this project -- see the session history for the
// scripts used. These tests pin that same, independently-checked sequence.

TEST(KleitmanSpencer, OptimalRowsMatchesTheKnownSmallSequence) {
    EXPECT_EQ(covarray::kleitman_spencer_optimal_rows(2), 4u);
    EXPECT_EQ(covarray::kleitman_spencer_optimal_rows(3), 4u);
    EXPECT_EQ(covarray::kleitman_spencer_optimal_rows(4), 5u);
    EXPECT_EQ(covarray::kleitman_spencer_optimal_rows(5), 6u);
    EXPECT_EQ(covarray::kleitman_spencer_optimal_rows(6), 6u);
    EXPECT_EQ(covarray::kleitman_spencer_optimal_rows(10), 6u);
    EXPECT_EQ(covarray::kleitman_spencer_optimal_rows(11), 7u);
    EXPECT_EQ(covarray::kleitman_spencer_optimal_rows(15), 7u);
    EXPECT_EQ(covarray::kleitman_spencer_optimal_rows(16), 8u);
}

TEST(KleitmanSpencer, OptimalRowsRejectsKBelowTwo) {
    EXPECT_THROW((void)covarray::kleitman_spencer_optimal_rows(0), std::invalid_argument);
    EXPECT_THROW((void)covarray::kleitman_spencer_optimal_rows(1), std::invalid_argument);
}

TEST(KleitmanSpencer, ConstructBuildsAValidCoveringArrayWithExactlyKColumns) {
    for (const std::size_t k : {2u, 3u, 4u, 5u, 6u, 10u, 11u, 15u, 16u, 35u}) {
        const covarray::array a = covarray::kleitman_spencer_construct(k);

        EXPECT_EQ(a.rows(), covarray::kleitman_spencer_optimal_rows(k)) << "k = " << k;
        EXPECT_EQ(a.columns(), k) << "k = " << k;
        EXPECT_EQ(a.order(), 2u) << "k = " << k;
        EXPECT_EQ(a.strength(), 2u) << "k = " << k;

        const covarray::verify_result result = covarray::verify_fast(a);
        EXPECT_TRUE(result.is_covering_array) << "k = " << k;
    }
}

TEST(KleitmanSpencer, ConstructRejectsKBelowTwo) {
    EXPECT_THROW((void)covarray::kleitman_spencer_construct(0), std::invalid_argument);
    EXPECT_THROW((void)covarray::kleitman_spencer_construct(1), std::invalid_argument);
}

TEST(KleitmanSpencer, ConstructMaxColumnsMatchesTheKnownSmallSequence) {
    // The most columns a CA(n; 2, k, 2) can have, for n = 4..9.
    const std::size_t expected[] = {3, 4, 10, 15, 35, 56};
    for (std::size_t n = 4; n <= 9; ++n) {
        const covarray::array a = covarray::kleitman_spencer_construct_max_columns(n);
        EXPECT_EQ(a.columns(), expected[n - 4]) << "n = " << n;
    }
}

TEST(KleitmanSpencer, ConstructMaxColumnsBuildsAValidCoveringArrayWithNRows) {
    for (std::size_t n = 4; n <= 10; ++n) {
        const covarray::array a = covarray::kleitman_spencer_construct_max_columns(n);

        EXPECT_EQ(a.rows(), n) << "n = " << n;
        EXPECT_EQ(a.order(), 2u) << "n = " << n;
        EXPECT_EQ(a.strength(), 2u) << "n = " << n;

        const covarray::verify_result result = covarray::verify_fast(a);
        EXPECT_TRUE(result.is_covering_array) << "n = " << n;
    }
}

TEST(KleitmanSpencer, ConstructMaxColumnsRejectsNBelowFour) {
    EXPECT_THROW((void)covarray::kleitman_spencer_construct_max_columns(0), std::invalid_argument);
    EXPECT_THROW((void)covarray::kleitman_spencer_construct_max_columns(1), std::invalid_argument);
    EXPECT_THROW((void)covarray::kleitman_spencer_construct_max_columns(2), std::invalid_argument);
    EXPECT_THROW((void)covarray::kleitman_spencer_construct_max_columns(3), std::invalid_argument);
}

TEST(KleitmanSpencer, MaxColumnsAndOptimalRowsAreTightInverses) {
    // kleitman_spencer_construct_max_columns(n) finds the most columns
    // k that fit in n rows; feeding that k back into
    // kleitman_spencer_optimal_rows() should report that n is indeed the
    // fewest rows k needs -- i.e. n was not wasteful.
    for (std::size_t n = 4; n <= 10; ++n) {
        const std::size_t k = covarray::kleitman_spencer_construct_max_columns(n).columns();
        EXPECT_EQ(covarray::kleitman_spencer_optimal_rows(k), n) << "n = " << n;
    }
}
