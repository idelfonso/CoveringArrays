#include "covarray/kleitman_spencer.hpp"

#include <covarray/array.hpp>

#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace covarray {

namespace {

// unsigned __int128 is a GCC extension, not ISO C++ -- -Wpedantic flags
// using it at all, even though this project only ever builds with GCC
// (see CMakeLists.txt) and so can rely on it freely. Scoped to exactly
// the two functions below that need a type wide enough to multiply two
// std::size_t values without overflowing before the matching division
// (which is always exact -- see binomial_at_least()'s documentation)
// brings the result back down.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

// True iff C(n, r) >= threshold, computed incrementally (the standard
// C(n, i + 1) = C(n, i) * (n - i) / (i + 1) recurrence, always exact at
// every step) and stopped the moment that is established, without ever
// needing the full value of C(n, r) itself -- which, for the r this file
// calls it with (always <= n / 2), is reached by a strictly increasing
// sequence, so stopping early is safe. The __int128 intermediate has an
// unreachably large amount of headroom over any std::size_t threshold --
// this never risks the overflow exact_binomial_or_throw() below guards
// against.
[[nodiscard]] bool binomial_at_least(std::size_t n, std::size_t r, std::size_t threshold) {
    if (r > n) {
        return threshold == 0;
    }
    if (r > n - r) {
        r = n - r;
    }

    unsigned __int128 value = 1;
    for (std::size_t i = 0; i < r; ++i) {
        value = (value * (n - i)) / (i + 1);
        if (value >= threshold) {
            return true;
        }
    }
    return value >= threshold;
}

// The exact value of C(n, r).
// Throws std::overflow_error if it does not fit in std::size_t.
[[nodiscard]] std::size_t exact_binomial_or_throw(std::size_t n, std::size_t r) {
    if (r > n) {
        return 0;
    }
    if (r > n - r) {
        r = n - r;
    }

    unsigned __int128 value = 1;
    for (std::size_t i = 0; i < r; ++i) {
        value = (value * (n - i)) / (i + 1);
    }

    if (value > std::numeric_limits<std::size_t>::max()) {
        std::ostringstream msg;
        msg << "covarray::kleitman_spencer: C(" << n << ", " << r << ") overflows std::size_t";
        throw std::overflow_error(msg.str());
    }
    return static_cast<std::size_t>(value);
}

#pragma GCC diagnostic pop

// Advances `combo` (m strictly increasing indices into [0, universe_size))
// to the next combination in lexicographic order; returns false once
// there is none. Identical in spirit to the per-file combination-advance
// helpers already in coverage.cpp and construct.cpp.
[[nodiscard]] bool next_combination(std::vector<std::size_t>& combo, std::size_t universe_size) {
    const std::size_t m = combo.size();
    std::size_t i = m;
    while (i > 0) {
        --i;
        if (combo[i] < universe_size - m + i) {
            ++combo[i];
            for (std::size_t j = i + 1; j < m; ++j) {
                combo[j] = combo[j - 1] + 1;
            }
            return true;
        }
    }
    return false;
}

// Builds a CA(n; 2, k, 2) from the first k size-m subsets of the n - 1
// non-hub rows, in lexicographic order -- see the header documentation
// for why any distinct size-m subsets work, so "the first k" is as good
// a choice as any.
[[nodiscard]] array build_from_subsets(std::size_t n, std::size_t m, std::size_t k) {
    array result(n, k, 2, 2);

    for (std::size_t c = 0; c < k; ++c) {
        result(0, c) = 1;  // the hub row: 1 in every column.
    }

    std::vector<std::size_t> subset(m);
    std::iota(subset.begin(), subset.end(), std::size_t{0});

    for (std::size_t c = 0; c < k; ++c) {
        for (std::size_t r = 1; r < n; ++r) {
            result(r, c) = 0;
        }
        for (const std::size_t offset : subset) {
            result(1 + offset, c) = 1;  // offset is into the n - 1 non-hub rows.
        }
        if (c + 1 < k) {
            (void)next_combination(subset, n - 1);
        }
    }

    return result;
}

}  // namespace

std::size_t kleitman_spencer_optimal_rows(std::size_t k) {
    if (k < 2) {
        throw std::invalid_argument(
            "covarray::kleitman_spencer_optimal_rows: k must be >= 2 (strength 2 requires k >= 2)");
    }

    for (std::size_t n = 2;; ++n) {
        const std::size_t m = (n / 2) - 1;
        if (binomial_at_least(n - 1, m, k)) {
            return n;
        }
    }
}

array kleitman_spencer_construct(std::size_t k) {
    const std::size_t n = kleitman_spencer_optimal_rows(k);  // also validates k
    const std::size_t m = (n / 2) - 1;
    return build_from_subsets(n, m, k);
}

array kleitman_spencer_construct_max_columns(std::size_t n) {
    if (n < 4) {
        throw std::invalid_argument(
            "covarray::kleitman_spencer_construct_max_columns: n must be >= 4 (no CA(n; 2, k, 2) "
            "with k >= 2 exists below that)");
    }

    const std::size_t m = (n / 2) - 1;
    const std::size_t k = exact_binomial_or_throw(n - 1, m);
    return build_from_subsets(n, m, k);
}

}  // namespace covarray
