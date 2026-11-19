#include "covarray/canonical.hpp"

#include <covarray/array.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace covarray {

namespace {

// The lexicographically smallest sequence reachable from `column` (one
// column's values, in some fixed row order) by relabeling the v real
// symbols with any bijection of [0, v). The wildcard passes through
// unchanged -- it is not one of the v real symbols, so it never
// participates in the relabeling.
//
// Assigning each distinct value the next unused label in the order it
// first appears is exactly the lex-smallest such relabeling: the value at
// the first position must become 0 to minimize that position, the first
// *new* value afterwards must become 1 to minimize its position without
// touching anything earlier, and so on -- using any other label, or the
// same label for two different values, can only make some position worse.
[[nodiscard]] std::vector<symbol_type> canonicalize_column(const std::vector<symbol_type>& column,
                                                            std::size_t v,
                                                            symbol_type wildcard) {
    std::vector<bool> assigned(v, false);
    std::vector<symbol_type> label_of(v, 0);
    symbol_type next_label = 0;

    std::vector<symbol_type> out(column.size());
    for (std::size_t i = 0; i < column.size(); ++i) {
        const symbol_type value = column[i];
        if (value == wildcard) {
            out[i] = wildcard;
            continue;
        }
        if (!assigned[value]) {
            assigned[value] = true;
            label_of[value] = next_label++;
        }
        out[i] = label_of[value];
    }
    return out;
}

// O(N!): for each row order, the optimal column order and per-column
// symbol relabeling both have a closed form (see canonicalize_column()
// and the sort below) -- see the header documentation for the full
// argument. Column-major flattening: column 0's N entries, then column
// 1's, and so on.
[[nodiscard]] std::vector<symbol_type> canonical_form_by_row_search(const array& a) {
    const std::size_t n = a.rows();
    const std::size_t k = a.columns();
    const std::size_t v = a.order();
    const symbol_type wildcard = a.wildcard();

    std::vector<std::size_t> row_order(n);
    std::iota(row_order.begin(), row_order.end(), std::size_t{0});

    std::vector<symbol_type> column_scratch(n);
    std::vector<std::vector<symbol_type>> columns(k);

    std::vector<symbol_type> best;
    bool have_best = false;

    // column_scratch and columns are scratch space reused across every
    // permutation so the loop allocates only what std::next_permutation
    // itself needs.
    do {
        for (std::size_t c = 0; c < k; ++c) {
            for (std::size_t i = 0; i < n; ++i) {
                column_scratch[i] = a(row_order[i], c);
            }
            columns[c] = canonicalize_column(column_scratch, v, wildcard);
        }

        // For this fixed row order, sorting the (already symbol-
        // canonicalized) columns ascending and concatenating them is the
        // lex-smallest column order: every column has the same length n,
        // so comparing concatenations of them reduces exactly to
        // comparing the columns themselves.
        std::sort(columns.begin(), columns.end());

        std::vector<symbol_type> candidate;
        candidate.reserve(n * k);
        for (const std::vector<symbol_type>& column : columns) {
            candidate.insert(candidate.end(), column.begin(), column.end());
        }

        if (!have_best || candidate < best) {
            best = std::move(candidate);
            have_best = true;
        }
    } while (std::next_permutation(row_order.begin(), row_order.end()));

    return best;
}

// Advances `digits` (each in [0, radix)) to the next value of a mixed-
// radix counter with that many digits, most-significant first; returns
// false (and resets every digit to 0) once it would wrap past the last
// one. An empty `digits` (radix with zero positions) always returns
// false, so a single all-digits-0 "combination" is still visited exactly
// once by the do/while loops below.
[[nodiscard]] bool advance_counter(std::vector<std::size_t>& digits, std::size_t radix) {
    for (std::size_t i = digits.size(); i-- > 0;) {
        if (++digits[i] < radix) {
            return true;
        }
        digits[i] = 0;
    }
    return false;
}

// O(k! * v!^k): symbol permutation has no closed form independent of row
// order (see the header documentation), so column order and every
// per-column symbol map are searched together; row order then has a
// closed form (sort the now fully-determined rows ascending -- every row
// has the same length k, so the same "equal-length blocks" argument
// row-search uses for columns applies here to rows). Row-major
// flattening: row 0's k entries, then row 1's, and so on -- a different
// convention from row-search's column-major one, which is fine, since
// canonical_form() never compares results from the two against each
// other (see its header documentation).
[[nodiscard]] std::vector<symbol_type> canonical_form_by_column_search(const array& a) {
    const std::size_t n = a.rows();
    const std::size_t k = a.columns();
    const std::size_t v = a.order();
    const symbol_type wildcard = a.wildcard();

    std::vector<std::size_t> col_order(k);
    std::iota(col_order.begin(), col_order.end(), std::size_t{0});

    // Every permutation of the v real symbols, generated once and reused
    // for every column and every column order tried below. Always has at
    // least one entry (the identity, possibly of length 0), even when
    // v <= 1, so the per-column choice below always has something to
    // index.
    std::vector<std::vector<symbol_type>> symbol_perms;
    {
        std::vector<symbol_type> perm(v);
        std::iota(perm.begin(), perm.end(), symbol_type{0});
        do {
            symbol_perms.push_back(perm);
        } while (std::next_permutation(perm.begin(), perm.end()));
    }
    const std::size_t perms_per_column = symbol_perms.size();

    std::vector<std::size_t> symbol_choice(k, 0);  // symbol_perms index, per output column position
    std::vector<std::vector<symbol_type>> rows(n, std::vector<symbol_type>(k));

    std::vector<symbol_type> best;
    bool have_best = false;

    do {
        std::fill(symbol_choice.begin(), symbol_choice.end(), 0);
        bool has_choice = true;
        while (has_choice) {
            for (std::size_t i = 0; i < n; ++i) {
                for (std::size_t pos = 0; pos < k; ++pos) {
                    const symbol_type value = a(i, col_order[pos]);
                    rows[i][pos] =
                        (value == wildcard) ? wildcard : symbol_perms[symbol_choice[pos]][value];
                }
            }

            // Content is now fully fixed (column order and every symbol
            // map chosen), so sorting the N rows ascending is the
            // lex-smallest row order.
            std::sort(rows.begin(), rows.end());

            std::vector<symbol_type> candidate;
            candidate.reserve(n * k);
            for (const std::vector<symbol_type>& row : rows) {
                candidate.insert(candidate.end(), row.begin(), row.end());
            }

            if (!have_best || candidate < best) {
                best = std::move(candidate);
                have_best = true;
            }

            has_choice = advance_counter(symbol_choice, perms_per_column);
        }
    } while (std::next_permutation(col_order.begin(), col_order.end()));

    return best;
}

[[nodiscard]] double log_factorial(std::size_t n) { return std::lgamma(static_cast<double>(n) + 1.0); }

// Whichever of row-search (O(N!)) or column-search (O(k! * v!^k)) has the
// smaller estimated cost, compared on a log scale so it never risks
// overflowing an actual factorial.
[[nodiscard]] bool column_search_is_cheaper(std::size_t n, std::size_t k, std::size_t v) {
    const double row_search_cost = log_factorial(n);
    const double column_search_cost = log_factorial(k) + (static_cast<double>(k) * log_factorial(v));
    return column_search_cost < row_search_cost;
}

}  // namespace

std::vector<symbol_type> canonical_form(const array& a) {
    if (column_search_is_cheaper(a.rows(), a.columns(), a.order())) {
        return canonical_form_by_column_search(a);
    }
    return canonical_form_by_row_search(a);
}

bool isomorphic(const array& a, const array& b) { return canonical_form(a) == canonical_form(b); }

}  // namespace covarray
