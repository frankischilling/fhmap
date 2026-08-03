# Representative benchmark and optimization implementation plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a reproducible comparison against ten reusable C hash-map libraries, then keep only measured fhmap hot-path changes that improve the suite without breaking its correctness or memory guardrails.

**Architecture:** Each library builds as a separate adapter executable linked to one shared C11 benchmark driver. The driver owns deterministic data sets, timing, statistics, validation, and allocation accounting; shell and AWK tools fetch pinned sources and aggregate tab-separated results. fhmap optimization follows the untouched baseline as two independent experiments with correctness and performance gates.

**Tech stack:** C11, POSIX shell, AWK, GNU Make, GCC, Clang, Git, GitHub CLI, ASan, and UBSan.

---

Run every mutating Git or GitHub command through `@git-human-workflow`, and run `@git-commit-author` before each commit. Set a task-specific helper path in the shell used for execution:

```bash
FHMAP_GIT_WORKFLOW=/home/frank/git-human-workflow/.worktrees/gh-auth-status-compat/skills/git-human-workflow/scripts/git-human-workflow.sh
```

Use `@humanizer` in file mode for every Markdown change and in embedded mode for issue, commit, and pull-request text. Before publishing text, pipe it through `bash "$FHMAP_GIT_WORKFLOW" sanitize-text` and inspect the output.

### Task 1: Deterministic data and statistics

**Files:**

- Create: `bench/compare/common/bench_common.h`
- Create: `bench/compare/common/bench_common.c`
- Create: `tests/test_bench_common.c`
- Modify: `Makefile:3-27`

**Step 1: Write the failing common-library test**

Cover the stable mixer, deterministic shuffled integer keys, disjoint misses,
borrowed string lifetime, median, and median absolute deviation:

```c
static void test_statistics(void)
{
    double odd[] = {9.0, 1.0, 4.0};
    double even[] = {8.0, 2.0, 6.0, 4.0};
    assert(bench_median(odd, 3) == 4.0);
    assert(bench_median(even, 4) == 5.0);
    assert(bench_mad(odd, 3, 4.0) == 3.0);
}

static void test_dataset(void)
{
    bench_dataset a = BENCH_DATASET_ZERO;
    bench_dataset b = BENCH_DATASET_ZERO;
    size_t i;
    assert(bench_dataset_init(&a, 4096, UINT64_C(1234)));
    assert(bench_dataset_init(&b, 4096, UINT64_C(1234)));
    for (i = 0; i < a.count; ++i) {
        assert(a.u64_keys[i] == b.u64_keys[i]);
        assert(a.u64_misses[i] == b.u64_misses[i]);
        assert(a.u64_keys[i] != a.u64_misses[i]);
        assert(strcmp(a.str_keys[i], b.str_keys[i]) == 0);
        assert(strcmp(a.str_keys[i], a.str_misses[i]) != 0);
    }
    bench_dataset_destroy(&b);
    bench_dataset_destroy(&a);
}
```

**Step 2: Run the test to verify it fails**

Run: `make build/test_bench_common`

Expected: FAIL because `bench_common.h` and its functions do not exist.

**Step 3: Implement the public benchmark helpers**

Use this interface:

```c
#define BENCH_DATASET_ZERO {0}

typedef struct bench_dataset {
    size_t count;
    uint64_t seed;
    uint64_t *u64_keys;
    uint64_t *u64_misses;
    char **str_keys;
    char **str_misses;
    char *string_storage;
} bench_dataset;

uint64_t bench_mix64(uint64_t value);
bool bench_dataset_init(bench_dataset *data, size_t count, uint64_t seed);
void bench_dataset_destroy(bench_dataset *data);
double bench_seconds_now(void);
double bench_median(double *values, size_t count);
double bench_mad(double *values, size_t count, double median);
```

Generate integer keys from unique even values, derive misses from the paired odd
values, and shuffle both arrays with a fixed xorshift state. Generate printable
strings from the mixed integer values into one owned storage block, then keep
stable borrowed pointers in the arrays. Sort a copy inside the statistics
functions so caller sample order remains unchanged. Return `false` on overflow,
allocation failure, or invalid input.

