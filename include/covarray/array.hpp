// The covering array type: an N x k matrix of symbols plus its four
// defining parameters (N, k, t, v).
//
// A covering array CA(N; t, k, v) is stored as a row-major N x k matrix of
// symbols in `{0, ..., v-1}`, plus the reserved wildcard symbol `v` for
// entries that carry no coverage obligation. This header is deliberately a
// pure data type: dimensions, storage and bounds-checked access, nothing
// else.
//   - Reading it from and writing it to the project's text format lives in
//     covarray::read_array() / covarray::write_array() (covarray/io.hpp).
//   - Checking that it actually covers every t-tuple is a separate
//     concern, covarray::verify_fast() (covarray/coverage.hpp) — an array
//     mid-repair by a heuristic is allowed to be momentarily non-covering,
//     so that check is never forced on construction or mutation here.
#ifndef COVARRAY_ARRAY_HPP
#define COVARRAY_ARRAY_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace covarray {

// One matrix entry. `v` is hard-capped at 255 project-wide, and the
// wildcard symbol is `v` itself, so `std::uint8_t` represents every legal
// value (0..255) with no wasted storage and no risk of overflow at the cap.
using symbol_type = std::uint8_t;

// Hard limit on the order (number of symbols per factor). Above this, the
// wildcard symbol (`v`) would not fit in symbol_type.
inline constexpr std::size_t max_order = 255;

// Location of a symbol outside `[0, v]`, as found by array::find_invalid_symbol().
struct symbol_violation {
    std::size_t row;     // 0-based row index of the offending entry.
    std::size_t column;  // 0-based column index of the offending entry.
    symbol_type value;   // The out-of-range value itself.
};

// Checks that `(n, k, v, t)` are self-consistent.
//
// `v` must be in `[1, max_order]`, and `t` must not exceed `k`.
// `N` carries no constraint of its own -- `N == 0` is a legal, empty array.
//
//   n - rows (unconstrained; accepted for signature symmetry with the
//       other three parameters).
//   k - columns.
//   v - order (number of symbols); must be in `[1, max_order]`.
//   t - strength; must satisfy `t <= k`.
// Throws std::invalid_argument naming the offending parameter on failure.
void check_dimensions(std::size_t n, std::size_t k, std::size_t v, std::size_t t);

// A covering array CA(N; t, k, v): an N x k matrix of symbols, plus
// the four parameters that define it.
//
// Entries are in `[0, v-1]`, or equal to `v` for a wildcard ("don't care",
// no coverage obligation).
//
// This type enforces its own shape invariants (dimensions self-consistent,
// symbols in range) but not the covering property itself — see the file
// documentation above.
class array {
   public:
    // Constructs an empty (0 x 0) array. Valid but not useful on its own;
    // typically immediately overwritten by covarray::read_array().
    array() noexcept = default;

    // Allocates an N x k matrix of the given order and strength, every
    // entry initialized to symbol 0.
    //   n - rows.
    //   k - columns.
    //   v - order (number of symbols).
    //   t - strength.
    // Throws std::invalid_argument if the dimensions are not self-consistent
    // (see check_dimensions()).
    array(std::size_t n, std::size_t k, std::size_t v, std::size_t t);

    [[nodiscard]] std::size_t rows() const noexcept { return n_; }  // N

    [[nodiscard]] std::size_t columns() const noexcept { return k_; }  // k

    [[nodiscard]] std::size_t order() const noexcept { return v_; }  // v

    [[nodiscard]] std::size_t strength() const noexcept { return t_; }  // t

    // The reserved "don't care" symbol: `v`, one past the last real symbol.
    [[nodiscard]] symbol_type wildcard() const noexcept { return static_cast<symbol_type>(v_); }

    // Whether `s` is the wildcard symbol for this array's order.
    [[nodiscard]] bool is_wildcard(symbol_type s) const noexcept { return s == wildcard(); }

    // True iff this array has zero rows.
    [[nodiscard]] bool empty() const noexcept { return n_ == 0; }

    // Unchecked element access for inner search/coverage loops.
    //
    // No bounds or range check. Intended for maximum performance.
    // Prefer at() wherever the indices, or the stored value, are
    // not already known-good (file I/O, tests).
    //   row - 0-based row index; undefined behavior if `row >= rows()`.
    //   col - 0-based column index; undefined behavior if `col >= columns()`.
    [[nodiscard]] symbol_type& operator()(std::size_t row, std::size_t col) noexcept {
        return data_[(row * k_) + col];
    }

    // Const overload of operator()(std::size_t, std::size_t).
    [[nodiscard]] symbol_type operator()(std::size_t row, std::size_t col) const noexcept {
        return data_[(row * k_) + col];
    }

    // Bounds-checked element access.
    //
    // Does not check that the stored symbol itself is in range — use
    // find_invalid_symbol() for that.
    // Throws std::out_of_range if `row >= rows()` or `col >= columns()`.
    [[nodiscard]] symbol_type& at(std::size_t row, std::size_t col);

    // Const overload of at(std::size_t, std::size_t).
    [[nodiscard]] symbol_type at(std::size_t row, std::size_t col) const;

    // Row `r` as a mutable, contiguous span of columns() symbols.
    [[nodiscard]] std::span<symbol_type> row(std::size_t r) noexcept {
        return std::span(data_).subspan(r * k_, k_);
    }

    // Const overload of row(std::size_t).
    [[nodiscard]] std::span<const symbol_type> row(std::size_t r) const noexcept {
        return std::span(data_).subspan(r * k_, k_);
    }

    // The whole matrix, row-major and contiguous — for vectorized coverage code.
    [[nodiscard]] std::span<symbol_type> data() noexcept { return data_; }

    // Const overload of data().
    [[nodiscard]] std::span<const symbol_type> data() const noexcept { return data_; }

    // Finds the first entry outside `[0, v]` in row-major order, if any.
    //
    // An array accepted by covarray::write_array() always has none.
    // Returns the location of the first out-of-range symbol, or
    // `std::nullopt` if every entry is valid.
    [[nodiscard]] std::optional<symbol_violation> find_invalid_symbol() const noexcept;

    // Validates every entry is in `[0, v]`.
    // Throws std::invalid_argument naming the row, column and value of the
    // first out-of-range symbol, if any; otherwise does nothing.
    void validate_symbols() const;

   private:
    std::size_t n_ = 0;
    std::size_t k_ = 0;
    std::size_t v_ = 0;
    std::size_t t_ = 0;
    std::vector<symbol_type> data_;
};

}  // namespace covarray

#endif  // COVARRAY_ARRAY_HPP
