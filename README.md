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

## CLI usage

```
covarray_cli verify <file> [--all-columns] [--write-missing]
covarray_cli check-classes <dir> --expect <count>
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
