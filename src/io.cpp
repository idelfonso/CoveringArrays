#include "covarray/io.hpp"

#include <covarray/array.hpp>

#include <charconv>
#include <deque>
#include <format>
#include <fstream>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

namespace covarray {

namespace {

// Strips comments from one physical line, per the rules in io.hpp: a 'c'/'C'
// prefix comment discards the whole line, otherwise '#' discards everything
// from its first occurrence onward.
[[nodiscard]] std::string strip_comment(std::string_view line) {
    std::size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
        ++i;
    }

    if (i < line.size() && (line[i] == 'c' || line[i] == 'C')) {
        const bool at_end = (i + 1 == line.size());
        const bool followed_by_blank = !at_end && (line[i + 1] == ' ' || line[i + 1] == '\t');
        if (at_end || followed_by_blank) {
            return {};
        }
    }

    const std::size_t hash = line.find('#');
    return std::string(hash == std::string_view::npos ? line : line.substr(0, hash));
}

// Flattens the stream into whitespace-separated tokens, skipping comment and
// blank lines, while tracking the physical line number for error messages.
class token_reader {
   public:
    explicit token_reader(std::istream& in) : in_(in) {}

    // Returns the next token, or throws std::runtime_error naming `expected` if
    // the stream ends first.
    [[nodiscard]] std::string next(std::string_view expected) {
        while (pending_.empty()) {
            if (!refill()) {
                throw std::runtime_error(
                    std::format("covarray: unexpected end of input, expected {}", expected));
            }
        }
        std::string token = std::move(pending_.front());
        pending_.pop_front();
        return token;
    }

    [[nodiscard]] std::size_t line() const noexcept { return line_no_; }

   private:
    [[nodiscard]] bool refill() {
        std::string raw_line;
        while (std::getline(in_, raw_line)) {
            ++line_no_;
            if (!raw_line.empty() && raw_line.back() == '\r') {
                raw_line.pop_back();
            }

            std::istringstream content(strip_comment(raw_line));
            std::string token;
            bool any = false;
            while (content >> token) {
                pending_.push_back(token);
                any = true;
            }
            if (any) {
                return true;
            }
        }
        return false;
    }

    std::istream& in_;
    std::deque<std::string> pending_;
    std::size_t line_no_ = 0;
};

[[nodiscard]] std::size_t parse_field(const std::string& token,
                                      std::string_view field,
                                      std::size_t line_no) {
    long long value = -1;
    const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
    if (ec != std::errc{} || ptr != token.data() + token.size() || value < 0) {
        throw std::runtime_error(std::format(
            "covarray: line {}: field '{}' is not a non-negative integer: '{}'", line_no, field, token));
    }
    return static_cast<std::size_t>(value);
}

}  // namespace

array read_array(std::istream& in) {
    token_reader tokens(in);

    const std::string n_token = tokens.next("N");
    const std::size_t n = parse_field(n_token, "N", tokens.line());

    const std::string k_token = tokens.next("k");
    const std::size_t k = parse_field(k_token, "k", tokens.line());

    const std::string vk_token = tokens.next("v^k");
    const std::size_t vk_line = tokens.line();
    const std::size_t caret = vk_token.find('^');
    if (caret == std::string::npos) {
        throw std::runtime_error(std::format(
            "covarray: line {}: header field 3 must be 'v^k', got '{}'", vk_line, vk_token));
    }
    const std::size_t v = parse_field(vk_token.substr(0, caret), "v", vk_line);
    const std::size_t exponent =
        parse_field(vk_token.substr(caret + 1), "exponent of v^k", vk_line);
    if (exponent != k) {
        throw std::runtime_error(std::format(
            "covarray: line {}: exponent in 'v^k' ({}) does not match the column count k ({})",
            vk_line,
            exponent,
            k));
    }

    const std::string t_token = tokens.next("t");
    const std::size_t t = parse_field(t_token, "t", tokens.line());

    array result(n, k, v, t);
    for (std::size_t r = 0; r < n; ++r) {
        for (std::size_t c = 0; c < k; ++c) {
            const std::string token = tokens.next(std::format("matrix entry (row {}, column {})", r, c));
            const std::size_t entry_line = tokens.line();

            long long value = 0;
            const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
            if (ec != std::errc{} || ptr != token.data() + token.size()) {
                throw std::runtime_error(std::format(
                    "covarray: line {}: row {}, column {}: not an integer: '{}'", entry_line, r, c, token));
            }
            if (value < 0 || value > static_cast<long long>(v)) {
                throw std::invalid_argument(std::format(
                    "covarray: line {}: row {}, column {}: symbol {} is out of range [0, {}]",
                    entry_line,
                    r,
                    c,
                    value,
                    v));
            }
            result(r, c) = static_cast<symbol_type>(value);
        }
    }
    return result;
}

array read_array(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error(std::format("covarray: cannot open '{}' for reading", path.string()));
    }
    return read_array(in);
}

void write_array(std::ostream& out, const array& a) {
    out << a.rows() << ' ' << a.columns() << ' ' << a.order() << '^' << a.columns() << ' '
        << a.strength() << '\n';
    for (std::size_t r = 0; r < a.rows(); ++r) {
        const auto row = a.row(r);
        for (std::size_t c = 0; c < row.size(); ++c) {
            if (c != 0) {
                out << ' ';
            }
            out << static_cast<unsigned>(row[c]);
        }
        out << '\n';
    }
}

void write_array(const std::filesystem::path& path, const array& a) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error(std::format("covarray: cannot open '{}' for writing", path.string()));
    }
    write_array(out, a);
}

}  // namespace covarray
