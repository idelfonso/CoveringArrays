// Canonical form of a covering array, for isomorph (equivalence-class)
// checking.
//
// Two covering arrays of the same shape (N, k, v) are considered
// isomorphic here iff one can be turned into the other by some
// combination of: permuting its rows, permuting its columns, and
// independently relabeling the v real symbols within each column (the
// wildcard symbol is never relabeled -- it carries no coverage meaning,
// so there is nothing to permute it with).
//
// The standard way to decide that: reduce each array to its canonical
// form -- the lexicographically smallest sequence reachable by any such
// permutation, read column-major (column 0's N entries, then column 1's,
// ...) -- and compare element-wise. Two arrays are isomorphic iff their
// canonical forms are equal.
//
// canonical_form() is exact, not a heuristic, but it is also genuinely
// factorial-time, via one of two dual algorithms (picked per call,
// whichever is cheaper for that array's shape -- see src/canonical.cpp):
//
//   - row search, O(N!): for a *fixed* row order, the optimal column
//     order (sort ascending -- every column has the same length, so
//     comparing concatenations of them reduces to comparing the columns
//     themselves) and the optimal per-column symbol relabeling (greedy
//     first-occurrence labeling) both have a closed form, so the only
//     thing actually searched is the row permutation.
//   - column search, O(k! * v!^k): symbol permutation is defined per
//     column, not per row, so unlike the row-search case there is no way
//     to fix the per-column symbol maps without a row order already in
//     hand to make "first occurrence" meaningful -- and no row order is
//     in hand yet, since finding one is the point. So column order and
//     every per-column symbol map are searched together; only once both
//     are fixed does row order get a closed form (sort the now
//     fully-determined rows ascending).
//
// Which one runs is chosen by comparing N! against k! * v!^k (so it is
// meant for whichever of N or k is small -- a covering array with far
// more rows than columns and few symbols, or the reverse, is the common
// case this picks up), never both. This is still meant for the small
// arrays an exhaustive equivalence-class enumeration produces, not for
// anything covarray::sa_construct() would build at scale in both
// dimensions.
#ifndef COVARRAY_CANONICAL_HPP
#define COVARRAY_CANONICAL_HPP

#include <covarray/array.hpp>

#include <vector>

namespace covarray {

// The canonical form of `a`, as documented above. Its length is always
// a.rows() * a.columns().
[[nodiscard]] std::vector<symbol_type> canonical_form(const array& a);

// True iff `a` and `b` are isomorphic -- equivalently, iff
// canonical_form(a) == canonical_form(b). Comparing canonical forms
// directly is cheaper when checking more than one pair against the same
// array, since each form only needs to be computed once.
[[nodiscard]] bool isomorphic(const array& a, const array& b);

}  // namespace covarray

#endif  // COVARRAY_CANONICAL_HPP
