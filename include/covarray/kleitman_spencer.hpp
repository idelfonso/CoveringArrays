// Exact, optimal construction of binary (v = 2) strength-2 covering
// arrays, after Kleitman and Spencer's 1973 result on families of
// pairwise "qualitatively independent" sets.
//
// For k >= 2 columns, the fewest rows any CA(N; 2, k, 2) can have is the
// smallest N with C(N - 1, floor(N / 2) - 1) >= k; conversely, for N >= 4
// rows, the most columns any CA(N; 2, k, 2) can have is exactly
// C(N - 1, floor(N / 2) - 1). (k < 2 and N < 4 are excluded because
// t = 2 needs k >= t = 2, and the smallest N any CA(N; 2, 2, 2) can have
// is 4 -- one row per combination of the two columns' two symbols, by a
// plain pigeonhole argument.)
//
// The construction achieving that bound, for either direction: set row 0
// (the "hub") to 1 in every column. Build each other column from a
// distinct size-(floor(N / 2) - 1) subset of the remaining N - 1 rows:
// 1 at the rows in that subset, 0 elsewhere. Any two distinct
// same-size subsets of equal size m satisfy, automatically:
//   - neither contains the other (equal size, distinct -- a subset
//     relation between them would force equality), giving both the
//     (1, 0) and (0, 1) patterns among those rows;
//   - their union has at most 2m elements, which is *always* strictly
//     fewer than the N - 1 non-hub rows for this m (2m < N - 1 holds for
//     every N -- see src/kleitman_spencer.cpp), giving the (0, 0)
//     pattern too;
//   - row 0 gives the (1, 1) pattern unconditionally, for every pair of
//     columns.
// So no further restriction on which subsets to pick is needed (unlike a
// naive attempt that fixes m = floor((N - 1) / 2) instead, which can pick
// a complementary pair of subsets and lose the (0, 0) pattern -- m here
// is deliberately one smaller than that naive guess). Taking every
// C(N - 1, m) such subset gives the maximum-columns construction; taking
// any k <= C(N - 1, m) of them gives a CA(N; 2, k, 2) with exactly k
// columns, including the fewest-rows one.
//
// This covers exactly the single point (t, v) = (2, 2) of the parameter
// space -- see covarray::sa_construct() (covarray/construct.hpp) for a
// general-purpose (but not provably optimal) constructor.
#ifndef COVARRAY_KLEITMAN_SPENCER_HPP
#define COVARRAY_KLEITMAN_SPENCER_HPP

#include <covarray/array.hpp>

#include <cstddef>

namespace covarray {

// The fewest rows any CA(N; 2, k, 2) can have, for this many columns.
// Throws std::invalid_argument if k < 2 (t = 2 requires k >= 2).
[[nodiscard]] std::size_t kleitman_spencer_optimal_rows(std::size_t k);

// Builds a CA(N; 2, k, 2) with N == kleitman_spencer_optimal_rows(k) --
// the fewest rows possible for this many columns.
// Throws std::invalid_argument under the same condition as that function.
[[nodiscard]] array kleitman_spencer_construct(std::size_t k);

// Builds a CA(N; 2, k, 2) with the most columns any strength-2, order-2
// covering array can have in n rows.
// Throws std::invalid_argument if n < 4 (no CA(n; 2, k, 2) with k >= 2
// exists below that -- see kleitman_spencer_optimal_rows(2)).
// Throws std::overflow_error if that column count does not fit in
// std::size_t (astronomically unlikely short of a very large n).
[[nodiscard]] array kleitman_spencer_construct_max_columns(std::size_t n);

}  // namespace covarray

#endif  // COVARRAY_KLEITMAN_SPENCER_HPP