**Step 4: Run the focused and existing tests**

Run: `make build/test_bench_common && ./build/test_bench_common && make test`

Expected: both test executables exit 0 and print their success lines.

**Step 5: Commit**

```bash
bash "$FHMAP_GIT_WORKFLOW" git add Makefile bench/compare/common tests/test_bench_common.c
bash "$FHMAP_GIT_WORKFLOW" git commit -m "test(bench): add deterministic workloads" -m "Refs #1"
```

### Task 2: Scoped allocation accounting

**Files:**

- Create: `bench/compare/common/bench_alloc.h`
- Create: `bench/compare/common/bench_alloc.c`
- Create: `tests/test_bench_alloc.c`
- Modify: `Makefile`

**Step 1: Write the failing allocator test**

```c
static void test_scope(void)
{
    bench_alloc_stats stats;
    void *a;
    void *b;
    bench_alloc_reset();
    bench_alloc_begin();
    a = bench_malloc(24);
    b = bench_calloc(4, 16);
    assert(a != NULL && b != NULL);
    a = bench_realloc(a, 48);
    assert(a != NULL);
    bench_free(b);
    stats = bench_alloc_end();
    assert(stats.allocations == 3);
    assert(stats.current_bytes == 48);
    assert(stats.peak_bytes >= 88);
    bench_free(a);
    assert(bench_alloc_snapshot().current_bytes == 0);
}
```

Also test zero-size requests, multiplication overflow in `bench_calloc`, failed
reallocation, and an allocation made outside the active scope.

**Step 2: Run the test to verify it fails**

Run: `make build/test_bench_alloc`

Expected: FAIL because the allocator API is missing.

**Step 3: Implement the allocator**

Use this interface:

```c
typedef struct bench_alloc_stats {
    size_t current_bytes;
    size_t peak_bytes;
    size_t allocations;
    size_t frees;
} bench_alloc_stats;

void bench_alloc_reset(void);
void bench_alloc_begin(void);
bench_alloc_stats bench_alloc_end(void);
bench_alloc_stats bench_alloc_snapshot(void);
void *bench_malloc(size_t size);
void *bench_calloc(size_t count, size_t size);
void *bench_realloc(void *pointer, size_t size);
void bench_free(void *pointer);
```

Prefix tracked blocks with a `max_align_t`-aligned header containing requested
size and scope membership. Count requested bytes rather than allocator metadata.
Do not change the old block or counters when `realloc` fails.

**Step 4: Run focused tests with sanitizers**

Run: `make build/test_bench_alloc && ./build/test_bench_alloc && make sanitize`

Expected: allocation tests pass; the existing sanitizer run reports no error.

**Step 5: Commit**

```bash
bash "$FHMAP_GIT_WORKFLOW" git add Makefile bench/compare/common tests/test_bench_alloc.c
bash "$FHMAP_GIT_WORKFLOW" git commit -m "test(bench): track map allocations" -m "Refs #1"
```

### Task 3: Adapter contract, driver, and fhmap adapter

**Files:**

- Create: `bench/compare/common/bench_adapter.h`
- Create: `bench/compare/common/bench_driver.c`
- Create: `bench/compare/adapters/fhmap_adapter.c`
- Create: `tests/test_bench_compare.sh`
- Modify: `Makefile`

**Step 1: Write the failing smoke test**

The script must run both key families and reject malformed output:

```sh
output=$(./build/bench-compare/fhmap --smoke --count 32 --samples 3)
rows=$(printf '%s\n' "$output" | awk -F '\t' '$1 == "result" { count++ } END { print count + 0 }')
test "$rows" -eq 14
printf '%s\n' "$output" | awk -F '\t' '
  $1 == "result" && ($9 != "ok" || $10 == "") { exit 1 }
  END { if (NR == 0) exit 1 }
'
```

**Step 2: Run the smoke test to verify it fails**

Run: `make bench-compare-smoke`

