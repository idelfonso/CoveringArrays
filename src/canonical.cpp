#include "covarray/canonical.hpp"

#include <covarray/array.hpp>

#include <algorithm>
#include <compare>
#include <utility>
#include <vector>

namespace covarray {

namespace {

// v! exactly, as an int. create_permutations()'s table is v! rows, so
// this is only ever called with v small enough for that to fit in memory
// -- the same factorial-time ceiling the rest of this file documents.
int factorial(int v) {
    int result = 1;
    for (int i = 2; i <= v; ++i) {
        result *= i;
    }
    return result;
}

/**
 * Scratch memory needed by is_canonical() and the helper functions it
 * calls (check_next_column, sort_rows, merge). Allocated once per
 * is_canonical()/canonicalize_by_column_search() call, so they are
 * self-contained with respect to auxiliary memory -- including the
 * v-symbol permutation table (fv, T), which used to be a pair of global
 * variables shared (non-reentrantly) across calls.
 *
 * X and Y are used to reorder rows by swapping row pointers -- an O(1)
 * operation per row, independent of row width k. That's what lets
 * sort_rows()/merge() reorder rows cheaply. The pointers themselves point
 * into x_storage/y_storage, one contiguous allocation per matrix instead
 * of one per row, so every member here is RAII-owned (freed automatically,
 * including on an exception, with no matching free_aux_buffers() call
 * needed anymore).
 *
 * Because X/Y are self-referential (they point into this object's own
 * x_storage/y_storage), copying an AuxBuffers would copy x_storage/
 * y_storage into fresh buffers while X/Y's pointers kept pointing at the
 * original's -- silently wrong. Copying is therefore disabled; moving
 * stays safe, since std::vector's move constructor relocates ownership of
 * a buffer without relocating the buffer itself.
 *
 * T has no such requirement -- its rows are never swapped, only read
 * after create_permutations() fills them in -- so it stays a plain
 * vector of vectors rather than getting the same contiguous treatment.
 */
struct AuxBuffers {
    AuxBuffers() = default;
    AuxBuffers(const AuxBuffers&) = delete;
    AuxBuffers& operator=(const AuxBuffers&) = delete;
    AuxBuffers(AuxBuffers&&) = default;
    AuxBuffers& operator=(AuxBuffers&&) = default;

