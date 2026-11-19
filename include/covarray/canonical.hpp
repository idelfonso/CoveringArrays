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
// O(N!): for a *fixed* row order, the optimal column order (sort
// ascending -- every column has the same length, so comparing
// concatenations of them reduces to comparing the columns themselves)
// and the optimal per-column symbol relabeling (greedy first-occurrence
// labeling) both have a closed form, so the only thing actually searched
// is the row permutation -- see src/canonical.cpp for the argument. That
// is intentional: this is meant for the small (single- to
// low-double-digit N) arrays an exhaustive equivalence-class enumeration
// produces, not for anything covarray::sa_construct() would build.
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