Expected: FAIL because the fhmap adapter executable is absent.

**Step 3: Define the adapter interface**

```c
typedef enum bench_key_kind {
    BENCH_KEYS_U64,
    BENCH_KEYS_CSTR
} bench_key_kind;

typedef struct bench_map_ops {
    const char *name;
    void *(*create)(bench_key_kind kind, size_t expected);
    bool (*reserve)(void *map, size_t expected);
    bool (*put_u64)(void *map, uint64_t key, uint64_t value, bool *inserted);
    bool (*get_u64)(void *map, uint64_t key, uint64_t *value);
    bool (*remove_u64)(void *map, uint64_t key);
    bool (*put_cstr)(void *map, const char *key, uint64_t value, bool *inserted);
    bool (*get_cstr)(void *map, const char *key, uint64_t *value);
    bool (*remove_cstr)(void *map, const char *key);
    bool (*iterate)(void *map, uint64_t *checksum, size_t *seen);
    size_t (*size)(const void *map);
    size_t (*capacity)(const void *map);
    void (*destroy)(void *map);
} bench_map_ops;

extern const bench_map_ops bench_adapter;
```

The driver parses `--count`, `--samples`, `--seed`, and `--smoke`. It runs
reserved insert, growing insert, update, hit lookup, miss lookup, erase, and
iteration for both key families. Mixed churn is added in Task 8. Every row uses
this tab-separated schema:

```text
result map key_kind workload count samples median_ns_per_op mad_ns_per_op status checksum final_size current_bytes peak_bytes allocations capacity
```

Print diagnostics to stderr and exit nonzero on clock failure, allocation
failure, wrong size, wrong checksum, or adapter failure.

**Step 4: Implement the fhmap adapter**

Create raw fhmap configurations for `uint64_t` and borrowed `const char *` keys.
Set `FHMAP_MALLOC` and `FHMAP_FREE` to the scoped allocator before including the
implementation. Translate `FHMAP_PUT_ERROR`, `FHMAP_PUT_UPDATED`, and
`FHMAP_PUT_INSERTED` into the adapter contract without timing data generation.

**Step 5: Run the smoke and regression tests**

Run: `make bench-compare-smoke test clang-test sanitize`

Expected: 14 valid fhmap result rows; all existing tests pass.

**Step 6: Commit**

```bash
bash "$FHMAP_GIT_WORKFLOW" git add Makefile bench/compare tests/test_bench_compare.sh
bash "$FHMAP_GIT_WORKFLOW" git commit -m "feat(bench): add adapter runner" -m "Refs #1"
```

### Task 4: Pinned competitor fetcher

**Files:**

- Create: `bench/compare/competitors.lock`
- Create: `bench/compare/fetch-competitors.sh`
- Modify: `tests/test_bench_compare.sh`
- Modify: `Makefile`

**Step 1: Add a failing lock-file validation test**

Run the fetcher in offline validation mode and assert ten unique names, valid
40-digit lowercase commits, HTTPS GitHub URLs, license paths, and source paths:

```sh
./bench/compare/fetch-competitors.sh --check-lock
test "$(awk -F '\t' '!/^#/ { print $1 }' bench/compare/competitors.lock | sort -u | wc -l)" -eq 10
```

**Step 2: Run the validation test to verify it fails**

Run: `./tests/test_bench_compare.sh`

Expected: FAIL because the lock and fetch script do not exist.

**Step 3: Add the pinned lock entries**

Use tab-separated fields `name`, `repository`, `commit`, `license`, and
`source-paths`. Pin these commits:

