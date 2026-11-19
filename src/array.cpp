#include "covarray/array.hpp"

#include <sstream>

namespace covarray {

void check_dimensions([[maybe_unused]] std::size_t n, std::size_t k, std::size_t v, std::size_t t) {
    // N carries no constraint of its own; every value including 0 is legal.

    if (v < 1 || v > max_order) {
        std::ostringstream msg;
        msg << "covarray::array: order v = " << v << " is out of range [1, " << max_order << "]";
        throw std::invalid_argument(msg.str());
    }

    if (k == 0) {
        if (t != 0) {
            throw std::invalid_argument("covarray::array: strength t must be 0 when k = 0");
        }
        return;
    }

    if (t > k) {
        std::ostringstream msg;
        msg << "covarray::array: strength t = " << t << " exceeds column count k = " << k;
        throw std::invalid_argument(msg.str());
    }
}

array::array(std::size_t n, std::size_t k, std::size_t v, std::size_t t)
    : n_(n), k_(k), v_(v), t_(t), data_(n * k, symbol_type{0}) {
    check_dimensions(n_, k_, v_, t_);
}

symbol_type& array::at(std::size_t row, std::size_t col) {
    if (row < 0 || row >= n_ || col < 0 || col >= k_) {
        std::ostringstream msg;
        msg << "covarray::array::at: index (" << row << ", " << col << ") out of range for a " << n_
            << " x " << k_ << " array";
        throw std::out_of_range(msg.str());
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
    std::ostringstream msg;
    msg << "covarray::array: symbol " << static_cast<unsigned>(violation->value) << " at (row "
        << violation->row << ", column " << violation->column << ") is out of range [0, " << v_
        << "]";
    throw std::invalid_argument(msg.str());
}

}  // namespace covarray
