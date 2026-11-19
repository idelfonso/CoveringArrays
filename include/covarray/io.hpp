// Reading and writing the project's covering-array text format.
//
// Kept out of covarray::array on purpose: the matrix type is a pure
// in-memory data structure usable by header-only consumers who never touch
// a filesystem, while parsing and comment handling pull in
// `<fstream>`/`<filesystem>` and have their own failure modes (a malformed
// header, a truncated file). Separating them means a sanitizer build or an
// inner search loop never pays for parser code it doesn't use, and a parser
// bug is never mistaken for an invariant violation of covarray::array
// itself.
//
// File format:
//
// One header line, then N rows of k symbols:
//
//   N k v^k t
//   <row 0: k symbols, whitespace-separated>
//   ...
//   <row N-1: k symbols, whitespace-separated>
//
// e.g. `"3886 12 6^12 4"` is CA(3886; 4, 12, 6): 3886 rows, 12 columns,
// strength 4, symbols 0..5 (6 is the wildcard). The exponent in `v^k` must
// equal the column count `k` given just before it; a mismatch is a format
// error, not silently ignored.
//
// Comments:
//
//   - `#` discards the rest of its line and may appear anywhere on it
//     (Python-style): `"3 2 1    # inline note"` is `"3 2 1"`.
//   - A line whose first non-blank character is `c` or `C`, immediately
//     followed by a blank character or end of line, is discarded in full
//     (DIMACS-style): `"c a comment"` is a comment, but `"Cnotacomment"` is
//     not — it is parsed as data, and will fail as such if it isn't
//     numeric.
//   - Blank lines are skipped.
#ifndef COVARRAY_IO_HPP
#define COVARRAY_IO_HPP

#include <covarray/array.hpp>

#include <filesystem>
#include <iosfwd>

namespace covarray {

// Reads one covering array from `in` in the format documented above.
// Throws std::runtime_error on a malformed header, a header/column
// mismatch, or premature end of input.
// Throws std::invalid_argument on a symbol outside `[0, v]` or on
// dimensions rejected by check_dimensions(). Every error names the
// offending line.
[[nodiscard]] array read_array(std::istream& in);

// Opens `path` and reads one covering array from it.
// Throws std::runtime_error if the file cannot be opened, in addition to
// the errors thrown by read_array(std::istream&).
[[nodiscard]] array read_array(const std::filesystem::path& path);

// Writes `a` in the canonical format above, with no comments.
//
// `a` must have no invalid symbols (see array::find_invalid_symbol()); this
// is not re-checked here, since array cannot hold one without having been
// told to.
void write_array(std::ostream& out, const array& a);

// Opens `path` and writes `a` to it, truncating any existing content.
// Throws std::runtime_error if the file cannot be opened.
void write_array(const std::filesystem::path& path, const array& a);

}  // namespace covarray

#endif  // COVARRAY_IO_HPP
