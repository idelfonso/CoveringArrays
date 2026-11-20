#include "covarray/array.hpp"

#include <format>

namespace covarray {

void check_dimensions([[maybe_unused]] std::size_t n, std::size_t k, std::size_t v, std::size_t t) {
    // N carries no constraint of its own; every value including 0 is legal.

    if (v < 1 || v > max_order) {
        throw std::invalid_argument(
            std::format("covarray::array: order v = {} is out of range [1, {}]", v, max_order));
    }

    if (k == 0) {
        if (t != 0) {
            throw std::invalid_argument("covarray::array: strength t must be 0 when k = 0");
        }
        return;
    }

    if (t > k) {
        throw std::invalid_argument(
            std::format("covarray::array: strength t = {} exceeds column count k = {}", t, k));
    }
}

array::array(std::size_t n, std::size_t k, std::size_t v, std::size_t t)
    : n_(n), k_(k), v_(v), t_(t), data_(n * k, symbol_type{0}) {
    check_dimensions(n_, k_, v_, t_);
}

symbol_type& array::at(std::size_t row, std::size_t col) {
    if (row < 0 || row >= n_ || col < 0 || col >= k_) {
        throw std::out_of_range(std::format(
            "covarray::array::at: index ({}, {}) out of range for a {} x {} array", row, col, n_, k_));
    }
    return data_[(row * k_) + col];
}

symbol_type array::at(std::size_t row, std::size_t col) const {
    return const_cast<array&>(*this).at(row, col);
}

std::optional<symbol_violation> array::find_invalid_symbol() const noexcept {
    const symbol_type limit = wildcard();  // valid symbols are 0..v inclusive.
    for (std::size_t r = 0; r < n_; ++r) {
        for (std::size_t c = 0; c < k_; ++c) {
            const symbol_type s = data_[(r * k_) + c];
            if (s > limit) {
                return symbol_violation{r, c, s};
            }
        }
    }
    return std::nullopt;
}

void array::validate_symbols() const {
    const auto violation = find_invalid_symbol();
    if (!violation) {
        return;
    }
    throw std::invalid_argument(std::format(
        "covarray::array: symbol {} at (row {}, column {}) is out of range [0, {}]",
        static_cast<unsigned>(violation->value),
        violation->row,
        violation->column,
        v_));
}

}  // namespace covarray
