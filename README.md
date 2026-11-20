# CoveringArrays

A C++ library and CLI for manipulating covering arrays.

## Layout

- `include/covarray/`, `src/` — the `covarray` library: the array type
  (`array.hpp`), coverage checking (`coverage.hpp`), the text file format
  (`io.hpp`), canonical-form isomorphism checking (`canonical.hpp`), and
  the exact Kleitman-Spencer construction for strength-2, 2-symbol
  covering arrays (`kleitman_spencer.hpp`).
- `tools/covarray_cli.cpp` — the `covarray_cli` command-line tool.
- `tests/` — GoogleTest suite.
- `third_party/googletest/` — the vendored GoogleTest sources the test
  suite builds against.
- `data/instances/` — pre-built covering array instances for manual checks
  and testing; see `data/README.md`.

## Build

```bash
cmake -S . -B build -DCMAKE_CXX_COMPILER=g++
cmake --build build -j
ctest --test-dir build
```

GCC/g++ is the only supported compiler. The project requires C++20 (it
uses `std::span`, among other C++20 library features).

With no `-DCMAKE_BUILD_TYPE`, the build above defaults to `RelWithDebInfo`
(`-O2 -g`). Two other configurations are useful on top of that:

### Release build (optimized, no debug info)

```bash
cmake -S . -B build-release -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
```

`Release` is GCC's `-O3 -DNDEBUG`, with no `-g` -- the smallest, fastest
binary, for benchmarking or anything else where you don't need a debugger
to see source lines.

### ASan/UBSan build

```bash
cmake -S . -B build-asan -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan -j
ctest --test-dir build-asan
```

There is no CMake option for this -- the sanitizer flags are passed
straight through `CMAKE_CXX_FLAGS`/`CMAKE_EXE_LINKER_FLAGS` into every
target, including `covarray_tests`. `-fno-sanitize-recover=all` makes
UBSan abort on the first violation instead of merely printing and
continuing, so a violation always fails the test run it happens in.

## CLI usage

```
covarray_cli verify <file> [--all-columns] [--write-missing]
covarray_cli check-classes <dir> --expect <count>
covarray_cli canonize <file> [--write <outfile>]
covarray_cli is-canonical <file>
covarray_cli kleitman-spencer-rows <k>
covarray_cli kleitman-spencer-construct <k> <outfile>
covarray_cli kleitman-spencer-max-columns <n> <outfile>
covarray_cli --help
covarray_cli --version
```

`verify` checks that `<file>` (the project's `N k v^k t` text format, see
`include/covarray/io.hpp`) is a covering array. Exit status: `0` if it is,
`1` if it is not, `2` on a usage or parse error.

- `--all-columns` checks every column combination instead of halting at the
  first one missing a tuple.
- `--write-missing` writes every deficient combination found to
  `N<rows>k<columns>v<order>^<columns>t<strength>.missing` (columns and
  symbols are both 0-indexed in that report).

Example:

```bash
./build/covarray_cli verify data/instances/CA_N4_k3_v2_t2.txt
```

`check-classes` validates an equivalence-class library: `<dir>` must hold a
`count.txt` containing `--expect`'s value, that many `class_NNNN.txt` files
(`class_0000.txt`, `class_0001.txt`, ...; the same text format as above),
each a valid covering array, with every distinct pair of them
non-isomorphic -- no permutation of rows, columns, and (independently, per
column) symbols turns one into the other. See
`include/covarray/canonical.hpp` for what that means and why it is an
expensive, factorial-time check meant for small arrays. Every problem found
is reported, not just the first. Exit status: `0` if every check passed,
`1` if any did not, `2` on a usage error.

Example:

```bash
./build/covarray_cli check-classes data/some_class_library --expect 50
```

`canonize` and `is-canonical` both read `<file>` (the project's array text
format) and run `covarray::canonical_form_column_major()`/
`covarray::is_canonical()` on it -- see `include/covarray/canonical.hpp`
for what "canonical" means here and note both are factorial-time, with no
size guard -- fine for small arrays, but can hang on large ones.

- `canonize <file> [--write <outfile>]` prints
  `canonical_form_column_major(<file>)`'s entries, space-separated,
  column-major. This is `canonical_form_column_major()`'s own flat
  output, not another covering array text file. `--write <outfile>`
  additionally writes the canonicalized array itself (a genuine covering
  array, via `covarray::canonicalize()`) to `<outfile>`, in the project's
  standard array text format. Exit status: `0` on success, `2` on a usage
  or parse error.
- `is-canonical <file>` prints `true` or `false`: whether `<file>`'s
  array already equals its own canonical form. Exit status: `0` if true,
  `1` if false, `2` on a usage or parse error.

Example:

```bash
./build/covarray_cli canonize data/instances/CA_N4_k3_v2_t2.txt
./build/covarray_cli is-canonical data/instances/CA_N4_k3_v2_t2.txt
```

The three `kleitman-spencer-*` commands build exact, optimal
`CA(N; 2, k, 2)` covering arrays (strength 2, 2 symbols), after Kleitman
and Spencer's 1973 result -- see `include/covarray/kleitman_spencer.hpp`.
Exit status: `0` on success, `2` on a usage or parse error (including
`k < 2` or `n < 4`, which have no valid array to build).

- `kleitman-spencer-rows <k>` prints the fewest rows any `CA(N; 2, k, 2)`
  can have.
- `kleitman-spencer-construct <k> <outfile>` writes a `CA(N; 2, k, 2)`
  with that many rows.
- `kleitman-spencer-max-columns <n> <outfile>` writes a `CA(n; 2, k, 2)`
  with the most columns any such array can have in `n` rows.

Example:

```bash
./build/covarray_cli kleitman-spencer-rows 20            # -> 8
./build/covarray_cli kleitman-spencer-construct 20 ca.txt
```
