#include "../bench/compare/common/bench_adapter.h"
#include "../bench/compare/common/bench_alloc.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool fail_next_malloc;
static bool fail_next_realloc;
static size_t malloc_failures;
static size_t realloc_failures;

void *__real_bench_malloc(size_t size);
void *__real_bench_realloc(void *pointer, size_t size);

void *__wrap_bench_malloc(size_t size)
{
    if (fail_next_malloc) {
        fail_next_malloc = false;
        ++malloc_failures;
        return NULL;
    }
    return __real_bench_malloc(size);
}

void *__wrap_bench_realloc(void *pointer, size_t size)
{
    if (fail_next_realloc) {
        fail_next_realloc = false;
        ++realloc_failures;
        return NULL;
    }
    return __real_bench_realloc(pointer, size);
}

static void assert_stats_equal(bench_alloc_stats actual,
                               bench_alloc_stats expected)
{
    assert(actual.current_bytes == expected.current_bytes);
    assert(actual.peak_bytes == expected.peak_bytes);
    assert(actual.allocations == expected.allocations);
    assert(actual.frees == expected.frees);
}

static bool put_key(void *map, bench_key_kind kind, uint64_t key,
                    const char *string_key)
{
    bool inserted = false;

    if (kind == BENCH_KEYS_U64)
        return bench_adapter.put_u64(map, key, key, &inserted) && inserted;
    return bench_adapter.put_cstr(map, string_key, key, &inserted) &&
           inserted;
}

static bool get_key(void *map, bench_key_kind kind, uint64_t key,
                    const char *string_key, uint64_t *value)
{
    if (kind == BENCH_KEYS_U64)
        return bench_adapter.get_u64(map, key, value);
    return bench_adapter.get_cstr(map, string_key, value);
}

static void initialize_keys(char keys[8][24])
{
    size_t index;

    for (index = 0u; index < 8u; ++index) {
        int length = snprintf(keys[index], sizeof(keys[index]), "key-%zu",
                              index);

        assert(length > 0);
        assert((size_t)length < sizeof(keys[index]));
    }
}

static void populate(void *map, bench_key_kind kind, char keys[8][24])
{
    size_t index;

    for (index = 0u; index < 8u; ++index)
        assert(put_key(map, kind, (uint64_t)index, keys[index]));
}

static size_t count_exact_contents(void *map, bench_key_kind kind,
                                   char keys[8][24])
{
    size_t preserved = 0u;
    size_t index;

    for (index = 0u; index < 8u; ++index) {
        uint64_t value = UINT64_MAX;

        if (get_key(map, kind, (uint64_t)index, keys[index], &value) &&
            value == (uint64_t)index) {
            ++preserved;
        }
    }
    return preserved;
}

static void test_smaller_reserve(bench_key_kind kind)
{
    char keys[8][24];
    bench_alloc_stats before;
    bench_alloc_stats after;
    size_t capacity;
    size_t preserved;
    void *map;

    initialize_keys(keys);
    bench_alloc_reset();
    bench_alloc_begin();
    map = bench_adapter.create(kind, 96u);
    assert(map != NULL);
    populate(map, kind, keys);
    capacity = bench_adapter.capacity(map);
    assert(capacity >= 128u);
    before = bench_alloc_snapshot();

    assert(bench_adapter.reserve(map, 10u));
    after = bench_alloc_snapshot();
    preserved = count_exact_contents(map, kind, keys);
    if (bench_adapter.capacity(map) != capacity) {
        fprintf(stderr, "khashl reserve shrank from %zu to %zu buckets\n",
                capacity, bench_adapter.capacity(map));
    }
    assert(bench_adapter.capacity(map) == capacity);
    assert(bench_adapter.size(map) == 8u);
    assert(preserved == 8u);
    assert_stats_equal(after, before);

    bench_adapter.destroy(map);
    after = bench_alloc_end();
    assert(after.current_bytes == 0u);
}

