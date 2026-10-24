# Comparison benchmarks

The comparison suite runs `fhmap` and ten pinned C hash map implementations
through the same driver. It covers `uint64_t` keys and borrowed C string keys.
Results apply to the measured build, host, and workloads. The suite covers
eleven implementations; it cannot establish a ranking across all hash maps.

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

The count fixes the integer members: keys are the first `count` even `u64`
values, and misses are their paired odd successors. The seed changes the shared
shuffle order, not those sets. C string keys and misses are fixed-width
hexadecimal encodings of the mixed, shuffled integers. The seed is also an
input to the generated original and updated values. The driver checks operation
counts, values, final size, iteration checksums, and final contents outside the
timer.

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
branch in your working repository. The recipe stops if its sibling destination
already exists. It starts the new checkout from `main`, then creates the
recorded branch at commit
`623d9a2f1570cd8a80c4a4dfbb762c94f1cbd353`.

```sh
(
  set -eu

  source_repo=$(git rev-parse --show-toplevel)
  source_parent=$(dirname "$source_repo")
  baseline_checkout=$source_parent/fhmap-benchmark-baseline
  test ! -e "$baseline_checkout"
  baseline_commit=623d9a2f1570cd8a80c4a4dfbb762c94f1cbd353
  git -C "$source_repo" cat-file -e "$baseline_commit^{commit}"

  git clone --no-local --branch main "$source_repo" "$baseline_checkout"
  cd "$baseline_checkout"
  git switch -c perf/issue-1-benchmark-suite "$baseline_commit"

  baseline_cflags='-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -O3 -DNDEBUG -march=native'
  make CC=cc BENCH_COMPARE_CFLAGS="$baseline_cflags" bench-compare-fetch
  make CC=cc BENCH_COMPARE_CFLAGS="$baseline_cflags" bench-compare-build

  report_dir=$baseline_checkout/bench/results
  final_report=$report_dir/2026-08-02-i9-10900f-gcc13-baseline.tsv
  mkdir -p "$report_dir"
  test ! -e "$final_report"
  temporary_report=$(mktemp "$report_dir/.fhmap-baseline.XXXXXX")
  trap 'rm -f "$temporary_report"' EXIT
  trap 'exit 1' HUP INT TERM

  FHMAP_BENCH_COUNTS=32,4096,262144,1048576 \
  FHMAP_BENCH_SAMPLES=9 \
  FHMAP_BENCH_SEED=7640891576956012809 \
  FHMAP_BENCH_CPU=0 \
  FHMAP_BENCH_CC=cc \
  FHMAP_BENCH_CFLAGS="$baseline_cflags" \
    ./bench/compare/run-comparison.sh >"$temporary_report"
  test ! -e "$final_report"
  mv "$temporary_report" "$final_report"
  trap - EXIT HUP INT TERM
)
```

Matching the recorded host metadata also requires the CPU, kernel, and compiler
listed below. CPU 0 must be available to `taskset`. The compiler command is
`cc`, and the first line from `cc --version` must be
`cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`. The recipe sets the recorded
flags, seed, and CPU explicitly. It writes the named report only after the
runner succeeds. A different host or toolchain produces a valid report with
different metadata and timing values.

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

The baseline file has 21 metadata rows, 704 result rows, 704 rank rows, and 11
geomean rows. Its SHA-256 digest is
`f7cadd758baf2acbb2916bdc14e93a8da6843b794922fc522b6edcde0e89a551`.

## Final accepted-code report

The final report is
[`bench/results/2026-08-02-final.tsv`](../bench/results/2026-08-02-final.tsv).
It records commit `3b26e77dbf39cf0ea71866b0367a27dbdf74ca05` on
`perf/issue-1-benchmark-suite`. Its CPU, kernel, compiler, compiler flags,
counts, sample count, seed, CPU pin, and ten competitor lock revisions match
the baseline metadata above. The file has 21 metadata rows, 704 result rows,
704 rank rows, and 11 geomean rows, with all 704 results marked `ok`. Its
SHA-256 digest is
`707b62a62736a9022221814c2eab6f0877d69349293f645710074c09161ceebc`.

The table compares the two saved geomeans. Ranks are calculated separately for
each report.

| Adapter | Baseline rank | Baseline Mops/s | Final rank | Final Mops/s | Observed change |
|---|---:|---:|---:|---:|---:|
| M*LIB | 1 | 40.441930 | 1 | 55.131920 | +36.324% |
| Verstable | 4 | 38.130426 | 2 | 50.546701 | +32.563% |
| STC | 3 | 39.287157 | 3 | 50.487647 | +28.509% |
| khash | 5 | 37.000197 | 4 | 49.999000 | +35.132% |
| CC | 2 | 39.491569 | 5 | 48.698045 | +23.313% |
| khashl | 7 | 27.112437 | 6 | 34.995061 | +29.074% |
| Pottery | 6 | 27.996975 | 7 | 33.574391 | +19.921% |
| fhmap | 8 | 26.108846 | 8 | 32.832863 | +25.754% |
| stb_ds | 9 | 24.230024 | 9 | 31.286507 | +29.123% |
| tidwall | 10 | 17.025416 | 10 | 20.279344 | +19.112% |
| uthash | 11 | 12.893230 | 11 | 17.291412 | +34.112% |

