#include <covarray/array.hpp>
#include <covarray/canonical.hpp>

#include <gtest/gtest.h>

namespace {

covarray::array make_array(std::size_t v,
                            std::size_t t,
                            const std::vector<std::vector<covarray::symbol_type>>& rows) {
    const std::size_t n = rows.size();
    const std::size_t k = rows.empty() ? 0 : rows.front().size();
    covarray::array a(n, k, v, t);
    for (std::size_t r = 0; r < n; ++r) {
        for (std::size_t c = 0; c < k; ++c) {
            a(r, c) = rows[r][c];
        }
    }
    return a;
}

}  // namespace

TEST(canonical_form, RowPermutationDoesNotChangeIt) {
    const covarray::array a = make_array(2, 1, {{0, 1}, {1, 0}});
    const covarray::array b = make_array(2, 1, {{1, 0}, {0, 1}});  // rows swapped

    EXPECT_EQ(covarray::canonical_form(a), covarray::canonical_form(b));
    EXPECT_TRUE(covarray::isomorphic(a, b));
}

TEST(canonical_form, ColumnPermutationDoesNotChangeIt) {
    const covarray::array a = make_array(2, 1, {{0, 1, 1}, {1, 0, 0}});
    const covarray::array b = make_array(2, 1, {{1, 1, 0}, {0, 0, 1}});  // columns reordered

    EXPECT_EQ(covarray::canonical_form(a), covarray::canonical_form(b));
    EXPECT_TRUE(covarray::isomorphic(a, b));
}

TEST(canonical_form, SymbolRelabelingDoesNotChangeIt) {
    const covarray::array a = make_array(3, 1, {{0, 1, 2}, {1, 2, 0}, {2, 0, 1}});
    // Relabel 0 -> 2, 1 -> 0, 2 -> 1 throughout.
    const covarray::array b = make_array(3, 1, {{2, 0, 1}, {0, 1, 2}, {1, 2, 0}});

    EXPECT_EQ(covarray::canonical_form(a), covarray::canonical_form(b));
    EXPECT_TRUE(covarray::isomorphic(a, b));
}

TEST(canonical_form, CombinedPermutationsDoNotChangeIt) {
    const covarray::array a = make_array(2, 2, {{0, 0, 0}, {0, 1, 1}, {1, 0, 1}, {1, 1, 0}});
    // Reverse the rows, swap columns 0 and 2, and flip symbols 0 <-> 1.
    const covarray::array b = make_array(2, 2, {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 1}});

    EXPECT_EQ(covarray::canonical_form(a), covarray::canonical_form(b));
    EXPECT_TRUE(covarray::isomorphic(a, b));
}

TEST(canonical_form, StructurallyDifferentArraysAreNotIsomorphic) {
    // With a single column, row permutation plus relabeling its symbols
    // can turn any arrangement into any other with the same multiset of
    // per-symbol counts, and only those. `a` has three 0s and one 1; `c`
    // has two of each -- different count multisets, so these cannot be
    // isomorphic.
    const covarray::array a = make_array(2, 1, {{0}, {0}, {0}, {1}});
    const covarray::array c = make_array(2, 1, {{0}, {0}, {1}, {1}});

    EXPECT_NE(covarray::canonical_form(a), covarray::canonical_form(c));
    EXPECT_FALSE(covarray::isomorphic(a, c));
}

TEST(canonical_form, WildcardIsNeverRelabeled) {
    const covarray::symbol_type wildcard = 2;  // v == 2
    const covarray::array a = make_array(2, 1, {{wildcard, 0}, {1, wildcard}});
    // Relabel the real symbols 0 <-> 1; the wildcard entries must stay put.
    const covarray::array b = make_array(2, 1, {{wildcard, 1}, {0, wildcard}});

    EXPECT_EQ(covarray::canonical_form(a), covarray::canonical_form(b));
    EXPECT_TRUE(covarray::isomorphic(a, b));
}

TEST(canonical_form, EmptyArrayHasAnEmptyCanonicalForm) {
    const covarray::array a;  // 0 x 0
    EXPECT_TRUE(covarray::canonical_form(a).empty());
}
