#include "covarray/coverage.hpp"

#include <covarray/array.hpp>

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace covarray {

namespace {

// idx = values[0] * v^(t-1) + values[1] * v^(t-2) + ... + values[t-1].
[[nodiscard]] std::size_t encode_tuple(const std::vector<symbol_type>& values,
                                       std::size_t v) noexcept {
    std::size_t idx = 0;
    for (const symbol_type value : values) {
        idx = (idx * v) + value;
    }
    return idx;
}

// Inverse of encode_tuple.
[[nodiscard]] tuple_values decode_tuple(std::size_t idx, std::size_t t, std::size_t v) {
    tuple_values values(t);
    for (std::size_t i = t; i-- > 0;) {
        values[i] = static_cast<symbol_type>(idx % v);
        idx /= v;
    }
    return values;
}

// Advances `combo` (t strictly increasing indices into [0, k)) to the next
// combination in lexicographic order; returns false once there is none.
[[nodiscard]] bool next_combination(std::vector<std::size_t>& combo, std::size_t k) {
    const std::size_t t = combo.size();
    std::size_t i = t;
    while (i > 0) {
        --i;
        if (combo[i] < k - t + i) {
            ++combo[i];
            for (std::size_t j = i + 1; j < t; ++j) {
                combo[j] = combo[j - 1] + 1;
            }
            return true;
        }
    }
    return false;
}

// Marks the tuple that row `row` covers for the column combination `combo`,
// given `a`'s columns pre-transposed into `columns_major` (column c occupies
// columns_major[c * n .. c * n + n)). A row with a wildcard in any of the
// chosen columns is ignored outright: it covers nothing for this
// combination. Only symbols in [0, v-1] ever cover a tuple.
//
// `tuple_scratch` (size t) is owned by the caller and reused across every
// row and every combination, so this hot loop allocates nothing.
void mark_row(const std::vector<std::size_t>& combo,
              std::size_t row,
              std::size_t n,
              const std::vector<symbol_type>& columns_major,
              symbol_type wildcard,
              std::size_t v,
              std::vector<symbol_type>& tuple_scratch,
              std::vector<bool>& covered,
              std::size_t& covered_count) {
    const std::size_t t = combo.size();
    for (std::size_t i = 0; i < t; ++i) {
        const symbol_type value = columns_major[(combo[i] * n) + row];
        if (value == wildcard) {
            return;
        }
        tuple_scratch[i] = value;
    }

    const std::size_t idx = encode_tuple(tuple_scratch, v);
    if (!covered[idx]) {
        covered[idx] = true;
        ++covered_count;
    }
}

[[nodiscard]] missing_combination make_missing_combination(const std::vector<std::size_t>& combo,
                                                           const std::vector<bool>& covered,
                                                           std::size_t v) {
    missing_combination result;
    result.columns = combo;
    const std::size_t t = combo.size();
    for (std::size_t idx = 0; idx < covered.size(); ++idx) {
        if (!covered[idx]) {
            result.tuples.push_back(decode_tuple(idx, t, v));
        }
    }
    return result;
}

}  // namespace

verify_result verify_fast(const array& a, const verify_options& options) {
    verify_result result;
    result.is_covering_array = true;

    const std::size_t k = a.columns();
    const std::size_t t = a.strength();
    const std::size_t v = a.order();
    const std::size_t n = a.rows();

    // t == 0 means "no columns are required to agree on anything": the one
    // trivial tuple is vacuously covered. k == 0 cannot occur with t > 0 (see
    // check_dimensions), so this also disposes of the only other degenerate
    // shape, a default-constructed array.
    if (k == 0 || t == 0) {
        return result;
    }

    std::size_t tuple_space = 1;
    for (std::size_t i = 0; i < t; ++i) {
        if (tuple_space > (std::numeric_limits<std::size_t>::max() / v)) {
            throw std::overflow_error("covarray::verify_fast: v^t overflows std::size_t");
        }
        tuple_space *= v;
    }

    // Column-major transpose, built once. The algorithm below re-reads every
    // column up to C(k - 1, t - 1) times (once per combination containing it);
    // columns_major keeps each one contiguous so that repeated reread is a
    // linear scan rather than a k-element stride through array's row-major
    // storage, which is what made this worth transposing up front.
    std::vector<symbol_type> columns_major(k * n);
    for (std::size_t r = 0; r < n; ++r) {
        for (std::size_t c = 0; c < k; ++c) {
            columns_major[(c * n) + r] = a(r, c);
        }
    }

    const symbol_type wildcard = a.wildcard();

    std::vector<std::size_t> combo(t);
    for (std::size_t i = 0; i < t; ++i) {
        combo[i] = i;
    }

    std::vector<bool> covered(tuple_space);
    std::vector<symbol_type> tuple_scratch(t);

    bool has_combo = true;
    while (has_combo) {
        std::fill(covered.begin(), covered.end(), false);
        std::size_t covered_count = 0;

        // The loop condition is the whole point: once covered_count reaches
        // tuple_space, every v^t tuple has been seen and the remaining rows have
        // nothing left to contribute.
        for (std::size_t r = 0; r < n && covered_count < tuple_space; ++r) {
            mark_row(
                combo, r, n, columns_major, wildcard, v, tuple_scratch, covered, covered_count);
        }

        ++result.combinations_checked;
        if (covered_count < tuple_space) {
            result.is_covering_array = false;
            ++result.combinations_with_missing;
            result.missing_tuples_total += tuple_space - covered_count;
            if (options.collect_missing) {
                result.missing.push_back(make_missing_combination(combo, covered, v));
            }
            if (!options.check_all_combinations) {
                break;
            }
        }

        has_combo = next_combination(combo, k);
    }

    return result;
}

std::string summarize(const verify_result& result) {
    std::ostringstream out;
    out << result.combinations_with_missing << " of " << result.combinations_checked
        << " column combination" << (result.combinations_checked == 1 ? "" : "s")
        << " missing at least one tuple, " << result.missing_tuples_total << " tuple"
        << (result.missing_tuples_total == 1 ? "" : "s") << " missing in total";
    return out.str();
}

std::string missing_report_filename(const array& a) {
    std::ostringstream name;
    name << 'N' << a.rows() << 'k' << a.columns() << 'v' << a.order() << '^' << a.columns() << 't'
         << a.strength() << ".missing";
    return name.str();
}

void write_missing(const std::filesystem::path& path, const array& a, const verify_result& result) {
    std::ofstream out(path);
    if (!out) {
        std::ostringstream msg;
        msg << "covarray: cannot open '" << path.string() << "' for writing";
        throw std::runtime_error(msg.str());
    }

    out << "# covarray missing-tuple report\n"
        << "# N=" << a.rows() << " k=" << a.columns() << " v=" << a.order() << " t=" << a.strength()
        << "\n"
        << "# columns are 0-indexed; symbols are 0-indexed (wildcard = "
        << static_cast<unsigned>(a.wildcard()) << ")\n"
        << "# one block per deficient column combination: a header line naming its\n"
        << "# columns and how many tuples it is missing, then one line per missing\n"
        << "# tuple listing its symbols in the same column order.\n";

    for (const missing_combination& combo : result.missing) {
        out << "columns";
        for (const std::size_t c : combo.columns) {
            out << ' ' << c;
        }
        out << " : " << combo.tuples.size() << " missing\n";
        for (const tuple_values& tuple : combo.tuples) {
            for (std::size_t i = 0; i < tuple.size(); ++i) {
                if (i != 0) {
                    out << ' ';
                }
                out << static_cast<unsigned>(tuple[i]);
            }
            out << '\n';
        }
    }

    out << "# total: " << result.combinations_with_missing << " of " << result.combinations_checked
        << " combinations missing " << result.missing_tuples_total << " tuples overall\n";
}

}  // namespace covarray
