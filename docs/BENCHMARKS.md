# Comparison benchmarks

The comparison suite runs `fhmap` and ten pinned C hash map implementations
through the same driver. It covers `uint64_t` keys and borrowed C string keys.
Treat its output as a measurement of one build and host, not as a general
ordering of the libraries.

## Requirements

The scripts expect:

- a POSIX `sh`, Make, Git, and network access to GitHub for the first fetch;
- a C11 compiler such as GCC or Clang that accepts the selected flags;
- POSIX AWK with `exp()` and `log()`, plus `sed`, `tr`, `cmp`, `mktemp`, and
  `uname`;
- enough memory and disk space for eleven native builds and the requested
  data sizes.

Linux `taskset` is optional. When it is installed and the selected CPU is
available, the runner pins each adapter process. Without a usable `taskset`, an
implicit CPU request becomes `none`. An explicit unavailable
`FHMAP_BENCH_CPU` is an error. The runner reads `/proc/cpuinfo` for the CPU
model and falls back to `sysctl` where that interface is available.

## Fetch, build, and run

Run these targets from the repository root:

```sh
make bench-compare-fetch
make bench-compare-build
make bench-compare-smoke
make bench-compare
```

`bench-compare-fetch` validates `bench/compare/competitors.lock`, then fetches
the ten competitors into `build/competitors`. Each checkout must have the
locked origin and exact 40-character commit, a detached HEAD, a clean worktree,
and the tracked license and source paths named by the lock file. The fetcher
rejects symbolic-link targets, nested repositories, dirty checkouts, attached
branches, and partial source trees. New clones stay in a staging directory
until every check passes.

`bench-compare-build` builds all eleven adapters. `bench-compare-smoke` runs the
adapter, fetcher, report, fault-path, and build-matrix tests.
`bench-compare` builds the adapters and prints a full TSV report to standard
output.

The runner accepts these environment variables:

| Variable | Default | Meaning |
|---|---|---|
| `FHMAP_BENCH_COUNTS` | `32,4096,262144,1048576` | Unique, comma-separated positive element counts |
| `FHMAP_BENCH_SAMPLES` | `9` | Timed samples per scenario |
| `FHMAP_BENCH_SEED` | `7640891576956012809` | Unsigned 64-bit dataset seed; zero is valid |
| `FHMAP_BENCH_CPU` | `0` | Logical CPU requested from `taskset` |
| `FHMAP_BENCH_CC` | `${CC:-cc}` | Compiler command checked and recorded in metadata |
| `FHMAP_BENCH_CFLAGS` | `-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -O3 -DNDEBUG -march=native` | Flags recorded in metadata |

Make exports its `CC` value as `FHMAP_BENCH_CC` and its
`BENCH_COMPARE_CFLAGS` value as `FHMAP_BENCH_CFLAGS`. If you run the script
directly, those last two variables describe the binaries; they do not rebuild
them. Keep their values consistent with the preceding build.

## Workloads

Every map runs both key kinds through eight workloads:

| Workload | Timed operation |
|---|---|
| `insert_reserved` | Insert `count` items after the adapter's reserve step |
| `insert_growing` | Insert `count` items without a reserve step |
| `update` | Replace the values of `count` existing keys |
| `lookup_hit` | Read `count` existing keys |
| `lookup_miss` | Probe `count` absent keys |
| `erase` | Remove `count` existing keys |
| `iterate` | Traverse the map, reported per visited entry |
| `churn` | Repeat a fixed mixed-operation block |

Churn starts with `floor(0.7 * count)` live entries, with a minimum of one. Its
20-operation block contains 10 successful lookups, 4 failed lookups, 3 erases,
and 3 inserts, in a fixed interleaved order. The driver rounds the churn
operation count up to a complete 20-operation block. Erase and insert pairs
keep the live set stable.

Datasets, miss keys, and values come from the selected seed. The driver checks
operation counts, values, final size, iteration checksums, and final contents
outside the timer.

## Report format

The report is tab-separated and has no header row. A complete default report
contains four row types.

`meta` rows have three fields:

```text
meta    name    value
```

The metadata names are `format`, `uname`, `cpu_model`, `compiler`,
`compiler_flags`, `branch`, `commit`, `counts`, `samples`, `seed`, `cpu_pin`,
and one `lock_NAME` field for each competitor.

Each `result` row has 15 fields:

