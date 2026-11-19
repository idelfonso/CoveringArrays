#include "covarray/canonical.hpp"

#include <covarray/array.hpp>

#include <algorithm>
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

}  // namespace

std::vector<symbol_type> canonical_form(const array& a) {
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

    // Every row permutation is tried (see the file documentation for why
    // this is the only thing that needs to be searched). column_scratch
    // and columns are scratch space reused across every permutation so the
    // loop allocates only what std::next_permutation itself needs.
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

bool isomorphic(const array& a, const array& b) { return canonical_form(a) == canonical_form(b); }

}  // namespace covarray