```text
cc	https://github.com/JacksonAllan/CC.git	2d62942eb2369b5387e0c4f2aa06f052004d16e0	LICENSE	cc.h
verstable	https://github.com/JacksonAllan/Verstable.git	dd83033fb72736a1d2332e43b84b7794b5d19635	LICENSE	verstable.h
khashl	https://github.com/attractivechaos/khashl.git	9e86261a66015c56c596ef31beee66d96dec5920	khashl.h	khashl.h
khash	https://github.com/attractivechaos/klib.git	97a0fcb790b43b9e5da8994f4671021fec036f19	LICENSE.txt	khash.h
mlib	https://github.com/P-p-H-d/mlib.git	4acdfe4c12040150e2e531517a005a1aace6b794	LICENSE	m-core.h,m-dict.h
stc	https://github.com/stclib/STC.git	7c65ea473240270fcb8c0a287b04257b58902125	LICENSE	include/stc
stb_ds	https://github.com/nothings/stb.git	2c980bb59875b0d32144a71867fbdebb2f77cd20	LICENSE	stb_ds.h
uthash	https://github.com/troydhanson/uthash.git	a49bed0b4abb7dff16c73906dcdc8a9718d582d2	LICENSE	src/uthash.h
tidwall	https://github.com/tidwall/hashmap.c.git	3735986914fc1c4e6a9123b406d0970666aa4aec	LICENSE	hashmap.h,hashmap.c
pottery	https://github.com/ludocode/pottery.git	741bf5fa1ef348c43a86e79322545dc7d6320007	LICENSE	include/pottery
```

Verify every license filename against the pinned checkout during execution and
correct the lock before committing if an upstream repository uses a different
path.

**Step 4: Implement fetch and verification**

The script accepts `--check-lock` and `--dest DIR`. For each entry, clone with
`--filter=blob:none --no-checkout`, fetch the exact commit, detach at that
commit, and verify `git rev-parse HEAD` before copying nothing. Adapters compile
against `build/competitors/<name>` in place. An existing checkout with the wrong
commit is an error, not an implicit update.

**Step 5: Run online and offline checks**

Run:

```bash
./bench/compare/fetch-competitors.sh --check-lock
./bench/compare/fetch-competitors.sh --dest build/competitors
./bench/compare/fetch-competitors.sh --dest build/competitors
```

Expected: the first fetch checks out ten exact commits; the second is an
idempotent verification with no checkout changes.

**Step 6: Commit**

```bash
bash "$FHMAP_GIT_WORKFLOW" git add Makefile bench/compare/competitors.lock bench/compare/fetch-competitors.sh tests/test_bench_compare.sh
bash "$FHMAP_GIT_WORKFLOW" git commit -m "build(bench): pin competitor sources" -m "Refs #1"
```

### Task 5: CC, Verstable, khashl, and khash adapters

**Files:**

- Create: `bench/compare/adapters/cc_adapter.c`
- Create: `bench/compare/adapters/verstable_adapter.c`
- Create: `bench/compare/adapters/khashl_adapter.c`
- Create: `bench/compare/adapters/khash_adapter.c`
- Modify: `Makefile`
- Modify: `tests/test_bench_compare.sh`

**Step 1: Add the four adapters to the expected smoke list**

```sh
for adapter in cc verstable khashl khash; do
    test -x "build/bench-compare/$adapter"
    "build/bench-compare/$adapter" --smoke --count 32 --samples 3 |
        awk -F '\t' '$1 == "result" && $9 != "ok" { exit 1 }'
done
```

**Step 2: Run the smoke target to verify it fails**

Run: `make bench-compare-smoke`

Expected: FAIL on the first missing adapter executable.

**Step 3: Implement one adapter at a time**

Read each pinned public header and its examples before writing the adapter. Use
the library's typed or macro-generated `uint64_t` map for integer keys and its
documented content-hash configuration for strings. Store `uint64_t` values,
reserve where supported, and expose unsupported capacity as zero. Keep all data
ownership in the driver.

After each adapter, run:

```bash
make "build/bench-compare/<name>"
"build/bench-compare/<name>" --smoke --count 32 --samples 3
```

Expected: 14 `status=ok` rows and exit 0 for each adapter.

**Step 4: Run the batch checks**

Run: `make bench-compare-smoke test sanitize`

Expected: fhmap and all four competitor adapters pass.

**Step 5: Commit**

