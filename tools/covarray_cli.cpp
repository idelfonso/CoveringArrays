/// @file
/// @brief `covarray_cli` — thin command-line front end over the library.
///
/// Today this has exactly one subcommand:
///
/// @code
/// covarray_cli verify <file> [--all-columns] [--write-missing]
/// @endcode
///
/// built on covarray::verify_fast(). `--help` and `--version` are a stable
/// contract that `tools/CMakeLists.txt` already pins down with CTest smoke
/// tests (`covarray.cli.help`, `covarray.cli.version`).
///
/// `verify` checks that `<file>` (the project's `N k v^k t` text format, see
/// covarray/io.hpp) is a covering array:
///   - By default it halts at the first `C(k, t)` column combination missing
///     a tuple and reports that combination's columns plus one example
///     missing tuple — a witness, never a bare failure.
///   - `--all-columns` checks every combination instead and prints the
///     location-free summarize() count.
///   - `--write-missing` writes every deficient combination found (just the
///     one, in the default mode) to its canonical
///     `covarray::missing_report_filename()`.
///
/// One subtlety: `verify_options::collect_missing` is forced on whenever
/// `--all-columns` is *not* given, even without `--write-missing` — the
/// default mode holds at most one deficient combination, which is exactly
/// what the witness message needs. Under `--all-columns` it is only turned
/// on when `--write-missing` will actually consume it, since that list can
/// be large.
///
/// Exit status: `0` if `<file>` is a covering array, `1` if it is not, `2`
/// on a usage or parse error.
#include <covarray/array.hpp>
#include <covarray/config.hpp>
#include <covarray/coverage.hpp>
#include <covarray/io.hpp>

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
      << "  covarray_cli --help\n"
      << "  covarray_cli --version\n\n"
      << "verify checks that <file> (the project's 'N k v^k t' text format) is a\n"
      << "covering array. Exit status: 0 if it is, 1 if it is not, 2 on a usage or\n"
      << "parse error.\n\n"
      << "  --all-columns    check every C(k, t) column combination instead of\n"
      << "                   halting at the first one missing a tuple.\n"
      << "  --write-missing  write every deficient combination found to\n"
      << "                   N<rows>k<columns>v<order>^<columns>t<strength>.missing\n";
}

// Prints the columns (1-indexed, for a human reader) and one example missing
// tuple from `combo` — the "uncovered tuple and witnessing columns" the
// project's verifiers are expected to report, never a bare failure.
void print_witness(std::ostream& out, const covarray::missing_combination& combo) {
  out << "  columns:";
  for (const std::size_t c : combo.columns) {
    out << ' ' << (c + 1);
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
    std::cout << "covering array (CA(" << a.rows() << "; " << a.strength() << ", " << a.columns()
              << ", " << a.order() << "))\n";
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

  std::cerr << "covarray: unknown command '" << args.front() << "'\n\n";
  print_usage(std::cerr);
  return 2;
}
