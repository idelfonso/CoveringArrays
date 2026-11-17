# data/instances

Pre-built covering array instances, kept here so they can be fed straight to
`covarray_cli verify` (or read via `covarray::read_array`) for manual checks,
regression tests, and benchmarking, without having to construct one by hand
first.

## File format

Each file is the project's plain-text covering array format (see
`include/covarray/io.hpp`):

```
N k v^k t
<row 0: k whitespace-separated symbols>
...
<row N-1: k symbols>
```

`#` and DIMACS-style `c `-prefixed lines are comments.

## Naming convention

`CA_N<rows>_k<columns>_v<order>_t<strength>.txt`, e.g. `CA_N4_k3_v2_t2.txt` is
CA(4; 2, 3, 2). Add a short `c` comment line at the top of new files noting
where the instance came from (hand-built, a known optimal array from the
literature, a specific generator run, etc.).

## Usage

```bash
./build/covarray_cli verify data/instances/CA_N4_k3_v2_t2.txt
```

## NIST instances

Some of the files here (named `ca.<t>.<v>^<k>.txt`, e.g. `ca.2.3^3.txt`) were
downloaded from the NIST covering array tables
(https://math.nist.gov/coveringarrays/, IPOG-F-generated arrays) and
verified with `covarray_cli verify` before being committed. They keep their
original NIST filename rather than this project's `CA_N...` convention
above, so the source of each is unambiguous; `<t>` and `<v>` are the
strength and order, and `<k>` (after the `^`) is the column count. They
vary across all four covering array parameters (strength `t`, order `v`,
column count `k`, and row count `N`); see the individual filenames and the
`c` comment line at the top of each file for the specifics of a given
instance.

Their content was converted from NIST's native format into this project's
format (see "File format" above) in two ways: a header line stating `N k
v^k t` was added (NIST's own files have no such header — `k`, `v`, and `t`
are only encoded in the filename, and the first line is just `N`), and
don't-care cells, written by NIST as `-`, were replaced with this project's
wildcard value `v`.
