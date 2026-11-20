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
// form -- the lexicographically smallest arrangement reachable by any
// such permutation, read column-major (column 0's N entries, then
// column 1's, ...) -- and compare element-wise. Two arrays are
// isomorphic iff their canonical forms are equal.
//
// canonical_form_column_major() is exact, not a heuristic, but it is
// also genuinely factorial-time: O(k! * v!^k) -- see src/canonical.cpp.
// Symbol permutation is defined per column, not per row, so there is no
// way to fix the per-column symbol maps without a row order already in
// hand to make "first occurrence" meaningful -- and no row order is in
// hand yet, since finding one is the point. So column order and every
// per-column symbol map are searched together; only once both are fixed
// does row order get a closed form (sort the now fully-determined rows
// ascending).
//
// That cost has no N in it at all, so this scales in the number of rows
// for free -- exactly the shape this project's covering arrays actually
// have (far more rows than columns). It is still meant for the small,
// few-columns arrays an exhaustive equivalence-class enumeration
// produces, not for anything covarray::sa_construct() would build at
// scale in both dimensions.
#ifndef COVARRAY_CANONICAL_HPP
#define COVARRAY_CANONICAL_HPP

#include <covarray/array.hpp>

#include <vector>

namespace covarray {

// `a` itself, rearranged into its canonical form (as documented above):
// same n, k, v, t, but with rows, columns, and per-column symbols
// permuted into the lexicographically smallest arrangement. Unlike
// canonical_form_column_major(), this is a genuine covering array --
// pass it to covarray::write_array() to persist it in the project's
// standard file format.
[[nodiscard]] array canonicalize(const array& a);

// canonicalize(a)'s matrix, flattened column-major (column 0's N
// entries, then column 1's, ...) into a single sequence for comparison.
// Its length is always a.rows() * a.columns().
[[nodiscard]] std::vector<symbol_type> canonical_form_column_major(const array& a);

// True iff `a` and `b` are isomorphic -- equivalently, iff
// canonical_form_column_major(a) == canonical_form_column_major(b).
// Comparing canonical forms directly is cheaper when checking more than
// one pair against the same array, since each form only needs to be
// computed once.
[[nodiscard]] bool isomorphic(const array& a, const array& b);

// True iff `a` is already its own canonical form -- i.e. no permutation of
// rows, columns, or per-column symbols reaches anything lexicographically
// smaller than a's current arrangement. As expensive as
// canonical_form_column_major() itself when the given array is already
// canonical, less expensive than canonical_form_column_major() if the
// given array is not canonical.
[[nodiscard]] bool is_canonical(const array& a);

}  // namespace covarray

#endif  // COVARRAY_CANONICAL_HPP
