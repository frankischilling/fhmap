#include "../bench/compare/common/bench_adapter.h"
#include "../bench/compare/common/bench_alloc.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

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

int main(void)
{
    test_reserve(BENCH_KEYS_U64);
    test_reserve(BENCH_KEYS_CSTR);
    return 0;
}