```bash
bash "$FHMAP_GIT_WORKFLOW" git add Makefile bench/compare/adapters tests/test_bench_compare.sh
bash "$FHMAP_GIT_WORKFLOW" git commit -m "feat(bench): add first map adapters" -m "Refs #1"
```

### Task 6: M*LIB, STC, and stb_ds adapters

**Files:**

- Create: `bench/compare/adapters/mlib_adapter.c`
- Create: `bench/compare/adapters/stc_adapter.c`
- Create: `bench/compare/adapters/stb_ds_adapter.c`
- Modify: `Makefile`
- Modify: `tests/test_bench_compare.sh`

**Step 1: Extend the failing smoke list**

Add `mlib`, `stc`, and `stb_ds`. The stb adapter uses `hm` for integer keys and
`sh` for strings inside one executable.

**Step 2: Run the smoke target to verify it fails**

Run: `make bench-compare-smoke`

Expected: FAIL on `mlib` before implementation.

**Step 3: Implement and verify each adapter**

Use `DICT_OA` for M*LIB, `hmap` for STC, and the documented binary-key and
string-key modes in `stb_ds.h`. Define each library's allocator macros before
its headers when supported. Do not include more than one competitor in an
adapter translation unit.

Run the focused build and 32-entry smoke command after each file, then run:

`make bench-compare-smoke test clang-test sanitize`

Expected: eight total map executables pass all adapter and fhmap tests.

**Step 4: Commit**

```bash
bash "$FHMAP_GIT_WORKFLOW" git add Makefile bench/compare/adapters tests/test_bench_compare.sh
bash "$FHMAP_GIT_WORKFLOW" git commit -m "feat(bench): add typed map adapters" -m "Refs #1"
```

### Task 7: uthash, tidwall, and Pottery adapters

**Files:**

- Create: `bench/compare/adapters/uthash_adapter.c`
- Create: `bench/compare/adapters/tidwall_adapter.c`
- Create: `bench/compare/adapters/pottery_adapter.c`
- Modify: `Makefile`
- Modify: `tests/test_bench_compare.sh`

**Step 1: Extend the failing smoke list**

Add `uthash`, `tidwall`, and `pottery` to the expected executable list.

**Step 2: Run the smoke target to verify it fails**

Run: `make bench-compare-smoke`

Expected: FAIL on `uthash` before implementation.

**Step 3: Implement the adapters**

For uthash, use separate record structs for integer and borrowed string keys and
exclude record allocation from timed map operations only when the same policy
is disclosed in its result metadata. For tidwall, provide documented compare,
hash, and free callbacks. For Pottery, instantiate the open hash map template in
one translation unit for each key family.

Run each focused smoke test, then run the whole adapter suite.

**Step 4: Verify all adapters**

Run: `make bench-compare-fetch bench-compare-smoke test clang-test sanitize`

Expected: eleven implementations across ten competitor repositories plus fhmap
produce valid rows; all local tests pass.

**Step 5: Commit**

```bash
bash "$FHMAP_GIT_WORKFLOW" git add Makefile bench/compare/adapters tests/test_bench_compare.sh
bash "$FHMAP_GIT_WORKFLOW" git commit -m "feat(bench): complete map adapters" -m "Refs #1"
```

### Task 8: Mixed churn, aggregation, and Make targets

**Files:**

- Modify: `bench/compare/common/bench_driver.c`
- Create: `bench/compare/run-comparison.sh`
- Create: `bench/compare/summarize.awk`
- Create: `tests/fixtures/bench-valid.tsv`
- Create: `tests/fixtures/bench-failed.tsv`
- Modify: `tests/test_bench_compare.sh`
- Modify: `Makefile`

**Step 1: Write failing churn and parser tests**

Add one expected churn row per key family. Feed the valid fixture to AWK and
assert a geometric-mean summary for each map. Feed the failed fixture and assert
nonzero exit status. A missing adapter, duplicate scenario, `N/A` primary row,
or non-`ok` row must fail aggregation.

**Step 2: Run the tests to verify they fail**

Run: `./tests/test_bench_compare.sh`

Expected: FAIL because churn and the summarizer are absent.