| Field | Name | Unit or meaning |
|---:|---|---|
| 1 | row type | `result` |
| 2 | map | Adapter name |
| 3 | key kind | `u64` or `cstr` |
| 4 | workload | One of the eight workload names |
| 5 | count | Requested element count |
| 6 | samples | Number of timed samples |
| 7 | median | Nanoseconds per operation |
| 8 | MAD | Median absolute deviation, in nanoseconds per operation |
| 9 | status | `ok`; the runner stops instead of publishing a failed matrix |
| 10 | checksum | Deterministic validation checksum |
| 11 | final size | Entries after the workload |
| 12 | current bytes | Tracked requested bytes before map destruction |
| 13 | peak bytes | Peak tracked requested bytes |
| 14 | allocations | Tracked allocation calls |
| 15 | capacity | Adapter-reported raw capacity, or zero when unavailable |

`rank` rows have seven fields:

```text
rank    count    key_kind    workload    map    median_ns_per_op    rank
```

Lower median latency ranks first within one count, key kind, and workload.
Exact ties receive the average of the occupied ranks, which is why a rank can
end in `.500`.

`geomean` rows have three fields:

```text
geomean    map    million_operations_per_second
```

The summarizer converts each median with `1000 / median_ns_per_op`, then takes
the geometric mean. Every count, key kind, and workload has equal weight, for
64 scenarios per map in the default report. This weighting does not represent
any particular application's operation mix.

To validate a saved full report and reproduce its rank and geomean section:

```sh
awk -f bench/compare/summarize.awk \
  bench/results/2026-08-02-i9-10900f-gcc13-baseline.tsv
```

The summarizer rejects partial matrices, malformed numeric fields, duplicate
rows, unknown maps or workloads, non-`ok` results, incomplete metadata, and a
saved summary that disagrees with the result rows.

## Timing and memory scope

Map creation, reserve, initial population, destruction, and final validation
are outside the timer. Allocation, growth, hashing, comparison, and value work
caused by a timed operation remain inside it. Small scenarios use enough maps
per batch to cover at least 256 operations. The runner repeats timed batches
until each sample accumulates at least 10 milliseconds.

Two untimed passes precede the samples. The first warms and validates the
scenario. The second records the checksum, size, requested allocation bytes,
allocation count, and capacity used in the result row. Timed samples then run
repeatedly, and the report gives their median and MAD.

Memory counters cover requested bytes routed through the benchmark allocation
hooks. They include adapter wrapper objects and map-owned allocations. They do
not include the dataset, timing buffers, allocator metadata, process RSS, code,
or string storage owned by the dataset. Peak bytes cover setup and the untimed
workload. Current bytes are sampled after the workload and before destruction.

## Adapter policy and caveats

The suite does not force one shared hash function on every library. It uses a
library's normal integer and string hash paths when they are available, so the
timings include each library's hash cost. `fhmap` uses a fixed per-map seed.
tidwall uses its upstream XXH3 helper for integers and SipHash helper for
strings with fixed seeds. Pottery requires callbacks; its adapter uses a direct
or folded integer hash and FNV-1a for strings. stb_ds also receives a fixed seed
for each new map.

All C string adapters compare string contents and borrow the dataset's string
storage. They do not own that text. uthash is different from the inline-entry
adapters because its user record node is part of the data structure. The
benchmark allocates and frees those nodes inside timed insert and erase
operations, counts their memory, and still borrows the C string content.

Reserve and capacity APIs are not uniform. uthash, stb_ds, and the selected
Pottery API have no usable public reserve operation in these adapters, so their
reserve callbacks are no-ops. Their `insert_reserved` results therefore do not
get the preallocation available to adapters with a reserve API. M*LIB reserves
storage but does not expose the raw capacity needed by this adapter. A capacity
of zero means unavailable, not an empty map. Nonzero capacities are raw slots
or buckets and may have different load thresholds, so they are not directly
comparable as usable entry counts.

Allocation-failure behavior also differs. The adapters return failure where
the pinned API makes that possible, but three upstream paths remain
nonrecoverable:

- pinned uthash has `HASH_NONFATAL_OOM=0`, so an internal allocation failure is
  fatal even though the benchmark checks its user record allocation;
- M*LIB uses fatal `M_MEMORY_FULL` behavior on allocation failure;
- stb_ds does not provide a checked insertion allocation result in the pinned
  API.

These three adapters do not demonstrate rollback-safe out-of-memory handling.
Do not infer that behavior from an `ok` benchmark row.

## Noise and reproducibility

The default build uses the same C11 warning set, `-O3`, `-DNDEBUG`, and
`-march=native` for the adapters. tidwall's separately compiled core also uses
native optimization. The fixed seed makes the data reproducible. CPU pinning,
the untimed passes, 10-millisecond sample calibration, nine repeated samples,
and median/MAD reporting reduce common sources of noise, but they do not remove
host load, frequency scaling, kernel, compiler, or microarchitecture effects.

