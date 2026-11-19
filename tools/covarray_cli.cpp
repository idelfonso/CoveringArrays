// `covarray_cli` — thin command-line front end over the library.
//
// Today this has four subcommands:
//
//   covarray_cli verify <file> [--all-columns] [--write-missing]
//   covarray_cli kleitman-spencer-rows <k>
//   covarray_cli kleitman-spencer-construct <k> <outfile>
//   covarray_cli kleitman-spencer-max-columns <n> <outfile>
//
// built on covarray::verify_fast() and covarray/kleitman_spencer.hpp
// respectively. `--help` and `--version` are a stable contract: both
// always succeed and print to stdout, independent of any other argument.
//
// `verify` checks that `<file>` (the project's `N k v^k t` text format, see
// covarray/io.hpp) is a covering array:
//   - By default it halts at the first `C(k, t)` column combination missing
//     a tuple and reports that combination's columns plus one example
//     missing tuple — a witness, never a bare failure.
//   - `--all-columns` checks every combination instead and prints the
//     location-free summarize() count.
//   - `--write-missing` writes every deficient combination found (just the
//     one, in the default mode) to its canonical
//     `covarray::missing_report_filename()`.
//
// One subtlety: `verify_options::collect_missing` is forced on whenever
// `--all-columns` is not given, even without `--write-missing` — the
// default mode holds at most one deficient combination, which is exactly
// what the witness message needs. Under `--all-columns` it is only turned
// on when `--write-missing` will actually consume it, since that list can
// be large.
//
// Exit status for verify: `0` if `<file>` is a covering array, `1` if it
// is not, `2` on a usage or parse error.
//
// The three `kleitman-spencer-*` subcommands are thin wrappers over
// covarray::kleitman_spencer_optimal_rows(), covarray::kleitman_spencer_
// construct(), and covarray::kleitman_spencer_construct_max_columns()
// respectively -- see include/covarray/kleitman_spencer.hpp for what
// they compute and why it is exact (not heuristic) for this one
// parameter point, strength 2 over 2 symbols. Exit status: `0` on
// success, `2` on a usage or parse error (including the k < 2 / n < 4
// domain errors those functions themselves throw on).
#include <covarray/array.hpp>
#include <covarray/config.hpp>
#include <covarray/coverage.hpp>
#include <covarray/io.hpp>
#include <covarray/kleitman_spencer.hpp>

#include <charconv>
#include <exception>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

void print_usage(std::ostream& out) {
    out << "covarray " << COVARRAY_VERSION_STRING << " -- construct and verify covering arrays\n\n"
        << "Usage:\n"
        << "  covarray_cli verify <file> [--all-columns] [--write-missing]\n"
        << "  covarray_cli kleitman-spencer-rows <k>\n"
        << "  covarray_cli kleitman-spencer-construct <k> <outfile>\n"
        << "  covarray_cli kleitman-spencer-max-columns <n> <outfile>\n"
        << "  covarray_cli --help\n"
        << "  covarray_cli --version\n\n"
        << "verify checks that <file> (the project's 'N k v^k t' text format) is a\n"
        << "covering array. Exit status: 0 if it is, 1 if it is not, 2 on a usage or\n"
        << "parse error.\n\n"
        << "  --all-columns    check every C(k, t) column combination instead of\n"
        << "                   halting at the first one missing a tuple.\n"
        << "  --write-missing  write every deficient combination found to\n"
        << "                   N<rows>k<columns>v<order>^<columns>t<strength>.missing\n\n"
        << "The three kleitman-spencer-* commands build exact, optimal CA(N; 2, k, 2)\n"
        << "covering arrays (strength 2, 2 symbols) -- see\n"
        << "include/covarray/kleitman_spencer.hpp. Exit status: 0 on success, 2 on a\n"
        << "usage or parse error.\n\n"
        << "  kleitman-spencer-rows <k>\n"
        << "      print the fewest rows any CA(N; 2, k, 2) can have.\n"
        << "  kleitman-spencer-construct <k> <outfile>\n"
        << "      write a CA(N; 2, k, 2) with that many rows.\n"
        << "  kleitman-spencer-max-columns <n> <outfile>\n"
        << "      write a CA(n; 2, k, 2) with the most columns any such array can have\n"
        << "      in n rows.\n";
}

// Parses `text` in full as a base-10 unsigned integer of type T; returns
// false (leaving `out` unspecified) on a partial, empty, or otherwise
// malformed parse, rather than silently accepting a numeric prefix.
template <typename T>
[[nodiscard]] bool parse_uint(std::string_view text, T& out) {
    const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), out);
    return parsed.ec == std::errc{} && parsed.ptr == (text.data() + text.size());
}

// Prints the columns and one example missing tuple from `combo` — the
// "uncovered tuple and witnessing columns" the project's verifiers are
// expected to report, never a bare failure.
void print_witness(std::ostream& out, const covarray::missing_combination& combo) {
    out << "  columns:";
    for (const std::size_t c : combo.columns) {
        out << ' ' << c;
    }
    out << " -- " << combo.tuples.size() << " tuple(s) not covered, e.g.:";
    for (const covarray::symbol_type s : combo.tuples.front()) {
        out << ' ' << static_cast<unsigned>(s);
    }
    out << '\n';
}