The final run reported a higher fhmap geomean, but it did not run optimized
fhmap code. `fhmap.h` is byte-for-byte identical to the file at the baseline
commit. Every unchanged competitor also reported a higher geomean, ranging
from 19.112% to 36.324%. These are separate long runs, so their differences do
not isolate a code effect.

fhmap's average per-scenario rank changed from 6.469 to 6.813. Compared with
the baseline, 15 of its 64 final medians were more than 5% slower, 46 were more
than 5% faster, and 3 stayed within 5%. The largest slowdown was u64 reserved
insertion at 1,048,576 entries, where median +/- MAD changed from
`63.881 +/- 3.749` to `105.720 +/- 13.090` ns. The largest decrease was u64
update at 262,144 entries, from `116.124 +/- 35.780` to
`37.471 +/- 3.618` ns. Since the implementation did not change, neither result
is evidence of an optimization or regression in the source.

The baseline and final reports match in all 704 checksums, final sizes, current
bytes, peak bytes, allocation counts, and capacities. fhmap's 64 current and
peak byte results are unchanged. Current requested bytes range from `1,287` at
32 entries to `35,651,783` at 1,048,576 entries. The largest peak is
`53,477,606` bytes during growing insertion at 1,048,576 entries. These values
cover the table-owned allocation scope described above, not total process
memory.

## Earlier optimization experiments

Neither measured experiment passed its acceptance gates, so no core
optimization was retained.

The combined insertion probe passed the GCC, Clang, sanitizer, SSE2, scalar,
collision, tombstone, growth, and differential checks. Its separate candidate
run had 11 of 64 fhmap medians more than 5% above the baseline. C string
reserved insertion at 1,048,576 entries was the worst observed case at
`120.946 +/- 12.322` ns, 48.178% above the baseline. Current bytes, peak bytes,
allocation counts, and capacities did not change. Because all ten unchanged
competitors moved by +20.915% to +42.115% in that run, its aggregate throughput
could not be attributed to the probe change. The candidate was removed.

The hot API inlining experiment applied forced inlining to seven public
operations. Under GCC and Clang, the optimized benchmark had no direct calls to
`fhmap_put`, `fhmap_get_const`, or `fhmap_remove`, and its functional checks
passed. It failed the portability gate. GCC produced array-bounds diagnostics
when compiled with `-Werror -O3`, while Clang produced
`-Wstatic-in-inline` diagnostics. Against an adjacent same-session control,
the candidate fhmap geomean was `37.426758` Mops/s versus `40.269948` Mops/s,
a 7.060% decrease. Twenty-five of 64 medians were more than 5% higher, and C
string reserved insertion at 32 entries rose 74.789%. The comparison executable
grew from 48,680 to 56,872 bytes, and its text size grew from 33,878 to 44,250
bytes. The candidate was removed.

Those experiments left the original insertion path and out-of-line raw API
boundary unchanged. They do not establish whether a generated typed fast path
would improve performance; that needs its own portability, code-size, memory,
and repeated timing checks.

That earlier work added the comparison suite but retained no core optimization.
Its results do not establish superiority over untested maps or workloads.

## Paired comparisons of a core change

Use `bench/compare/run-paired.py` when comparing two builds of fhmap. It runs
both executables on the same logical CPU, reverses their order in alternate
rounds, and keeps all per-run medians and MADs. Each executable is copied before
timing, so rebuilding it during a run cannot mix versions in the report.
Python 3 and Linux `taskset` are required for this optional tool.

Build both executables with the same compiler and flags, using separate
checkouts for the baseline and candidate. In each checkout:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
  -O3 -DNDEBUG -march=native \
  bench/compare/common/bench_driver.c \
  bench/compare/common/bench_common.c \
  bench/compare/common/bench_alloc.c \
  bench/compare/adapters/fhmap_adapter.c -o /tmp/fhmap-baseline
```

Change the output name to `/tmp/fhmap-candidate` for the candidate checkout.
Then run:

```sh
python3 bench/compare/run-paired.py \
  --baseline /tmp/fhmap-baseline --candidate /tmp/fhmap-candidate \
  --counts 32,4096,262144,1048576 --samples 9 --rounds 3 \
  --report build/paired.json
```

The default CPU is the first CPU allowed by the process affinity; `--cpu`
selects another. Do not run competing benchmarks on that CPU. The JSON report
records the executable hashes, host, seed, settings, and raw result rows.
A companion JSONL journal preserves completed runs if a later run fails.
Existing report paths are not overwritten.

A speedup ratio above 1 means the candidate took less time. The summary uses
the median paired ratio for each scenario, then their equal-weight geometric
mean. Inspect the individual rounds and regressions as well as that aggregate.
The runner refuses incomplete matrices, failed results, or differences in
checksums, final sizes, allocation counts, bytes, and capacity. This strict
memory check is intended for probe optimizations that leave the layout intact;
changes to allocation policy need a separate evaluation.

Paired runs reduce timing drift but do not remove shared-host interference or
frequency changes. A comparison against the same executable can still report
an apparent gain. Do not call a small aggregate difference an optimization
without checking repeated results and the affected workloads.