Use a separate checkout to reproduce the reference report without moving the
branch in your working repository. The checkout below starts from `main`, then
creates the recorded branch at commit
`623d9a2f1570cd8a80c4a4dfbb762c94f1cbd353`. The destination must not already
exist.

```sh
source_repo=$(git rev-parse --show-toplevel)
baseline_checkout=../fhmap-benchmark-baseline
git clone --no-local --branch main "$source_repo" "$baseline_checkout"
cd "$baseline_checkout"
git switch -c perf/issue-1-benchmark-suite \
  623d9a2f1570cd8a80c4a4dfbb762c94f1cbd353

baseline_cflags='-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -O3 -DNDEBUG -march=native'
make CC=cc BENCH_COMPARE_CFLAGS="$baseline_cflags" bench-compare-fetch
make CC=cc BENCH_COMPARE_CFLAGS="$baseline_cflags" bench-compare-build
mkdir -p bench/results
FHMAP_BENCH_COUNTS=32,4096,262144,1048576 \
FHMAP_BENCH_SAMPLES=9 \
FHMAP_BENCH_SEED=7640891576956012809 \
FHMAP_BENCH_CPU=0 \
FHMAP_BENCH_CC=cc \
FHMAP_BENCH_CFLAGS="$baseline_cflags" \
  ./bench/compare/run-comparison.sh \
  > bench/results/2026-08-02-i9-10900f-gcc13-baseline.tsv
```

Matching the recorded host metadata also requires the CPU, kernel, and compiler
listed below. CPU 0 must be available to `taskset`. The compiler command is
`cc`, and the first line from `cc --version` must be
`cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`. The commands set the recorded
flags, seed, and CPU explicitly. A different host or toolchain produces a valid
report with different metadata and timing values.

## Pre-optimization baseline

The saved report is
[`bench/results/2026-08-02-i9-10900f-gcc13-baseline.tsv`](../bench/results/2026-08-02-i9-10900f-gcc13-baseline.tsv).
Its filename supplies the 2026-08-02 date; version 1 report metadata does not
contain a wall-clock timestamp.

The report records:

- `Intel(R) Core(TM) i9-10900F CPU @ 2.80GHz`, pinned to logical CPU 0;
- `Linux DESKTOP-CJL9TT2 6.6.87.2-microsoft-standard-WSL2 #1 SMP
  PREEMPT_DYNAMIC Thu Jun  5 18:30:46 UTC 2025 x86_64 x86_64 x86_64
  GNU/Linux`;
- compiler metadata
  `cc cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`, formed from command `cc`
  and version line `cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`;
- `-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -O3 -DNDEBUG
  -march=native`;
- counts `32,4096,262144,1048576`, 9 samples, seed
  `7640891576956012809`, and the branch and commit shown above.

The ten competitor revisions are:

| Adapter | Commit |
|---|---|
| CC | `2d62942eb2369b5387e0c4f2aa06f052004d16e0` |
| Verstable | `dd83033fb72736a1d2332e43b84b7794b5d19635` |
| khashl | `9e86261a66015c56c596ef31beee66d96dec5920` |
| khash | `97a0fcb790b43b9e5da8994f4671021fec036f19` |
| M*LIB | `4acdfe4c12040150e2e531517a005a1aace6b794` |
| STC | `7c65ea473240270fcb8c0a287b04257b58902125` |
| stb_ds | `2c980bb59875b0d32144a71867fbdebb2f77cd20` |
| uthash | `a49bed0b4abb7dff16c73906dcdc8a9718d582d2` |
| tidwall | `3735986914fc1c4e6a9123b406d0970666aa4aec` |
| Pottery | `741bf5fa1ef348c43a86e79322545dc7d6320007` |

Sorting the equal-weight geomean rows from high to low places `fhmap` eighth
of eleven at `26.108846` million operations per second:

| Rank | Adapter | Geomean, million operations/s |
|---:|---|---:|
| 1 | M*LIB | 40.441930 |
| 2 | CC | 39.491569 |
| 3 | STC | 39.287157 |
| 4 | Verstable | 38.130426 |
| 5 | khash | 37.000197 |
| 6 | Pottery | 27.996975 |
| 7 | khashl | 27.112437 |
| 8 | fhmap | 26.108846 |
| 9 | stb_ds | 24.230024 |
| 10 | tidwall | 17.025416 |
| 11 | uthash | 12.893230 |

This is the untouched pre-optimization baseline captured before the combined
insertion-probe and hot API inlining experiments. It does not show that one map
is fastest for every workload. The per-scenario rows are the relevant
comparison when an application's key type, size, and operation mix are known.