int run_verify(std::span<const std::string_view> args) {
    bool all_columns = false;
    bool write_missing = false;
    std::string_view path;
    bool has_path = false;

    for (const std::string_view arg : args) {
        if (arg == "--all-columns") {
            all_columns = true;
        } else if (arg == "--write-missing") {
            write_missing = true;
        } else if (!has_path) {
            path = arg;
            has_path = true;
        } else {
            std::cerr << "covarray: unrecognized argument '" << arg << "'\n";
            return 2;
        }
    }

    if (!has_path) {
        std::cerr << "covarray: verify needs a file argument\n";
        return 2;
    }

    covarray::array a;
    try {
        a = covarray::read_array(std::filesystem::path(path));
    } catch (const std::exception& e) {
        std::cerr << "covarray: " << e.what() << '\n';
        return 2;
    }

    covarray::verify_options options;
    options.check_all_combinations = all_columns;
    // In the default (halt-at-first-failure) mode there is at most one
    // deficient combination to hold onto, and reporting it is the whole
    // point. Under --all-columns the list can be large, so it is only
    // collected when --write-missing will actually use it.
    options.collect_missing = write_missing || !all_columns;

    const covarray::verify_result result = covarray::verify_fast(a, options);

    std::cout << std::string(path) << ": ";
    if (result.is_covering_array) {
        std::cout << "covering array CA(" << a.rows() << "; " << a.strength() << ", " << a.columns()
                  << ", " << a.order() << ")\n";
    } else if (all_columns) {
        std::cout << "NOT a covering array -- " << covarray::summarize(result) << '\n';
    } else {
        std::cout << "NOT a covering array\n";
        print_witness(std::cout, result.missing.front());
    }

    if (write_missing && !result.missing.empty()) {
        const std::string report_path = covarray::missing_report_filename(a);
        covarray::write_missing(report_path, a, result);
        std::cout << "covarray: wrote " << report_path << '\n';
    }

    return result.is_covering_array ? 0 : 1;
}

int run_kleitman_spencer_rows(std::span<const std::string_view> args) {
    if (args.size() != 1) {
        std::cerr << "covarray: kleitman-spencer-rows needs <k>\n";
        return 2;
    }
    std::size_t k = 0;
    if (!parse_uint(args[0], k)) {
        std::cerr << "covarray: invalid k value '" << args[0] << "'\n";
        return 2;
    }

    try {
        std::cout << covarray::kleitman_spencer_optimal_rows(k) << '\n';
    } catch (const std::exception& e) {
        std::cerr << "covarray: " << e.what() << '\n';
        return 2;
    }
    return 0;
}

int run_kleitman_spencer_construct(std::span<const std::string_view> args) {
    if (args.size() != 2) {
        std::cerr << "covarray: kleitman-spencer-construct needs <k> <outfile>\n";
        return 2;
    }
    std::size_t k = 0;
    if (!parse_uint(args[0], k)) {
        std::cerr << "covarray: invalid k value '" << args[0] << "'\n";
        return 2;
    }
    const std::string_view out_path = args[1];

    covarray::array result;
    try {
        result = covarray::kleitman_spencer_construct(k);
    } catch (const std::exception& e) {
        std::cerr << "covarray: " << e.what() << '\n';
        return 2;
    }

    try {
        covarray::write_array(std::filesystem::path(out_path), result);
    } catch (const std::exception& e) {
        std::cerr << "covarray: " << e.what() << '\n';
        return 2;
    }

    std::cout << "covarray: wrote " << out_path << " -- CA(" << result.rows() << "; 2, " << k
               << ", 2)\n";
    return 0;
}

int run_kleitman_spencer_max_columns(std::span<const std::string_view> args) {
    if (args.size() != 2) {
        std::cerr << "covarray: kleitman-spencer-max-columns needs <n> <outfile>\n";
        return 2;
    }
    std::size_t n = 0;
    if (!parse_uint(args[0], n)) {
        std::cerr << "covarray: invalid n value '" << args[0] << "'\n";
        return 2;
    }
    const std::string_view out_path = args[1];

    covarray::array result;
    try {
        result = covarray::kleitman_spencer_construct_max_columns(n);
    } catch (const std::exception& e) {
        std::cerr << "covarray: " << e.what() << '\n';
        return 2;
    }

    try {
        covarray::write_array(std::filesystem::path(out_path), result);
    } catch (const std::exception& e) {
        std::cerr << "covarray: " << e.what() << '\n';
        return 2;
    }

    std::cout << "covarray: wrote " << out_path << " -- CA(" << n << "; 2, " << result.columns()
               << ", 2)\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const std::vector<std::string_view> args(argv + 1, argv + argc);

    if (args.empty() || args.front() == "--help" || args.front() == "-h") {
        print_usage(std::cout);
        return 0;
    }
    if (args.front() == "--version") {
        std::cout << "covarray " << COVARRAY_VERSION_STRING << '\n';
        return 0;
    }
    if (args.front() == "verify") {
        return run_verify(std::span(args).subspan(1));
    }
    if (args.front() == "kleitman-spencer-rows") {
        return run_kleitman_spencer_rows(std::span(args).subspan(1));
    }
    if (args.front() == "kleitman-spencer-construct") {
        return run_kleitman_spencer_construct(std::span(args).subspan(1));
    }
    if (args.front() == "kleitman-spencer-max-columns") {
        return run_kleitman_spencer_max_columns(std::span(args).subspan(1));
    }

    std::cerr << "covarray: unknown command '" << args.front() << "'\n\n";
    print_usage(std::cerr);
    return 2;
}
