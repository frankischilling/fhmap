# Representative benchmark and optimization design

Date: 2026-08-02

Issue: [#1](https://github.com/frankischilling/fhmap/issues/1)

## Purpose

fhmap needs a reproducible comparison against fast, reusable C hash maps before
its hot paths are changed. The benchmark will show where fhmap is fast, where it
is slow, and how much memory each workload consumes. Optimization will follow
the measurements one change at a time.

The comparison covers general-purpose, single-threaded maps with dynamic
insertion and lookup. Concurrent tables, perfect hashes, fixed data sets, and
maps embedded in larger runtimes are outside this benchmark because their
requirements do not match fhmap's API.

## Competitors

The suite uses ten reusable libraries from the catalog supplied for this work:

1. [CC `cc_map`](https://github.com/JacksonAllan/CC)
2. [Verstable](https://github.com/JacksonAllan/Verstable)
3. [khashl](https://github.com/attractivechaos/khashl)
4. [klib `khash`](https://github.com/attractivechaos/klib/blob/master/khash.h)
5. [M*LIB `DICT_OA`](https://github.com/P-p-H-d/mlib)
6. [STC `hmap`](https://github.com/stclib/STC)
7. [stb_ds `hm` and `sh`](https://github.com/nothings/stb/blob/master/stb_ds.h)
8. [uthash](https://github.com/troydhanson/uthash)
9. [tidwall/hashmap.c](https://github.com/tidwall/hashmap.c)
10. [Pottery open hash map](https://github.com/ludocode/pottery)

A lock file will record the repository URL, commit, license, and files used for
each adapter. A fetch script will place those revisions under `build/`, which
keeps third-party source out of the repository and makes a run repeatable.

## Benchmark structure

Each library will have its own adapter executable. Separate executables avoid
macro and symbol collisions between header libraries. They also isolate memory
measurements and make a failed adapter easy to identify.

Shared benchmark code will provide:

- deterministic integer and string data sets;
- workload sequencing and checksums;
- monotonic timing, warmups, and repeated samples;
- median and median absolute deviation calculations;
- scoped allocation accounting;
- tab-separated result records with environment metadata.

The proposed layout is:

```text
bench/compare/
  common/
  adapters/
  fetch-competitors.sh
  run-comparison.sh
  competitors.lock
bench/results/
docs/BENCHMARKS.md
```

The Makefile will expose `bench-compare-fetch`, `bench-compare-build`,
`bench-compare-smoke`, and `bench-compare`. The normal test target will not
download third-party code.

## Workloads

The primary key families are `uint64_t` and borrowed strings. String storage is
created before timing and remains valid until the map is destroyed. Every
adapter receives the same shuffled keys, lookup order, misses, and operation
sequence for a given seed.

The default population sizes are 32, 4,096, 262,144, and 1,048,576 entries.
They cover call overhead, cache-resident tables, last-level-cache pressure, and
tables larger than cache on the reference machine. Command-line options can
select other sizes and seeds.

Each key family measures:

- insertion into a reserved empty map;
- insertion with normal growth;
- replacement of existing values;
- successful and failed lookup;
- erase;
- full iteration;
- steady mixed churn at a fixed population.

Each operation runs after an untimed warmup. The default report uses nine timed
samples and records the median, median absolute deviation, operation count,
elapsed time, checksum, final size, requested allocation bytes, and peak
requested allocation bytes. A sample must run long enough to avoid timer noise.

The primary ranking uses each library's recommended end-to-end configuration.
A secondary shared-hash run will be included where the public API supports a
custom hash without changing table behavior. Unsupported shared-hash results
will be marked `N/A` and excluded from that comparison.

## Memory accounting

Each adapter is a separate process. A scoped allocator records allocations made
after data-set construction and before map destruction. Adapters will use a
library's allocator hooks when available. The standalone executables can use
linker allocation wrappers when a library has no allocator hook.

The report will distinguish current requested bytes, peak requested bytes,
allocation count, capacity, and resident-set change. Requested bytes are the
primary cross-library measure because allocator metadata and page accounting
vary by system. fhmap's own `fhmap_memory_usage` result will also be checked
against the scoped count.

## Result validity

An adapter must verify its final size and checksum against the shared workload
oracle before it prints a performance row. A mismatch, allocation failure,
clock failure, unsupported compiler requirement, or missing dependency exits
with a nonzero status and names the failed adapter and scenario. The runner will
not turn missing or failed results into zeroes.

The result header records CPU model, operating system, compiler and version,
compiler flags, map revision, data-set seed, population, and sample count. CPU
pinning will be used when `taskset` is available and reported when it is not.
Performance gates compare before and after builds from the same machine and
session rather than comparing unrelated historical runs.

## Optimization process

The first implementation phase builds the harness and records an untouched
fhmap baseline. Profiling and generated assembly will then identify the cost of
hashing, probing, key comparison, API boundaries, and table growth.

Two current hypotheses deserve measurement:

1. New insertion searches once for an existing key and again for an insertion
   slot. A combined probe may remove repeated control-byte scans.
2. Typed wrappers call out-of-line raw API functions. A carefully bounded inline
   path may reduce call and indirect callback overhead for common keys.

These are hypotheses, not promised changes. Each optimization gets its own
before and after benchmark, correctness test, and review. A change stays only
when the repeated results beat normal run-to-run variation and meet every
guardrail.

If changes to the existing core cannot meet the target, a generated typed fast
path may be designed in a separate issue. A wholesale probing-algorithm rewrite
is not part of this issue.

## Acceptance gates

The performance target is the highest geometric-mean throughput across the
primary integer and string workloads in the representative suite. The report
will also show each operation separately so a composite score cannot hide a
large weakness.

An optimization is rejected when:

- an operation is more than 5 percent slower than the untouched baseline;
- fhmap's table-owned memory increases for the same scenario;
- a correctness, compiler, sanitizer, differential, or collision test fails;
- the measured gain does not exceed normal sample variation.

The benchmark report will publish observed results without claiming that fhmap
beats untested implementations or workloads. API comparison will be a factual
feature table covering allocation failure, type safety, custom hashing,
iteration, ownership, portability, and integration size. It will not be folded
into a subjective score.

## Test plan

The existing GCC, Clang, AddressSanitizer, UndefinedBehaviorSanitizer, random
differential, alignment, collision, and tombstone tests remain required. Work
under this issue will add:

- adapter smoke tests with small deterministic data sets;
- cross-adapter size and checksum checks;
- allocator-accounting tests for allocate, reallocate, and free paths;
- benchmark parser tests for successful, missing, and failed rows;
- targeted tests for every hot-path change before its implementation;
- repeated baseline and candidate runs on the reference machine.

CI will build fhmap with GCC and Clang and run local correctness tests. It may
compile benchmark adapters from a prepared dependency cache, but noisy timing
thresholds will remain a documented manual check.

## Documentation

`docs/BENCHMARKS.md` will explain how to fetch, build, run, and interpret the
suite. It will include the tested environment and pinned revisions for any
published result. README performance claims will change only when a recorded
comparison supports the wording.
