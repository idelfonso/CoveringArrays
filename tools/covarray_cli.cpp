// `covarray_cli` — thin command-line front end over the library.
//
// Today this has two subcommands:
//
//   covarray_cli verify <file> [--all-columns] [--write-missing]
//   covarray_cli check-classes <dir> --expect <count>
//
// built on covarray::verify_fast() and covarray::canonical_form()
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
// `check-classes` validates an equivalence-class library in `<dir>`: a
// `count.txt` holding `--expect`'s value, that many `class_NNNN.txt`
// files (the same text format `verify` reads), each individually a valid
// covering array, and every distinct pair of them non-isomorphic under
// permutation of rows, columns, and (independently, per column) symbols
// (see include/covarray/canonical.hpp for what that means and why it is
// an expensive, factorial-time check). It reports every problem it finds
// rather than stopping at the first.
//
// Exit status for check-classes: `0` if every check passed, `1` if any
// did not, `2` on a usage error.
#include <covarray/array.hpp>
#include <covarray/canonical.hpp>
#include <covarray/config.hpp>
#include <covarray/coverage.hpp>
#include <covarray/io.hpp>

#include <charconv>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

void print_usage(std::ostream& out) {
    out << "covarray " << COVARRAY_VERSION_STRING << " -- construct and verify covering arrays\n\n"
        << "Usage:\n"
        << "  covarray_cli verify <file> [--all-columns] [--write-missing]\n"
        << "  covarray_cli check-classes <dir> --expect <count>\n"
        << "  covarray_cli --help\n"
        << "  covarray_cli --version\n\n"
        << "verify checks that <file> (the project's 'N k v^k t' text format) is a\n"
        << "covering array. Exit status: 0 if it is, 1 if it is not, 2 on a usage or\n"
        << "parse error.\n\n"
        << "  --all-columns    check every C(k, t) column combination instead of\n"
        << "                   halting at the first one missing a tuple.\n"
        << "  --write-missing  write every deficient combination found to\n"
        << "                   N<rows>k<columns>v<order>^<columns>t<strength>.missing\n\n"
        << "check-classes checks an equivalence-class library in <dir>: that it has a\n"
        << "count.txt holding exactly --expect (an integer), that it has exactly that\n"
        << "many class_NNNN.txt files (class_0000.txt, class_0001.txt, ..., each the\n"
        << "project's array text format), that each one is a valid covering array (see\n"
        << "'verify' above), and that every distinct pair of them is non-isomorphic --\n"
        << "no permutation of rows, columns, and (independently, per column) symbols\n"
        << "turns one into the other; see include/covarray/canonical.hpp. Every problem\n"
        << "found is reported, not just the first. Exit status: 0 if every check\n"
        << "passed, 1 if any did not, 2 on a usage error (bad arguments, <dir> missing\n"
        << "or unreadable, count.txt missing or not an integer).\n";
}

// Parses `text` in full as a base-10 unsigned integer of type T; returns
// false (leaving `out` unspecified) on a partial, empty, or otherwise
// malformed parse, rather than silently accepting a numeric prefix.
template <typename T>
[[nodiscard]] bool parse_uint(std::string_view text, T& out) {
    const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), out);
    return parsed.ec == std::errc{} && parsed.ptr == (text.data() + text.size());
}

// "class_0000.txt", "class_0001.txt", ... -- the naming convention
// check-classes expects an equivalence-class library's files to follow.
[[nodiscard]] std::string class_filename(std::size_t index) {
    std::ostringstream name;
    name << "class_" << std::setw(4) << std::setfill('0') << index << ".txt";
    return name.str();
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

int run_check_classes(std::span<const std::string_view> args) {
    std::optional<std::size_t> expect;
    std::vector<std::string_view> positional;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        if (arg == "--expect") {
            if (i + 1 >= args.size()) {
                std::cerr << "covarray: --expect needs a value\n";
                return 2;
            }
            std::size_t value = 0;
            if (!parse_uint(args[++i], value)) {
                std::cerr << "covarray: invalid --expect value '" << args[i] << "'\n";
                return 2;
            }
            expect = value;
        } else {
            positional.push_back(arg);
        }
    }

    if (positional.size() != 1) {
        std::cerr << "covarray: check-classes needs <dir>\n";
        return 2;
    }
    if (!expect) {
        std::cerr << "covarray: check-classes needs --expect <count>\n";
        return 2;
    }

    const std::filesystem::path dir(positional[0]);
    std::error_code is_dir_error;
    if (!std::filesystem::is_directory(dir, is_dir_error) || is_dir_error) {
        std::cerr << "covarray: '" << dir.string() << "' is not a readable directory\n";
        return 2;
    }

    const std::filesystem::path count_path = dir / "count.txt";
    std::ifstream count_in(count_path);
    std::size_t declared_count = 0;
    if (!count_in || !(count_in >> declared_count)) {
        std::cerr << "covarray: '" << count_path.string() << "' is missing or does not contain an integer\n";
        return 2;
    }

    bool ok = true;
    if (declared_count != *expect) {
        std::cout << "covarray: " << count_path.string() << " says " << declared_count << ", expected "
                   << *expect << '\n';
        ok = false;
    }

    // Every class that parsed successfully, in the same order as the
    // class_NNNN.txt files; classes.size() == *expect, with a default-
    // constructed (empty) array standing in for any that failed to parse,
    // so indices line up and parse failures are simply skipped below.
    std::vector<covarray::array> classes(*expect);
    std::vector<bool> parsed(*expect, false);

    for (std::size_t i = 0; i < *expect; ++i) {
        const std::filesystem::path class_path = dir / class_filename(i);
        try {
            classes[i] = covarray::read_array(class_path);
        } catch (const std::exception& e) {
            // e.what() already names the path and carries its own
            // "covarray: " prefix (see covarray::read_array).
            std::cout << e.what() << '\n';
            ok = false;
            continue;
        }

        const covarray::verify_result result = covarray::verify_fast(classes[i]);
        if (!result.is_covering_array) {
            std::cout << "covarray: " << class_path.string() << ": NOT a covering array -- "
                       << covarray::summarize(result) << '\n';
            ok = false;
        }

        parsed[i] = true;
    }

    // Each class's canonical form is computed once, up front, so the
    // O(count^2) pairwise comparison below is cheap regardless of how
    // expensive canonical_form() itself is per class.
    std::vector<std::vector<covarray::symbol_type>> canonical_forms(*expect);
    for (std::size_t i = 0; i < *expect; ++i) {
        if (parsed[i]) {
            canonical_forms[i] = covarray::canonical_form(classes[i]);
        }
    }

    for (std::size_t i = 0; i < *expect; ++i) {
        if (!parsed[i]) {
            continue;
        }
        for (std::size_t j = i + 1; j < *expect; ++j) {
            if (parsed[j] && canonical_forms[i] == canonical_forms[j]) {
                std::cout << "covarray: " << class_filename(i) << " and " << class_filename(j)
                           << " are isomorphic\n";
                ok = false;
            }
        }
    }

    if (ok) {
        std::cout << "covarray: " << dir.string() << " -- " << *expect
                   << " classes, all valid covering arrays and pairwise non-isomorphic\n";
    }

    return ok ? 0 : 1;
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
    if (args.front() == "check-classes") {
        return run_check_classes(std::span(args).subspan(1));
    }

    std::cerr << "covarray: unknown command '" << args.front() << "'\n\n";
    print_usage(std::cerr);
    return 2;
}