**Step 3: Implement steady churn**

Start from a reserved map at about 70 percent of its growth limit. Use a fixed
operation stream with 50 percent hit lookup, 20 percent miss lookup, 15 percent
erase, and 15 percent insert. Pair each erase with a later insert so the live
population stays bounded. Validate size and a shadow checksum after every timed
sample.

**Step 4: Implement the runner and summary**

`run-comparison.sh` records `uname`, CPU model, compiler version, flags, branch,
commit, and lock-file commits before concatenating adapter rows. Use `taskset -c
0` when available, unless `FHMAP_BENCH_CPU` selects another CPU. `summarize.awk`
computes per-workload ranks and an equally weighted geometric mean from primary
rows only.

Add these Make targets:

```make
bench-compare-fetch:
	./bench/compare/fetch-competitors.sh --dest $(BUILD)/competitors

bench-compare-build: bench-compare-fetch $(BENCH_COMPARE_BINS)

bench-compare-smoke: bench-compare-build
	./tests/test_bench_compare.sh

bench-compare: bench-compare-build
	./bench/compare/run-comparison.sh
```

**Step 5: Run parser, smoke, and short comparison tests**

Run:

```bash
./tests/test_bench_compare.sh
FHMAP_BENCH_COUNTS=32,4096 FHMAP_BENCH_SAMPLES=3 make bench-compare
```

Expected: the fixtures behave as specified and the short report has complete
metadata, result rows, ranks, and geometric means.

**Step 6: Commit**

```bash
bash "$FHMAP_GIT_WORKFLOW" git add Makefile bench/compare tests
bash "$FHMAP_GIT_WORKFLOW" git commit -m "feat(bench): aggregate comparison runs" -m "Refs #1"
```

### Task 9: Untouched baseline and benchmark guide

**Files:**

- Create: `bench/results/2026-08-02-i9-10900f-gcc13-baseline.tsv`
- Create: `docs/BENCHMARKS.md`
- Modify: `README.md:135-158`

**Step 1: Record the untouched baseline**

Run:

```bash
FHMAP_BENCH_COUNTS=32,4096,262144,1048576 \
FHMAP_BENCH_SAMPLES=9 \
./bench/compare/run-comparison.sh \
  > bench/results/2026-08-02-i9-10900f-gcc13-baseline.tsv
```

Expected: every primary scenario is present with `status=ok`; the summary names
fhmap's rank without a universal performance claim.

**Step 2: Write the benchmark guide**

Document prerequisites, pinned-source fetching, Make targets, environment
variables, output columns, memory accounting, native versus shared hashes,
noise controls, and how to reproduce the reference result. State the CPU,
compiler, flags, revisions, and date found in the result file.

Use `@humanizer` in file mode. Run its draft, audit, and final loop internally.
Do not add facts that are absent from the result file or lock file.

**Step 3: Update the README command list**

Add `make bench-compare-fetch`, `make bench-compare-smoke`, and `make
bench-compare`. Link to `docs/BENCHMARKS.md`. Keep the existing warning that
hardware, compiler, flags, distribution, and load factor affect results.

**Step 4: Verify the documentation and result**

Run:

```bash
rg -n '\x{2014}|\x{2013}|\x{201c}|\x{201d}' README.md docs/BENCHMARKS.md && exit 1 || true
git diff --check
./bench/compare/summarize.awk bench/results/2026-08-02-i9-10900f-gcc13-baseline.tsv
```

Expected: no banned punctuation, no whitespace errors, and a complete summary.

**Step 5: Commit**

```bash
bash "$FHMAP_GIT_WORKFLOW" git add README.md docs/BENCHMARKS.md bench/results/2026-08-02-i9-10900f-gcc13-baseline.tsv
bash "$FHMAP_GIT_WORKFLOW" git commit -m "docs: publish benchmark baseline" -m "Refs #1"
```

### Task 10: Combined insertion probe experiment

**Files:**