    std::vector<symbol_type> x_storage;          // contiguous backing for X's rows, N * k
    std::vector<symbol_type> y_storage;          // contiguous backing for Y's rows, N * k
    std::vector<symbol_type*> X;                 // N row pointers into x_storage: auxiliary copy of `a`
    std::vector<symbol_type*> Y;                 // N row pointers into y_storage: auxiliary working matrix
    std::vector<bool> used;                      // marks which columns are already checked
    std::vector<int> A;                          // undoes the relabeling of one column
    std::vector<int> aux_merge;                  // scratch for the merge function
    std::vector<symbol_type*> array_pointers;    // scratch for the merge function
    int fv = 0;                                  // v!, the row count of T
    std::vector<std::vector<int>> T;             // v-symbol permutation table, fv x v
};

/**
 * Creates and stores the v! symbol permutations in lexicographic order in
 * `aux.T`, assuming `aux.T` already points to an `aux.fv` x v matrix --
 * see allocate_aux_buffers(), which allocates both before calling this.
 */
void create_permutations(AuxBuffers& aux, int v) {
    std::vector<std::vector<int>>& T = aux.T;
    const int fv = aux.fv;

    for (int j = 0; j < v; j++) {
        T[0][static_cast<std::size_t>(j)] = j;
    }

    int r = 1;
    while (r < fv) {
        // generate the next permutation http://compprog.files.wordpress.com/2007/10/lexicoperm.c
        T[static_cast<std::size_t>(r)] = T[static_cast<std::size_t>(r - 1)];

        int i = v - 2;
        while ((i >= 0) && (T[static_cast<std::size_t>(r)][static_cast<std::size_t>(i)] >
                             T[static_cast<std::size_t>(r)][static_cast<std::size_t>(i + 1)])) {
            i--;
        }
        if (i < 0) break;

        int l = v - 1;
        while (T[static_cast<std::size_t>(r)][static_cast<std::size_t>(i)] >
               T[static_cast<std::size_t>(r)][static_cast<std::size_t>(l)]) {
            l--;
        }
        std::swap(T[static_cast<std::size_t>(r)][static_cast<std::size_t>(i)],
                  T[static_cast<std::size_t>(r)][static_cast<std::size_t>(l)]);

        l = 0;
        for (int j = i + 1; j < ((v + i) / 2) + 1; j++, l++) {
            std::swap(T[static_cast<std::size_t>(r)][static_cast<std::size_t>(j)],
                      T[static_cast<std::size_t>(r)][static_cast<std::size_t>(v - l - 1)]);
        }
        r++;
    }
}

// N, k, and v are never negative, so -- unlike create_permutations(),
// which needs a signed type for its backward permutation scan -- there's
// nothing here for std::size_t to lose. create_permutations() still
// takes v as int, so that one call is where this function's single
// boundary cast lives.
AuxBuffers allocate_aux_buffers(std::size_t n, std::size_t k, std::size_t v) {
    AuxBuffers aux;

    aux.x_storage.assign(n * k, symbol_type{0});
    aux.y_storage.assign(n * k, symbol_type{0});
    aux.X.resize(n);
    aux.Y.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        aux.X[i] = &aux.x_storage[i * k];
        aux.Y[i] = &aux.y_storage[i * k];
    }