static void test_failed_shrink_is_not_attempted(bench_key_kind kind)
{
    char keys[8][24];
    bench_alloc_stats before;
    bench_alloc_stats after;
    bool reserved;
    bool failure_was_requested;
    size_t capacity;
    size_t preserved;
    void *map;

    initialize_keys(keys);
    bench_alloc_reset();
    bench_alloc_begin();
    map = bench_adapter.create(kind, 96u);
    assert(map != NULL);
    populate(map, kind, keys);
    capacity = bench_adapter.capacity(map);
    before = bench_alloc_snapshot();

    realloc_failures = 0u;
    fail_next_realloc = true;
    reserved = bench_adapter.reserve(map, 10u);
    failure_was_requested = !fail_next_realloc;
    fail_next_realloc = false;
    after = bench_alloc_snapshot();
    preserved = count_exact_contents(map, kind, keys);
    if (failure_was_requested) {
        fprintf(stderr,
                "khashl failed shrink reported %s and preserved %zu of 8 entries\n",
                reserved ? "success" : "failure", preserved);
    }

    assert(reserved);
    assert(!failure_was_requested);
    assert(realloc_failures == 0u);
    assert(bench_adapter.capacity(map) == capacity);
    assert(bench_adapter.size(map) == 8u);
    assert(preserved == 8u);
    assert_stats_equal(after, before);

    bench_adapter.destroy(map);
    after = bench_alloc_end();
    assert(after.current_bytes == 0u);
}

static void test_growth_failure(bench_key_kind kind)
{
    char keys[8][24];
    bench_alloc_stats before;
    bench_alloc_stats after;
    size_t capacity;
    void *map;

    initialize_keys(keys);
    bench_alloc_reset();
    bench_alloc_begin();
    map = bench_adapter.create(kind, 96u);
    assert(map != NULL);
    populate(map, kind, keys);
    capacity = bench_adapter.capacity(map);
    before = bench_alloc_snapshot();

    malloc_failures = 0u;
    fail_next_malloc = true;
    assert(!bench_adapter.reserve(map, 200u));
    assert(!fail_next_malloc);
    assert(malloc_failures == 1u);
    assert(bench_adapter.capacity(map) == capacity);
    assert(bench_adapter.size(map) == 8u);
    assert(count_exact_contents(map, kind, keys) == 8u);
    assert_stats_equal(bench_alloc_snapshot(), before);

    before = bench_alloc_snapshot();
    realloc_failures = 0u;
    fail_next_realloc = true;
    assert(!bench_adapter.reserve(map, 200u));
    assert(!fail_next_realloc);
    assert(realloc_failures == 1u);
    after = bench_alloc_snapshot();
    assert(after.current_bytes == before.current_bytes);
    assert(after.allocations == before.allocations + 1u);
    assert(after.frees == before.frees + 1u);
    assert(bench_adapter.capacity(map) == capacity);
    assert(bench_adapter.size(map) == 8u);
    assert(count_exact_contents(map, kind, keys) == 8u);

    assert(bench_adapter.reserve(map, 200u));
    assert(bench_adapter.capacity(map) > capacity);
    assert(bench_adapter.size(map) == 8u);
    assert(count_exact_contents(map, kind, keys) == 8u);

    bench_adapter.destroy(map);
    after = bench_alloc_end();
    assert(after.current_bytes == 0u);
}

int main(int argument_count, char **arguments)
{
    if (argument_count == 1 ||
        (argument_count == 2 && strcmp(arguments[1], "shrink") == 0)) {
        test_smaller_reserve(BENCH_KEYS_U64);
        test_smaller_reserve(BENCH_KEYS_CSTR);
        if (argument_count == 2) return 0;
    }
    if (argument_count == 1 ||
        (argument_count == 2 && strcmp(arguments[1], "failed-shrink") == 0)) {
        test_failed_shrink_is_not_attempted(BENCH_KEYS_U64);
        test_failed_shrink_is_not_attempted(BENCH_KEYS_CSTR);
        if (argument_count == 2) return 0;
    }
    if (argument_count == 1 ||
        (argument_count == 2 && strcmp(arguments[1], "growth") == 0)) {
        test_growth_failure(BENCH_KEYS_U64);
        test_growth_failure(BENCH_KEYS_CSTR);
        return 0;
    }
    return 2;
}