- Modify: `fhmap.h:469-602,786-824`
- Modify: `tests/test_fhmap.c:20-260`
- Create: `bench/results/2026-08-02-combined-probe.tsv`

**Step 1: Add a failing probe-count test**

Define a test-only hook before including fhmap:

```c
static size_t group_matches;
#define FHMAP_TEST_GROUP_MATCH() (++group_matches)
```

Call `FHMAP_TEST_GROUP_MATCH()` once at the top of `fhmap__group_match`, with an
empty default when the macro is undefined. Reserve an empty table, reset the
counter, insert one new key, and assert that one probe group needs at most three
control comparisons:

```c
group_matches = 0;
assert(u64_map_put(&map, UINT64_C(7), UINT64_C(9)) == FHMAP_PUT_INSERTED);
assert(group_matches > 0 && group_matches <= 3);
```

Add collision and tombstone cases that update an existing key after a deleted
slot and insert a missing key into the earliest valid tombstone.

**Step 2: Run the test to verify it fails**

Run: `make clean test`

Expected: FAIL because the current new-key path calls both lookup and insert
scans and exceeds the three-comparison bound.

**Step 3: Implement one combined probe**

Add an internal result type:

```c
typedef struct fhmap__probe_result {
    size_t index;
    size_t insert_slot;
} fhmap__probe_result;
```

Implement `fhmap__find_or_prepare_hashed`. For each control group, collect
fingerprint matches, empty bytes, and deleted bytes. Check every matching key
before the first empty. Preserve the earliest tombstone before that empty. Set
`index` for an existing key; otherwise set `insert_slot` to the earliest valid
tombstone or empty bucket. Bound the scan by capacity.

Use the combined result in `fhmap_emplace`. Recompute it after any tombstone
cleanup or growth rehash. Keep `fhmap__find_index_hashed` for get and remove and
keep the insert-only helper for rehash, where duplicate detection is unnecessary.

**Step 4: Run correctness verification**

Run: `make test clang-test sanitize`

Expected: all tests pass, including collisions, tombstones, and the new count.

**Step 5: Run the candidate comparison**

Run the same environment and command used for the baseline, writing to
`bench/results/2026-08-02-combined-probe.tsv`. Compare it with the untouched
baseline using the summarizer.

Expected: insertion improves by more than its median absolute deviation, no
operation regresses by more than 5 percent, and fhmap table bytes do not change.

**Step 6: Keep or reject the experiment**

If the gate passes, commit the source, tests, and result. If it fails, restore
the source with an explicit reverse patch, retain a short rejected-experiment
record in the issue, and do not claim an improvement.

```bash
bash "$FHMAP_GIT_WORKFLOW" git add fhmap.h tests/test_fhmap.c bench/results/2026-08-02-combined-probe.tsv
bash "$FHMAP_GIT_WORKFLOW" git commit -m "perf: combine insertion probes" -m "Refs #1"
```

### Task 11: Hot API inlining experiment

**Files:**

- Modify: `fhmap.h:243-271,765-866`
- Create: `tests/test_hot_inline.sh`
- Create: `tests/multitu_impl.c`
- Create: `tests/multitu_user.c`
- Modify: `Makefile`
- Create: `bench/results/2026-08-02-hot-inline.tsv`

**Step 1: Write failing code-generation and linkage tests**

Build a normal two-translation-unit program where `multitu_impl.c` defines
`FHMAP_IMPLEMENTATION` and `multitu_user.c` calls get, put, and remove. It must
link and run. Separately compile `bench/bench_fhmap.c` with GCC and Clang at
`-O3`, disassemble `main`, and fail when it contains direct calls to
`fhmap_put`, `fhmap_get_const`, or `fhmap_remove`.

**Step 2: Run the test to verify the code-generation assertion fails**

Run: `./tests/test_hot_inline.sh`

Expected: multi-TU linkage passes, but the current optimized benchmark still
contains direct calls to hot raw functions.

**Step 3: Add a portable hot-definition macro**

Use an empty fallback and a GNU/Clang implementation:

