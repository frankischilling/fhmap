#include "../bench/compare/common/bench_adapter.h"
#include "../bench/compare/common/bench_alloc.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void assert_stats_equal(bench_alloc_stats actual,
                               bench_alloc_stats expected)
{
    assert(actual.current_bytes == expected.current_bytes);
    assert(actual.peak_bytes == expected.peak_bytes);
    assert(actual.allocations == expected.allocations);
    assert(actual.frees == expected.frees);
}

static void test_empty_map(bench_key_kind kind)
{
    const char missing[] = "missing";
    bench_alloc_stats before;
    bench_alloc_stats ended;
    uint64_t checksum = UINT64_MAX;
    uint64_t value = UINT64_MAX;
    size_t seen = SIZE_MAX;
    void *map;

    bench_alloc_reset();
    bench_alloc_begin();
    map = bench_adapter.create(kind, 32u);
    assert(map != NULL);
    before = bench_alloc_snapshot();
    assert(bench_adapter.reserve(map, 32u));
    assert(bench_adapter.reserve(map, 32u));
    assert(bench_adapter.reserve(map, 1u));
    assert(bench_adapter.capacity(map) == 0u);
    assert(!bench_adapter.reserve(map, SIZE_MAX));
    if (kind == BENCH_KEYS_U64) {
        assert(!bench_adapter.get_u64(map, UINT64_C(7), &value));
        assert(!bench_adapter.remove_u64(map, UINT64_C(7)));
    } else {
        assert(!bench_adapter.get_cstr(map, missing, &value));
        assert(!bench_adapter.remove_cstr(map, missing));
    }
    assert(value == UINT64_MAX);
    assert(bench_adapter.iterate(map, &checksum, &seen));
    assert(checksum == 0u);
    assert(seen == 0u);
    assert(bench_adapter.size(map) == 0u);
    assert_stats_equal(bench_alloc_snapshot(), before);
    bench_adapter.destroy(map);
    ended = bench_alloc_end();
    assert(ended.current_bytes == 0u);
}

static void test_string_records(void)
{
    char retained[] = "retained";
    char stored[] = "same-content";
    char lookup[] = "same-content";
    bench_alloc_stats after_create;
    bench_alloc_stats after_first;
    bench_alloc_stats after_second;
    bench_alloc_stats before_update;
    bench_alloc_stats before_remove;
    bench_alloc_stats after_remove;
    bench_alloc_stats ended;
    bool inserted = false;
    uint64_t checksum = 0u;
    uint64_t value = 0u;
    size_t seen = 0u;
    void *map;

    assert(&stored[0] != &lookup[0]);
    bench_alloc_reset();
    bench_alloc_begin();
    map = bench_adapter.create(BENCH_KEYS_CSTR, 0u);
    assert(map != NULL);
    after_create = bench_alloc_snapshot();
    assert(bench_adapter.put_cstr(map, retained, UINT64_C(17), &inserted));
    assert(inserted);
    after_first = bench_alloc_snapshot();
    assert(after_first.allocations == after_create.allocations + 3u);
    assert(bench_adapter.put_cstr(map, stored, UINT64_C(41), &inserted));
    assert(inserted);
    after_second = bench_alloc_snapshot();
    assert(after_second.allocations == after_first.allocations + 1u);
    assert(bench_adapter.get_cstr(map, lookup, &value));
    assert(value == UINT64_C(41));

    before_update = bench_alloc_snapshot();
    assert(bench_adapter.put_cstr(map, lookup, UINT64_C(42), &inserted));
    assert(!inserted);
    assert_stats_equal(bench_alloc_snapshot(), before_update);
    assert(bench_adapter.size(map) == 2u);
    assert(bench_adapter.iterate(map, &checksum, &seen));
    assert(seen == 2u);
    assert(checksum == bench_checksum_add(
                           bench_checksum_add(0u, UINT64_C(17)),
                           UINT64_C(42)));

    before_remove = bench_alloc_snapshot();
    assert(bench_adapter.remove_cstr(map, lookup));
    after_remove = bench_alloc_snapshot();
    assert(after_remove.frees == before_remove.frees + 1u);
    assert(!bench_adapter.get_cstr(map, stored, &value));
    assert(bench_adapter.size(map) == 1u);
    bench_adapter.destroy(map);
    assert(strcmp(retained, "retained") == 0);
    assert(strcmp(stored, "same-content") == 0);
    assert(strcmp(lookup, "same-content") == 0);
    ended = bench_alloc_end();
    assert(ended.current_bytes == 0u);
    assert(ended.allocations == ended.frees);
}

static void test_u64_and_wrong_kind(void)
{
    bench_alloc_stats before_update;
    bench_alloc_stats ended;
    bool inserted = true;
    uint64_t value = 0u;
    void *map;

    bench_alloc_reset();
    bench_alloc_begin();
    map = bench_adapter.create(BENCH_KEYS_U64, 0u);
    assert(map != NULL);
    assert(!bench_adapter.put_cstr(map, "wrong", 1u, &inserted));
    assert(!inserted);
    assert(bench_adapter.put_u64(map, UINT64_C(9), UINT64_C(11),
                                 &inserted));
    assert(inserted);
    before_update = bench_alloc_snapshot();
    assert(bench_adapter.put_u64(map, UINT64_C(9), UINT64_C(13),
                                 &inserted));
    assert(!inserted);
    assert_stats_equal(bench_alloc_snapshot(), before_update);
    assert(bench_adapter.get_u64(map, UINT64_C(9), &value));
    assert(value == UINT64_C(13));
    assert(!bench_adapter.get_cstr(map, "9", &value));
    assert(bench_adapter.remove_u64(map, UINT64_C(9)));
    assert(!bench_adapter.remove_u64(map, UINT64_C(9)));
    bench_adapter.destroy(map);
    ended = bench_alloc_end();
    assert(ended.current_bytes == 0u);
    assert(ended.allocations == ended.frees);
}

static void test_invalid_create(void)
{
    assert(bench_adapter.create((bench_key_kind)99, 0u) == NULL);
    assert(bench_adapter.create(BENCH_KEYS_U64, SIZE_MAX) == NULL);
    assert(bench_adapter.create(BENCH_KEYS_CSTR, SIZE_MAX) == NULL);
}

int main(void)
{
    assert(strcmp(bench_adapter.name, "uthash") == 0);
    test_empty_map(BENCH_KEYS_U64);
    test_empty_map(BENCH_KEYS_CSTR);
    test_string_records();
    test_u64_and_wrong_kind();
    test_invalid_create();
    return 0;
}
