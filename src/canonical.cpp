#include "covarray/canonical.hpp"

#include <covarray/array.hpp>

#include <cstring>
#include <vector>

namespace covarray {

namespace {

enum class Comparison { SMALLER = -1, EQUAL = 0, GREATER = 1 };

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

// Allocates and returns a `rows` x `columns` matrix of `Elem`.
template <typename Elem>
Elem **get_matrix(int rows, int columns) {
    Elem **matrix = new Elem *[static_cast<std::size_t>(rows)];
    for (int i = 0; i < rows; i++) {
        matrix[i] = new Elem[static_cast<std::size_t>(columns)];
    }
    return matrix;
}

// Frees a matrix allocated by get_matrix() and nulls out *matrix.
template <typename Elem>
void free_matrix(Elem ***matrix, int rows) {
    Elem **m = *matrix;
    if (m != nullptr) {
        for (int i = 0; i < rows; i++) {
            delete[] m[i];
        }
        delete[] m;
        *matrix = nullptr;
    }
}

/**
 * Scratch memory needed by is_canonical() and the helper functions it
 * calls (check_next_column, sort_rows, merge). Allocated and freed once
 * per is_canonical() call, so they are self-contained with respect to
 * auxiliary memory -- including the v-symbol permutation table (fv, T),
 * which used to be a pair of global variables shared (non-reentrantly)
 * across calls.
 * X and Y are used to reorder rows by swapping row pointers -- an O(1)
 * operation per row, independent of row width k. That's what lets
 * sort_rows/merge reorder rows cheaply.
 */
struct AuxBuffers {
    symbol_type **X;                            // auxiliary copy of `a`
    symbol_type **Y;                            // auxiliary working matrix
    std::vector<bool> used;                     // marks which columns are already checked
    std::vector<int> A;                         // undoes the relabeling of one column
    std::vector<int> aux_merge;                 // scratch for the merge function
    std::vector<symbol_type *> array_pointers;  // scratch for the merge function
    int fv;                                     // v!, the row count of T
    int **T;                                    // v-symbol permutation table, fv x v
};

/**
 * Creates and stores the v! symbol permutations in lexicographic order in
 * `aux.T`, assuming `aux.T` already points to an `aux.fv` x v matrix --
 * see allocate_aux_buffers(), which allocates both before calling this.
 */
void create_permutations(AuxBuffers &aux, int v) {
    int **T = aux.T;
    const int fv = aux.fv;
    int i, j, tmp, l, r;

    for (j = 0; j < v; j++) T[0][j] = j;
    r = 1;
    while (r < fv) {
        // generate the next permutation http://compprog.files.wordpress.com/2007/10/lexicoperm.c
        std::memcpy(T[r], T[r - 1], static_cast<std::size_t>(v) * sizeof(int));
        i = v - 2;
        while ((i >= 0) && (T[r][i] > T[r][i + 1])) i--;
        if (i < 0) break;
        l = v - 1;
        while (T[r][i] > T[r][l]) l--;
        tmp = T[r][i];
        T[r][i] = T[r][l];
        T[r][l] = tmp;
        for (l = 0, j = i + 1; j < ((v + i) / 2) + 1; j++, l++) {
            tmp = T[r][j];
            T[r][j] = T[r][v - l - 1];
            T[r][v - l - 1] = tmp;
        }
        r++;
    }
}

AuxBuffers allocate_aux_buffers(int N, int k, int v) {
    AuxBuffers aux;
    aux.X = get_matrix<symbol_type>(N, k);
    aux.Y = get_matrix<symbol_type>(N, k);
    aux.used.resize(static_cast<std::size_t>(k));
    aux.A.resize(static_cast<std::size_t>(v));
    aux.aux_merge.resize(static_cast<std::size_t>(N));
    aux.array_pointers.resize(static_cast<std::size_t>(N));
    aux.fv = factorial(v);
    aux.T = get_matrix<int>(aux.fv, v);
    create_permutations(aux, v);
    return aux;
}

void free_aux_buffers(AuxBuffers &aux, int N) {
    free_matrix(&aux.X, N);
    free_matrix(&aux.Y, N);
    free_matrix(&aux.T, aux.fv);
}

/**
 * Compares the first `ncols` columns of D against the corresponding
 * columns of `a`. As soon as one column of D differs from the respective
 * column of `a`, returns SMALLER or GREATER according to which one is
 * smaller there. Returns EQUAL if every compared column matches.
 */
Comparison compare_columns(symbol_type **D, const array &a, int ncols) {
    const int n = static_cast<int>(a.rows());
    int i, j;

    for (j = 0; j < ncols; j++) {
        for (i = 0; i < n && D[i][j] == a(static_cast<std::size_t>(i), static_cast<std::size_t>(j)); i++) {
        }
        if (i < n) {
            return D[i][j] < a(static_cast<std::size_t>(i), static_cast<std::size_t>(j)) ? Comparison::SMALLER
                                                                                            : Comparison::GREATER;
        }
    }

    return Comparison::EQUAL;
}

/**
 * A merge sort over the N rows, called from check_next_column() on
 * essentially every branch of the search -- the hot path of the whole
 * algorithm.
 * Efficient because only pointers to rows are moved, not the rows.
 */
void merge(AuxBuffers &aux, int r, int left, int center, int right) {
    symbol_type **X = aux.X;
    symbol_type **Y = aux.Y;
    std::vector<int> &aux_merge = aux.aux_merge;
    std::vector<symbol_type *> &array_pointers = aux.array_pointers;
    int i, j, l, s;

    s = left;
    i = left;
    j = center + 1;
    while (i <= center && j <= right) {
        // compare rows i and j
        for (l = 0; l < r && Y[i][l] == Y[j][l]; l++) {
        }
        if (l == r) {
            aux_merge[static_cast<std::size_t>(s++)] = i++;
        } else {
            if (Y[i][l] < Y[j][l]) {
                aux_merge[static_cast<std::size_t>(s++)] = i++;
            } else {
                aux_merge[static_cast<std::size_t>(s++)] = j++;
            }
        }
    }
    while (i <= center) {
        aux_merge[static_cast<std::size_t>(s++)] = i++;
    }
    while (j <= right) {
        aux_merge[static_cast<std::size_t>(s++)] = j++;
    }

    for (i = left; i <= right; i++) array_pointers[static_cast<std::size_t>(i)] = Y[aux_merge[static_cast<std::size_t>(i)]];
    for (i = left; i <= right; i++) Y[i] = array_pointers[static_cast<std::size_t>(i)];
    for (i = left; i <= right; i++) array_pointers[static_cast<std::size_t>(i)] = X[aux_merge[static_cast<std::size_t>(i)]];
    for (i = left; i <= right; i++) X[i] = array_pointers[static_cast<std::size_t>(i)];
}

/**
 * Sorts the rows of matrix Y ascending on their first `r` columns (the row
 * ordering found for Y is applied to X too).
 */
void sort_rows(AuxBuffers &aux, int r, int left, int right) {
    int center;
    if (left < right) {
        center = (left + right) / 2;
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
bool check_next_column(const array &a, int k, int col, AuxBuffers &aux) {
    symbol_type **X = aux.X;
    symbol_type **Y = aux.Y;
    std::vector<bool> &used = aux.used;
    std::vector<int> &A = aux.A;
    int **T = aux.T;
    const int fv = aux.fv;
    int i, p, rel;
    Comparison result_cmp;
    const int N = static_cast<int>(a.rows());
    const int v = static_cast<int>(a.order());
    const symbol_type wildcard = a.wildcard();

    // end of the recursive calls
    if (col == k) return true;

    for (p = 0; p < k; p++) {
        if (used[static_cast<std::size_t>(p)]) continue;
        used[static_cast<std::size_t>(p)] = true;
        // copy column 'p' of X to column 'col' of Y
        for (i = 0; i < N; i++) Y[i][col] = X[i][p];
        sort_rows(aux, col + 1, 0, N - 1);
        result_cmp = compare_columns(Y, a, col + 1);
        if (result_cmp == Comparison::SMALLER) return false;
        else if (result_cmp == Comparison::EQUAL) {
            if (!check_next_column(a, k, col + 1, aux)) return false;
        }
        for (rel = 1; rel < fv; rel++) {
            // undo relabeling T[rel - 1] (back to the original symbols),
            // then apply T[rel] -- the wildcard is never relabeled, so it
            // passes through both steps unchanged.
            for (i = 0; i < v; i++) A[static_cast<std::size_t>(T[rel - 1][i])] = i;
            for (i = 0; i < N; i++) {
                if (Y[i][col] != wildcard) {
                    Y[i][col] = static_cast<symbol_type>(A[Y[i][col]]);
                }
            }
            for (i = 0; i < N; i++) {
                if (Y[i][col] != wildcard) {
                    Y[i][col] = static_cast<symbol_type>(T[rel][Y[i][col]]);
                }
            }
            sort_rows(aux, col + 1, 0, N - 1);
            result_cmp = compare_columns(Y, a, col + 1);
            if (result_cmp == Comparison::SMALLER) return false;
            else if (result_cmp == Comparison::EQUAL) {
                if (!check_next_column(a, k, col + 1, aux)) return false;
            }
        }
        used[static_cast<std::size_t>(p)] = false;
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
void check_next_column_2(int k, int col, AuxBuffers &aux, array &best) {
    symbol_type **X = aux.X;
    symbol_type **Y = aux.Y;
    std::vector<bool> &used = aux.used;
    std::vector<int> &A = aux.A;
    int **T = aux.T;
    const int fv = aux.fv;
    int i, j, l, p, rel;
    Comparison result_cmp;
    const int N = static_cast<int>(best.rows());
    const int v = static_cast<int>(best.order());
    const symbol_type wildcard = best.wildcard();

    // end of the recursive calls
    if (col == k) return;

    for (p = 0; p < k; p++) {
        if (used[static_cast<std::size_t>(p)]) continue;
        used[static_cast<std::size_t>(p)] = true;
        // copy column 'p' of X to column 'col' of Y
        for (i = 0; i < N; i++) Y[i][col] = X[i][p];
        sort_rows(aux, col + 1, 0, N - 1);
        result_cmp = compare_columns(Y, best, col + 1);
        if (result_cmp == Comparison::SMALLER) {
            // copy the still-unused columns of X into the remaining,
            // not-yet-decided columns of Y so it is a complete candidate
            // -- the recursive call right after will keep refining them.
            for (j = 0, l = col + 1; j < k; j++) {
                if (!used[static_cast<std::size_t>(j)]) {
                    for (i = 0; i < N; i++) Y[i][l] = X[i][j];
                    l++;
                }
            }
            for (i = 0; i < N; i++) {
                for (j = 0; j < k; j++) {
                    best(static_cast<std::size_t>(i), static_cast<std::size_t>(j)) = Y[i][j];
                }
            }
            check_next_column_2(k, col + 1, aux, best);
        } else if (result_cmp == Comparison::EQUAL) {
            check_next_column_2(k, col + 1, aux, best);
        }
        for (rel = 1; rel < fv; rel++) {
            // undo relabeling T[rel - 1] (back to the original symbols),
            // then apply T[rel] -- the wildcard is never relabeled, so it
            // passes through both steps unchanged.
            for (i = 0; i < v; i++) A[static_cast<std::size_t>(T[rel - 1][i])] = i;
            for (i = 0; i < N; i++) {
                if (Y[i][col] != wildcard) {
                    Y[i][col] = static_cast<symbol_type>(A[Y[i][col]]);
                }
            }
            for (i = 0; i < N; i++) {
                if (Y[i][col] != wildcard) {
                    Y[i][col] = static_cast<symbol_type>(T[rel][Y[i][col]]);
                }
            }
            sort_rows(aux, col + 1, 0, N - 1);
            result_cmp = compare_columns(Y, best, col + 1);
            if (result_cmp == Comparison::SMALLER) {
                for (j = 0, l = col + 1; j < k; j++) {
                    if (!used[static_cast<std::size_t>(j)]) {
                        for (i = 0; i < N; i++) Y[i][l] = X[i][j];
                        l++;
                    }
                }
                for (i = 0; i < N; i++) {
                    for (j = 0; j < k; j++) {
                        best(static_cast<std::size_t>(i), static_cast<std::size_t>(j)) = Y[i][j];
                    }
                }
                check_next_column_2(k, col + 1, aux, best);
            } else if (result_cmp == Comparison::EQUAL) {
                check_next_column_2(k, col + 1, aux, best);
            }
        }
        used[static_cast<std::size_t>(p)] = false;
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
    const int n = static_cast<int>(a.rows());
    const int k = static_cast<int>(a.columns());
    const int v = static_cast<int>(a.order());

    array best = a;
    AuxBuffers aux = allocate_aux_buffers(n, k, v);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < k; j++) {
            aux.X[i][j] = a(static_cast<std::size_t>(i), static_cast<std::size_t>(j));
        }
    }
    check_next_column_2(k, 0, aux, best);
    free_aux_buffers(aux, n);

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
    const int n = static_cast<int>(a.rows());
    const int k = static_cast<int>(a.columns());
    const int v = static_cast<int>(a.order());

    AuxBuffers aux = allocate_aux_buffers(n, k, v);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < k; j++) {
            aux.X[i][j] = a(static_cast<std::size_t>(i), static_cast<std::size_t>(j));
        }
    }
    const bool result = check_next_column(a, k, 0, aux);
    free_aux_buffers(aux, n);
    return result;
}

}  // namespace covarray
