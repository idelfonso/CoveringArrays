# CoveringArrays

A C++ library and CLI for manipulating covering arrays.

## Layout

- `include/covarray/`, `src/` — the `covarray` library: the array type
  (`array.hpp`), coverage checking (`coverage.hpp`), and the text file format
  (`io.hpp`).
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

GCC/g++ is the only supported compiler.

## CLI usage

```
covarray_cli verify <file> [--all-columns] [--write-missing]
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
