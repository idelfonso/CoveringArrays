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