    aux.used.resize(k);
    aux.A.resize(v);
    aux.aux_merge.resize(n);
    aux.array_pointers.resize(n);
    aux.fv = factorial(static_cast<int>(v));
    aux.T.assign(static_cast<std::size_t>(aux.fv), std::vector<int>(v));
    create_permutations(aux, static_cast<int>(v));
    return aux;
}

/**
 * Compares the first `ncols` columns of D against the corresponding
 * columns of `a`. As soon as one column of D differs from the respective
 * column of `a`, returns an ordering reflecting which one is smaller
 * there. Returns `std::strong_ordering::equal` if every compared column
 * matches.
 */
std::strong_ordering compare_columns(const std::vector<symbol_type*>& D, const array& a, std::size_t ncols) {
    const std::size_t n = a.rows();

    for (std::size_t j = 0; j < ncols; j++) {
        std::size_t i = 0;
        for (; i < n && D[i][j] == a(i, j); i++) {
        }
        if (i < n) {
            return D[i][j] < a(i, j) ? std::strong_ordering::less : std::strong_ordering::greater;
        }
    }

    return std::strong_ordering::equal;
}

/**
 * A merge sort over the N rows, called from check_next_column() on
 * essentially every branch of the search -- the hot path of the whole
 * algorithm.
 * Efficient because only pointers to rows are moved, not the rows.
 */
void merge(AuxBuffers& aux, int r, int left, int center, int right) {
    std::vector<symbol_type*>& X = aux.X;
    std::vector<symbol_type*>& Y = aux.Y;
    std::vector<int>& aux_merge = aux.aux_merge;
    std::vector<symbol_type*>& array_pointers = aux.array_pointers;

    int s = left;
    int i = left;
    int j = center + 1;
    while (i <= center && j <= right) {
        // compare rows i and j on their first r columns in one call,
        // instead of a hand-rolled scan to the first difference.
        const std::strong_ordering ordering = std::lexicographical_compare_three_way(
            Y[static_cast<std::size_t>(i)],
            Y[static_cast<std::size_t>(i)] + r,
            Y[static_cast<std::size_t>(j)],
            Y[static_cast<std::size_t>(j)] + r);
        if (ordering <= 0) {  // tie or row i smaller: take i first, preserving stability on a tie
            aux_merge[static_cast<std::size_t>(s++)] = i++;
        } else {
            aux_merge[static_cast<std::size_t>(s++)] = j++;
        }
    }
    while (i <= center) {
        aux_merge[static_cast<std::size_t>(s++)] = i++;
    }
    while (j <= right) {
        aux_merge[static_cast<std::size_t>(s++)] = j++;
    }

    for (int idx = left; idx <= right; idx++) {
        array_pointers[static_cast<std::size_t>(idx)] = Y[static_cast<std::size_t>(aux_merge[static_cast<std::size_t>(idx)])];
    }
    for (int idx = left; idx <= right; idx++) {
        Y[static_cast<std::size_t>(idx)] = array_pointers[static_cast<std::size_t>(idx)];
    }
    for (int idx = left; idx <= right; idx++) {
        array_pointers[static_cast<std::size_t>(idx)] = X[static_cast<std::size_t>(aux_merge[static_cast<std::size_t>(idx)])];
    }
    for (int idx = left; idx <= right; idx++) {
        X[static_cast<std::size_t>(idx)] = array_pointers[static_cast<std::size_t>(idx)];
    }
}

/**
 * Sorts the rows of matrix Y ascending on their first `r` columns (the row
 * ordering found for Y is applied to X too).
 */
void sort_rows(AuxBuffers& aux, int r, int left, int right) {
    if (left < right) {
        const int center = (left + right) / 2;
        sort_rows(aux, r, left, center);
        sort_rows(aux, r, center + 1, right);
        merge(aux, r, left, center, right);
    }
}

/**
 * Recursive, early-exit canonicity check: tries every not-yet-used
 * original column as the next output column (`for p`), and -- for each
 * such choice -- every symbol relabeling of it (`for rel`, with `rel ==
 * 0` meaning the identity, handled before the loop), sorting rows and
 * comparing against `a` after each. Returns false the moment any
 * candidate's first `col + 1` columns come out lexicographically smaller
 * than `a`'s own -- proof that `a` is not its own canonical form -- and
 * only returns true once every column order and every relabeling has been
 * tried without ever beating `a`.
 */
bool check_next_column(const array& a, std::size_t k, std::size_t col, AuxBuffers& aux) {
    std::vector<symbol_type*>& X = aux.X;
    std::vector<symbol_type*>& Y = aux.Y;
    std::vector<bool>& used = aux.used;
    std::vector<int>& A = aux.A;
    std::vector<std::vector<int>>& T = aux.T;
    const int fv = aux.fv;
    const std::size_t N = a.rows();
    const std::size_t v = a.order();
    const symbol_type wildcard = a.wildcard();

    // end of the recursive calls
    if (col == k) return true;

    // sort_rows()/merge() take signed row/column bounds -- see their own
    // documentation for why (the N == 0 case needs "the last row index"
    // to be representable as -1) -- so the one cast each needs happens
    // once here, rather than on every call below.
    const int next_col = static_cast<int>(col) + 1;
    const int last_row = static_cast<int>(N) - 1;

    for (std::size_t p = 0; p < k; p++) {
        if (used[p]) continue;
        used[p] = true;

        // copy column 'p' of X to column 'col' of Y
        for (std::size_t i = 0; i < N; i++) {
            Y[i][col] = X[i][p];
        }
        sort_rows(aux, next_col, 0, last_row);
        std::strong_ordering result_cmp = compare_columns(Y, a, col + 1);
        if (result_cmp == std::strong_ordering::less) return false;
        if (result_cmp == std::strong_ordering::equal) {
            if (!check_next_column(a, k, col + 1, aux)) return false;
        }

        for (int rel = 1; rel < fv; rel++) {
            // undo relabeling T[rel - 1] (back to the original symbols),
            // then apply T[rel] -- the wildcard is never relabeled, so it
            // passes through both steps unchanged.
            for (std::size_t i = 0; i < v; i++) {
                A[static_cast<std::size_t>(T[static_cast<std::size_t>(rel - 1)][i])] = static_cast<int>(i);
            }
            for (std::size_t i = 0; i < N; i++) {
                symbol_type& cell = Y[i][col];
                if (cell != wildcard) {
                    cell = static_cast<symbol_type>(A[cell]);
                }
            }
            for (std::size_t i = 0; i < N; i++) {
                symbol_type& cell = Y[i][col];
                if (cell != wildcard) {
                    cell = static_cast<symbol_type>(T[static_cast<std::size_t>(rel)][cell]);
                }
            }
            sort_rows(aux, next_col, 0, last_row);
            result_cmp = compare_columns(Y, a, col + 1);
            if (result_cmp == std::strong_ordering::less) return false;
            if (result_cmp == std::strong_ordering::equal) {
                if (!check_next_column(a, k, col + 1, aux)) return false;
            }
        }
        used[p] = false;
    }
    return true;
}

/**
 * Recursive, best-tracking column search: tries every not-yet-used
 * original column as the next output column (`for p`), and -- for each
 * such choice -- every symbol relabeling of it (`for rel`, with `rel ==
 * 0` meaning the identity, handled before the loop), sorting rows and
 * comparing against the running incumbent `best` after each. Whenever a
 * candidate's first `col + 1` columns come out lexicographically smaller
 * than `best`'s own, the remaining (not yet placed) columns are filled in
 * arbitrarily from the still-unused originals so `best` stays a complete,
 * valid array, and the search recurses deeper to keep refining it; an
 * EQUAL prefix also recurses deeper, to break the tie further along;
 * a GREATER prefix can never beat `best` and that branch is abandoned.
 * Unlike check_next_column(), this never short-circuits the whole search
 * -- it is building the canonical form itself, not just confirming `a`
 * already is one -- so every column order and relabeling combination
 * that isn't provably worse than the current incumbent gets explored.
 */
void check_next_column_2(std::size_t k, std::size_t col, AuxBuffers& aux, array& best) {
    std::vector<symbol_type*>& X = aux.X;
    std::vector<symbol_type*>& Y = aux.Y;
    std::vector<bool>& used = aux.used;
    std::vector<int>& A = aux.A;
    std::vector<std::vector<int>>& T = aux.T;
    const int fv = aux.fv;
    const std::size_t N = best.rows();
    const std::size_t v = best.order();
    const symbol_type wildcard = best.wildcard();

    // end of the recursive calls
    if (col == k) return;

    // see check_next_column()'s identical comment: sort_rows()/merge()
    // need a signed "last row index" so N == 0 can represent it as -1.
    const int next_col = static_cast<int>(col) + 1;
    const int last_row = static_cast<int>(N) - 1;

    for (std::size_t p = 0; p < k; p++) {
        if (used[p]) continue;
        used[p] = true;

        // copy column 'p' of X to column 'col' of Y
        for (std::size_t i = 0; i < N; i++) {
            Y[i][col] = X[i][p];
        }
        sort_rows(aux, next_col, 0, last_row);
        std::strong_ordering result_cmp = compare_columns(Y, best, col + 1);
        if (result_cmp == std::strong_ordering::less) {
            // copy the still-unused columns of X into the remaining,
            // not-yet-decided columns of Y so it is a complete candidate
            // -- the recursive call right after will keep refining them.
            for (std::size_t j = 0, l = col + 1; j < k; j++) {
                if (!used[j]) {
                    for (std::size_t i = 0; i < N; i++) {
                        Y[i][l] = X[i][j];
                    }
                    l++;
                }
            }
            for (std::size_t i = 0; i < N; i++) {
                for (std::size_t j = 0; j < k; j++) {
                    best(i, j) = Y[i][j];
                }
            }
            check_next_column_2(k, col + 1, aux, best);
        } else if (result_cmp == std::strong_ordering::equal) {
            check_next_column_2(k, col + 1, aux, best);
        }

        for (int rel = 1; rel < fv; rel++) {
            for (std::size_t i = 0; i < v; i++) {
                A[static_cast<std::size_t>(T[static_cast<std::size_t>(rel - 1)][i])] = static_cast<int>(i);
            }
            for (std::size_t i = 0; i < N; i++) {
                symbol_type& cell = Y[i][col];
                if (cell != wildcard) {
                    cell = static_cast<symbol_type>(A[cell]);
                }
            }
            for (std::size_t i = 0; i < N; i++) {
                symbol_type& cell = Y[i][col];
                if (cell != wildcard) {
                    cell = static_cast<symbol_type>(T[static_cast<std::size_t>(rel)][cell]);
                }
            }
            sort_rows(aux, next_col, 0, last_row);
            result_cmp = compare_columns(Y, best, col + 1);
            if (result_cmp == std::strong_ordering::less) {
                for (std::size_t j = 0, l = col + 1; j < k; j++) {
                    if (!used[j]) {
                        for (std::size_t i = 0; i < N; i++) {
                            Y[i][l] = X[i][j];
                        }
                        l++;
                    }
                }
                for (std::size_t i = 0; i < N; i++) {
                    for (std::size_t j = 0; j < k; j++) {
                        best(i, j) = Y[i][j];
                    }
                }
                check_next_column_2(k, col + 1, aux, best);
            } else if (result_cmp == std::strong_ordering::equal) {
                check_next_column_2(k, col + 1, aux, best);
            }
        }
        used[p] = false;
    }
}

// O(k! * v!^k): symbol permutation has no closed form independent of row
// order (see the header documentation), so column order and every
// per-column symbol map are searched together via check_next_column_2()'s
// branch-and-bound search, which also fixes row order as it goes (sort
// the rows decided so far ascending on every prefix) -- see
// check_next_column_2()'s own documentation for how the incumbent `best`
// is tracked and refined. Returns the canonical arrangement itself (same
// n, k, v, t as `a`), not a flattening of it -- canonical_form_column_major()
// does that flattening.
[[nodiscard]] array canonicalize_by_column_search(const array& a) {
    const std::size_t n = a.rows();
    const std::size_t k = a.columns();
    const std::size_t v = a.order();

    array best = a;
    AuxBuffers aux = allocate_aux_buffers(n, k, v);
    for (std::size_t i = 0; i < n; i++) {
        for (std::size_t j = 0; j < k; j++) {
            aux.X[i][j] = a(i, j);
        }
    }
    check_next_column_2(k, 0, aux, best);

    return best;
}

}  // namespace

array canonicalize(const array& a) { return canonicalize_by_column_search(a); }

std::vector<symbol_type> canonical_form_column_major(const array& a) {
    const array canonical = canonicalize_by_column_search(a);
    const std::size_t n = canonical.rows();
    const std::size_t k = canonical.columns();

    std::vector<symbol_type> out;
    out.reserve(n * k);
    for (std::size_t j = 0; j < k; ++j) {
        for (std::size_t i = 0; i < n; ++i) {
            out.push_back(canonical(i, j));
        }
    }
    return out;
}

bool isomorphic(const array& a, const array& b) {
    return canonical_form_column_major(a) == canonical_form_column_major(b);
}

bool is_canonical(const array& a) {
    const std::size_t n = a.rows();
    const std::size_t k = a.columns();
    const std::size_t v = a.order();

    AuxBuffers aux = allocate_aux_buffers(n, k, v);
    for (std::size_t i = 0; i < n; i++) {
        for (std::size_t j = 0; j < k; j++) {
            aux.X[i][j] = a(i, j);
        }
    }
    return check_next_column(a, k, 0, aux);
}

}  // namespace covarray