```c
#if defined(__GNUC__) || defined(__clang__)
#  define FHMAP_HOT_DEFINITION inline __attribute__((always_inline, hot))
#else
#  define FHMAP_HOT_DEFINITION
#endif
```

Apply it only to the implementation definitions of `fhmap_get_const`,
`fhmap_get`, `fhmap_contains`, `fhmap_emplace`, `fhmap_put`,
`fhmap_remove_copy`, and `fhmap_remove`. Keep their public declarations and
external symbols so translation units without the implementation still link.
Undefine the macro with the other private implementation macros.

**Step 4: Verify code generation and portability**

Run:

```bash
./tests/test_hot_inline.sh
make test clang-test sanitize
```

Expected: GCC and Clang inline the hot calls in the implementation translation
unit, the two-file program links, and all behavior tests pass.

**Step 5: Benchmark and enforce the gate**

Record `bench/results/2026-08-02-hot-inline.tsv` with the same baseline command.
Check throughput, variation, table bytes, executable size, and every individual
scenario. Reject the experiment if the geometric mean does not improve beyond
variation, any operation regresses by more than 5 percent, or portability tests
fail.

**Step 6: Keep or reject the experiment**

If accepted:

```bash
bash "$FHMAP_GIT_WORKFLOW" git add fhmap.h Makefile tests bench/results/2026-08-02-hot-inline.tsv
bash "$FHMAP_GIT_WORKFLOW" git commit -m "perf: inline hot map operations" -m "Refs #1"
```

If rejected, reverse the uncommitted source patch and record the evidence in the
issue without committing the failed implementation.

### Task 12: Final report, verification, push, and draft PR

**Files:**

- Modify: `docs/BENCHMARKS.md`
- Modify: `README.md`
- Create: `bench/results/2026-08-02-final.tsv`

**Step 1: Record the final full comparison**

Run the four-size, nine-sample command from Task 9 against the accepted code and
write `bench/results/2026-08-02-final.tsv`. Run the summarizer against baseline
and final files. Confirm all acceptance gates from the design document.

**Step 2: Update performance documentation**

Report the observed before and after fhmap results, competitor ranks, memory,
hardware, compiler, flags, lock revisions, and limitations. Change README claims
only when the final result supports the exact wording. Run `@humanizer` in file
mode on both documents and keep code blocks, commands, numbers, and links intact.

**Step 3: Run fresh full verification**

Run:

```bash
make clean
make test clang-test sanitize
make bench-compare-smoke
FHMAP_BENCH_COUNTS=32,4096 FHMAP_BENCH_SAMPLES=3 make bench-compare
git diff --check
if rg -n '\x{2014}|\x{2013}|\x{201c}|\x{201d}' README.md docs; then exit 1; fi
```

Expected: every command exits 0; the final `rg` prints nothing; the short
comparison contains all adapters and successful rows.

**Step 4: Commit the final report**

```bash
bash "$FHMAP_GIT_WORKFLOW" git add README.md docs/BENCHMARKS.md bench/results/2026-08-02-final.tsv
bash "$FHMAP_GIT_WORKFLOW" git commit -m "docs: publish optimized benchmark" -m "Closes #1"
```

Verify the author and committer:

```bash
git show -1 --format='%an <%ae>%n%cn <%ce>' --no-patch
```

Expected: both lines show `Francis <frankhagan890@gmail.com>`.

**Step 5: Push through the guarded workflow**

```bash
bash "$FHMAP_GIT_WORKFLOW" git push -u origin perf/issue-1-benchmark-suite
```

**Step 6: Create a humanized draft PR**

The PR body must state the benchmark scope, accepted and rejected experiments,
measured results, memory outcome, verification commands, and `Closes #1`. Run
the body through `@humanizer` in embedded mode and `sanitize-text`, inspect it,
then create the PR:

```bash
bash "$FHMAP_GIT_WORKFLOW" gh pr create \
  --base main \
  --head perf/issue-1-benchmark-suite \
  --draft \
  --title "Benchmark leading C maps and tune fhmap" \
  --body "$FHMAP_PR_BODY"
```
