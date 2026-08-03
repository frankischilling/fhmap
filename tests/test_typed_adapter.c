#include "../bench/compare/common/bench_adapter.h"
#include "../bench/compare/common/bench_alloc.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void assert_stats_equal(bench_alloc_stats actual,
                               bench_alloc_stats expected)
{
    if (actual.current_bytes != expected.current_bytes ||
        actual.peak_bytes != expected.peak_bytes ||
        actual.allocations != expected.allocations ||
        actual.frees != expected.frees) {
        fprintf(stderr,
                "allocation stats changed: %zu/%zu/%zu/%zu -> "
                "%zu/%zu/%zu/%zu\n",
                expected.current_bytes, expected.peak_bytes,
                expected.allocations, expected.frees,
                actual.current_bytes, actual.peak_bytes,
                actual.allocations, actual.frees);
    }
    assert(actual.current_bytes == expected.current_bytes);
    assert(actual.peak_bytes == expected.peak_bytes);
    assert(actual.allocations == expected.allocations);
    assert(actual.frees == expected.frees);
}

static void test_empty_lookup_no_allocation(bench_key_kind kind)
{
    const uint64_t missing_u64 = UINT64_C(0x123456789abcdef0);
    const char missing_cstr[] = "empty-map-miss";
    bench_alloc_stats before;
    bench_alloc_stats ended;
    uint64_t checksum = UINT64_MAX;
    uint64_t value = UINT64_MAX;
    size_t seen = SIZE_MAX;
    void *map;

    bench_alloc_reset();
    bench_alloc_begin();
    map = bench_adapter.create(kind, 0u);
    assert(map != NULL);
    before = bench_alloc_snapshot();
    if (kind == BENCH_KEYS_U64) {
        assert(!bench_adapter.remove_u64(map, missing_u64));
        assert_stats_equal(bench_alloc_snapshot(), before);
        assert(!bench_adapter.get_u64(map, missing_u64, &value));
        assert(value == UINT64_MAX);
        assert_stats_equal(bench_alloc_snapshot(), before);
        assert(!bench_adapter.get_u64(map, missing_u64, &value));
    } else {
        assert(!bench_adapter.remove_cstr(map, missing_cstr));
        assert_stats_equal(bench_alloc_snapshot(), before);
        assert(!bench_adapter.get_cstr(map, missing_cstr, &value));
        assert(value == UINT64_MAX);
        assert_stats_equal(bench_alloc_snapshot(), before);
        assert(!bench_adapter.get_cstr(map, missing_cstr, &value));
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

static void test_cstr_content_and_lifetime(void)
{
    char retained_key[] = "retained-borrowed-key";
    char stored_key[] = "equal-content-key";
    char lookup_key[] = "equal-content-key";
    bench_alloc_stats before_insert;
    bench_alloc_stats after_insert;
    bench_alloc_stats before_update;
    bench_alloc_stats ended;
    bool inserted = false;
    uint64_t value = 0;
    size_t capacity;
    void *map;

    assert(&stored_key[0] != &lookup_key[0]);
    bench_alloc_reset();
    bench_alloc_begin();
    map = bench_adapter.create(BENCH_KEYS_CSTR, 32u);
    assert(map != NULL);
    assert(bench_adapter.reserve(map, 32u));
    capacity = bench_adapter.capacity(map);
    assert(capacity == 0u || capacity >= 32u);
    assert(bench_adapter.put_cstr(map, retained_key, UINT64_C(17),
                                  &inserted));
    assert(inserted);
    before_insert = bench_alloc_snapshot();
    assert(bench_adapter.put_cstr(map, stored_key, UINT64_C(41), &inserted));
    assert(inserted);
    after_insert = bench_alloc_snapshot();
    assert_stats_equal(after_insert, before_insert);
    assert(bench_adapter.get_cstr(map, lookup_key, &value));
    assert(value == UINT64_C(41));
    before_update = bench_alloc_snapshot();
    assert(bench_adapter.put_cstr(map, lookup_key, UINT64_C(42), &inserted));
    assert(!inserted);
    assert_stats_equal(bench_alloc_snapshot(), before_update);
    assert(bench_adapter.size(map) == 2u);
    assert(bench_adapter.get_cstr(map, stored_key, &value));
    assert(value == UINT64_C(42));
#ifdef STC_TYPED_ADAPTER_TEST
    assert(bench_adapter.capacity(map) == 64u);
#endif
    assert(bench_adapter.reserve(map, 1u));
    assert(bench_adapter.capacity(map) == capacity);
    assert(bench_adapter.reserve(map, 32u));
    assert(bench_adapter.capacity(map) == capacity);
    assert(!bench_adapter.reserve(map, SIZE_MAX));
    assert(bench_adapter.remove_cstr(map, lookup_key));
    assert(bench_adapter.size(map) == 1u);
    bench_adapter.destroy(map);
    assert(strcmp(retained_key, "retained-borrowed-key") == 0);
    assert(strcmp(stored_key, "equal-content-key") == 0);
    assert(strcmp(lookup_key, "equal-content-key") == 0);
    ended = bench_alloc_end();
    assert(ended.current_bytes == 0u);
    assert(ended.allocations != 0u);
}

static void test_u64_contract(void)
{
    bool inserted = true;
    uint64_t value = 0;
    void *map = bench_adapter.create(BENCH_KEYS_U64, 0u);

    assert(map != NULL);
    assert(bench_adapter.reserve(map, 16u));
    assert(!bench_adapter.put_cstr(map, "wrong-kind", 1u, &inserted));
    assert(!inserted);
    assert(bench_adapter.put_u64(map, UINT64_C(2), UINT64_C(7), &inserted));
    assert(inserted);
    assert(bench_adapter.put_u64(map, UINT64_C(2), UINT64_C(9), &inserted));
    assert(!inserted);
    assert(bench_adapter.get_u64(map, UINT64_C(2), &value));
    assert(value == UINT64_C(9));
#ifdef STC_TYPED_ADAPTER_TEST
    assert(bench_adapter.capacity(map) == 32u);
#endif
    assert(bench_adapter.remove_u64(map, UINT64_C(2)));
    assert(!bench_adapter.get_u64(map, UINT64_C(2), &value));
    assert(bench_adapter.size(map) == 0u);
    bench_adapter.destroy(map);
}

static void test_invalid_sizes(void)
{
    assert(bench_adapter.create((bench_key_kind)99, 1u) == NULL);
    assert(bench_adapter.create(BENCH_KEYS_U64, SIZE_MAX) == NULL);
    assert(bench_adapter.create(BENCH_KEYS_CSTR, SIZE_MAX) == NULL);
}

int main(void)
{
    test_empty_lookup_no_allocation(BENCH_KEYS_U64);
    test_empty_lookup_no_allocation(BENCH_KEYS_CSTR);
    test_cstr_content_and_lifetime();
    test_u64_contract();
    test_invalid_sizes();
    return 0;
}
