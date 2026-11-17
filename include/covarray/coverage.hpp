/// @file
/// @brief Checking that an array actually covers: the project's *fast*
/// verifier.
///
/// This is a vectorized coverage-mask checker meant for inner search loops.
///
/// ### Algorithm
///
/// verify_fast() checks every `C(k, t)` combination of columns covers all
/// `v^t` tuples, with two optimizations needed to make a large `C(k, t)`
/// tractable:
///   - **Column-major transpose.** covarray::array stores rows contiguously
///     (a project-wide convention), but this algorithm re-reads the same
///     column up to `C(k - 1, t - 1)` times — once per combination that
///     contains it. verify_fast() transposes into a column-major scratch
///     buffer once per call so each reread is a linear, prefetch-friendly
///     scan instead of a `k`-element stride through row-major storage.
///   - **Two early exits.** A combination's row scan stops the instant its
///     `v^t`-tuple coverage bitset is fully set — the remaining rows cannot
///     add anything. Unless `verify_options::check_all_combinations` is set,
///     the whole scan additionally stops at the first deficient combination
///     instead of examining the rest.
///
/// Both the column-major buffer and every per-row scratch buffer are
/// allocated once, outside the row and combination loops, so the hot path
/// allocates nothing.
///
/// ### Wildcards
///
/// array::wildcard() carries no coverage obligation of its own, which this
/// verifier reads as "matches any required symbol" — a row with a wildcard
/// in one of the `t` chosen columns can cover any tuple that agrees with it
/// on the other columns.
///
/// ### Two report shapes, by design
///
/// summarize() returns a one-line, location-free count (how many
/// combinations were deficient, how many tuples were missing overall);
/// write_missing() writes the full per-combination, per-tuple detail to a
/// file. Keeping them separate means a caller who only wants pass/fail or
/// the summary counts (e.g. `--all-columns` without `--write-missing`) never
/// pays to retain the full, potentially large, missing-tuple list — see
/// verify_options::collect_missing.
#ifndef COVARRAY_COVERAGE_HPP
#define COVARRAY_COVERAGE_HPP

#include <covarray/array.hpp>

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace covarray {

/// One missing t-tuple, in the column order of the missing_combination it
/// belongs to.
using tuple_values = std::vector<symbol_type>;

/// One `C(k, t)` column combination that does not cover every `v^t` tuple.
struct missing_combination {
  std::vector<std::size_t> columns;  ///< t strictly increasing 0-based column indices.
  std::vector<tuple_values> tuples;  ///< Every tuple this combination fails to cover.
};

/// Behavior switches for verify_fast(), one-to-one with the `covarray_cli
/// verify` command-line flags.
struct verify_options {
  /// `--all-columns`: `false` (the default) stops at the first deficient
  /// combination; `true` checks every `C(k, t)` combination and accumulates.
  bool check_all_combinations = false;

  /// Whether to record each deficient combination's columns and missing
  /// tuples into verify_result::missing. Off by default because, combined
  /// with check_all_combinations, the full list can be large — a caller
  /// that only wants pass/fail or the summary counts shouldn't pay to
  /// retain it.
  bool collect_missing = false;
};

/// The outcome of one verify_fast() call.
struct verify_result {
  bool is_covering_array = false;             ///< True iff every combination fully covers.
  std::size_t combinations_checked = 0;       ///< `C(k, t)` combinations actually examined.
  std::size_t combinations_with_missing = 0;  ///< Of those, how many were deficient.
  std::size_t missing_tuples_total = 0;       ///< Summed uncovered-tuple count.
  std::vector<missing_combination> missing;   ///< Populated iff options.collect_missing.
};

/// @brief Checks every `C(k, t)` combination of columns covers all `v^t`
/// tuples. See the file documentation above for the algorithm.
/// @param a       the array to check.
/// @param options see verify_options; defaults to halt-at-first-failure,
///                not collecting missing-tuple detail.
/// @return a verify_result summarizing the outcome; see verify_options for
///         how `options` shapes what it contains.
/// @throws std::overflow_error if `v^t` does not fit in `std::size_t`.
[[nodiscard]] verify_result verify_fast(const array& a, const verify_options& options = {});

/// @brief A one-line, location-free summary of `result`: how many of the
/// checked combinations were deficient, and how many tuples were missing in
/// total. Never names a column or a tuple — see write_missing() for that
/// detail.
[[nodiscard]] std::string summarize(const verify_result& result);

/// The canonical missing-tuple report filename for `a`:
/// `N<rows>k<columns>v<order>^<columns>t<strength>.missing`.
[[nodiscard]] std::string missing_report_filename(const array& a);

/// @brief Writes every combination in `result.missing` to `path`: one block
/// per deficient combination, naming its columns (0-indexed) and listing
/// every tuple it fails to cover.
/// @throws std::runtime_error if the file cannot be opened.
void write_missing(const std::filesystem::path& path, const array& a, const verify_result& result);

}  // namespace covarray

#endif  // COVARRAY_COVERAGE_HPP
