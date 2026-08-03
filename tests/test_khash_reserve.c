#include "../bench/compare/common/bench_adapter.h"
#include "../bench/compare/common/bench_alloc.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static bool fail_next_malloc;
static size_t malloc_failures;

void *__real_bench_malloc(size_t size);

void *__wrap_bench_malloc(size_t size)
{
    if (fail_next_malloc) {
        fail_next_malloc = false;
        ++malloc_failures;
        return NULL;
    }
    return __real_bench_malloc(size);
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

static bool remove_key(void *map, bench_key_kind kind, uint64_t key,
                       const char *string_key)
{
    if (kind == BENCH_KEYS_U64)
        return bench_adapter.remove_u64(map, key);
    return bench_adapter.remove_cstr(map, string_key);
}

static bool get_key(void *map, bench_key_kind kind, uint64_t key,
                    const char *string_key, uint64_t *value)
{
    if (kind == BENCH_KEYS_U64)
        return bench_adapter.get_u64(map, key, value);
    return bench_adapter.get_cstr(map, string_key, value);
}

static void test_reserve(bench_key_kind kind)
{
    char string_keys[32][24];
    bench_alloc_stats before;
    bench_alloc_stats after;
    size_t capacity;
    void *map;
    size_t index;

    for (index = 0u; index < 32u; ++index) {
        int length = snprintf(string_keys[index], sizeof(string_keys[index]),
                              "key-%zu", index);

        assert(length > 0);
        assert((size_t)length < sizeof(string_keys[index]));
    }

    bench_alloc_reset();
    bench_alloc_begin();
    map = bench_adapter.create(kind, 32u);
    assert(map != NULL);

    capacity = bench_adapter.capacity(map);
    before = bench_alloc_snapshot();
    assert(bench_adapter.reserve(map, 32u));
    assert(bench_adapter.capacity(map) == capacity);
    assert_stats_equal(bench_alloc_snapshot(), before);

    for (index = 0u; index < 32u; ++index)
        assert(put_key(map, kind, (uint64_t)index, string_keys[index]));
    for (index = 0u; index < 16u; ++index)
        assert(remove_key(map, kind, (uint64_t)index, string_keys[index]));

    capacity = bench_adapter.capacity(map);
    before = bench_alloc_snapshot();
    assert(bench_adapter.reserve(map, 48u));
    after = bench_alloc_snapshot();
    assert(bench_adapter.capacity(map) == capacity);
    assert(after.allocations > before.allocations);

    before = after;
    assert(bench_adapter.reserve(map, 48u));
    assert(bench_adapter.capacity(map) == capacity);
    assert_stats_equal(bench_alloc_snapshot(), before);

    before = bench_alloc_snapshot();
    assert(bench_adapter.reserve(map, 64u));
    after = bench_alloc_snapshot();
    assert(bench_adapter.capacity(map) > capacity);
    assert(after.allocations > before.allocations);

    bench_adapter.destroy(map);
    after = bench_alloc_end();
    assert(after.current_bytes == 0u);
}

static void test_monotonic_cleanup(bench_key_kind kind)
{
    char string_keys[97][24];
    bench_alloc_stats before;
    bench_alloc_stats after;
    const uint64_t surviving_key = UINT64_C(95);
    uint64_t value = 0u;
    size_t capacity;
    size_t index;
    void *map;

    for (index = 0u; index < 97u; ++index) {
        int length = snprintf(string_keys[index], sizeof(string_keys[index]),
                              "key-%zu", index);

        assert(length > 0);
        assert((size_t)length < sizeof(string_keys[index]));
    }

    bench_alloc_reset();
    bench_alloc_begin();
    map = bench_adapter.create(kind, 96u);
    assert(map != NULL);
    capacity = bench_adapter.capacity(map);
    assert(capacity >= 128u);

    for (index = 0u; index < 96u; ++index)
        assert(put_key(map, kind, (uint64_t)index, string_keys[index]));
    for (index = 0u; index < 95u; ++index)
        assert(remove_key(map, kind, (uint64_t)index, string_keys[index]));
    assert(bench_adapter.size(map) == 1u);

    before = bench_alloc_snapshot();
    malloc_failures = 0u;
    fail_next_malloc = true;
    assert(!bench_adapter.reserve(map, 10u));
    assert(!fail_next_malloc);
    assert(malloc_failures == 1u);
    assert(bench_adapter.capacity(map) == capacity);
    assert(bench_adapter.size(map) == 1u);
    assert(get_key(map, kind, surviving_key, string_keys[95], &value));
    assert(value == surviving_key);
    assert_stats_equal(bench_alloc_snapshot(), before);

    assert(bench_adapter.reserve(map, 10u));
    if (bench_adapter.capacity(map) != capacity) {
        fprintf(stderr, "khash reserve shrank from %zu to %zu buckets\n",
                capacity, bench_adapter.capacity(map));
    }
    assert(bench_adapter.capacity(map) == capacity);
    assert(bench_adapter.size(map) == 1u);
    assert(get_key(map, kind, surviving_key, string_keys[95], &value));
    assert(value == surviving_key);
    assert(put_key(map, kind, UINT64_C(96), string_keys[96]));
    assert(get_key(map, kind, UINT64_C(96), string_keys[96], &value));
    assert(value == UINT64_C(96));

    bench_adapter.destroy(map);
    after = bench_alloc_end();
    assert(after.current_bytes == 0u);
}

int main(void)
{
    test_reserve(BENCH_KEYS_U64);
    test_reserve(BENCH_KEYS_CSTR);
    test_monotonic_cleanup(BENCH_KEYS_U64);
    test_monotonic_cleanup(BENCH_KEYS_CSTR);
    return 0;
}
