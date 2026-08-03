#include "../bench/compare/common/bench_adapter.h"
#include "../bench/compare/common/bench_alloc.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hashmap.h"

static size_t malloc_calls_until_failure;

void *__real_bench_malloc(size_t size);
bool tidwall_bench_test_entry_threshold(size_t buckets, size_t *threshold);

void *__wrap_bench_malloc(size_t size)
{
    if (malloc_calls_until_failure != 0u) {
        --malloc_calls_until_failure;
        if (malloc_calls_until_failure == 0u) return NULL;
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

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif
static double pinned_clamp_load_factor_reference(double factor,
                                                 double default_factor)
{
    return factor != factor ? default_factor
                            : factor < 0.50 ? 0.50
                                            : factor > 0.95 ? 0.95 : factor;
}

static size_t pinned_threshold_reference(size_t buckets)
{
    uint8_t loadfactor;

    loadfactor =
        pinned_clamp_load_factor_reference(0.60, 0.60) * 100;
    return (size_t)(buckets * (loadfactor / 100.0));
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static void test_threshold_matches_pinned_expression(void)
{
    bench_alloc_stats before;
    bench_alloc_stats ended;
    size_t actual;
    size_t buckets;

    bench_alloc_reset();
    bench_alloc_begin();
    before = bench_alloc_snapshot();
    buckets = 16u;
    for (;;) {
        assert(tidwall_bench_test_entry_threshold(buckets, &actual));
        assert(actual == pinned_threshold_reference(buckets));
        if (buckets > SIZE_MAX / 2u) break;
        buckets *= 2u;
    }
    assert_stats_equal(bench_alloc_snapshot(), before);
    ended = bench_alloc_end();
    assert(ended.current_bytes == 0u);
}

typedef struct pinned_boundary_item {
    uint64_t key;
} pinned_boundary_item;

static uint64_t pinned_boundary_hash(const void *raw_item, uint64_t seed_zero,
                                     uint64_t seed_one)
{
    const pinned_boundary_item *item =
        (const pinned_boundary_item *)raw_item;

    return hashmap_xxhash3(&item->key, sizeof(item->key), seed_zero,
                           seed_one);
}

static int pinned_boundary_compare(const void *raw_left,
                                   const void *raw_right, void *user_data)
{
    const pinned_boundary_item *left =
        (const pinned_boundary_item *)raw_left;
    const pinned_boundary_item *right =
        (const pinned_boundary_item *)raw_right;

    (void)user_data;
    return left->key == right->key ? 0 : 1;
}

static void test_pinned_resize_boundary(void)
{
    bench_alloc_stats ended;
    pinned_boundary_item item;
    struct hashmap *map;
    size_t index;
    size_t threshold;

    bench_alloc_reset();
    bench_alloc_begin();
    assert(tidwall_bench_test_entry_threshold(32u, &threshold));
    map = hashmap_new_with_allocator(
        bench_malloc, bench_realloc, bench_free, sizeof(item), 32u,
        UINT64_C(0x243f6a8885a308d3), UINT64_C(0x13198a2e03707344),
        pinned_boundary_hash, pinned_boundary_compare, NULL, NULL);
    assert(map != NULL);
    assert(hashmap_nbuckets(map) == 32u);
    for (index = 0u; index < threshold; ++index) {
        item.key = (uint64_t)index;
        assert(hashmap_set(map, &item) == NULL);
        assert(!hashmap_oom(map));
        assert(hashmap_nbuckets(map) == 32u);
    }
    item.key = (uint64_t)threshold;
    assert(hashmap_set(map, &item) == NULL);
    assert(!hashmap_oom(map));
    assert(hashmap_nbuckets(map) == 64u);
    assert(hashmap_count(map) == threshold + 1u);
    hashmap_free(map);
    ended = bench_alloc_end();
    assert(ended.current_bytes == 0u);
    assert(ended.allocations == ended.frees);
}

static void test_empty_map(bench_key_kind kind)
{
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
    assert(bench_adapter.capacity(map) >= 32u);
    before = bench_alloc_snapshot();
    assert(bench_adapter.reserve(map, 32u));
    assert(bench_adapter.reserve(map, 32u));
    assert(bench_adapter.reserve(map, 1u));
    assert(!bench_adapter.reserve(map, SIZE_MAX));
    if (kind == BENCH_KEYS_U64) {
        assert(!bench_adapter.get_u64(map, UINT64_C(7), &value));
        assert(!bench_adapter.remove_u64(map, UINT64_C(7)));
    } else {
        assert(!bench_adapter.get_cstr(map, "missing", &value));
        assert(!bench_adapter.remove_cstr(map, "missing"));
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

static void test_string_contract(void)
{
    char retained[] = "retained-borrowed";
    char stored[] = "same-content";
    char lookup[] = "same-content";
    bench_alloc_stats before_insert;
    bench_alloc_stats before_update;
    bench_alloc_stats ended;
    bool inserted = false;
    uint64_t checksum = 0u;
    uint64_t value = 0u;
    size_t seen = 0u;
    size_t capacity;
    void *map;

    assert(&stored[0] != &lookup[0]);
    bench_alloc_reset();
    bench_alloc_begin();
    map = bench_adapter.create(BENCH_KEYS_CSTR, 32u);
    assert(map != NULL);
    capacity = bench_adapter.capacity(map);
    before_insert = bench_alloc_snapshot();
    assert(bench_adapter.put_cstr(map, retained, UINT64_C(17), &inserted));
    assert(inserted);
    assert(bench_adapter.put_cstr(map, stored, UINT64_C(41), &inserted));
    assert(inserted);
    assert_stats_equal(bench_alloc_snapshot(), before_insert);
    assert(bench_adapter.get_cstr(map, lookup, &value));
    assert(value == UINT64_C(41));
    before_update = bench_alloc_snapshot();
    assert(bench_adapter.put_cstr(map, lookup, UINT64_C(42), &inserted));
    assert(!inserted);
    assert_stats_equal(bench_alloc_snapshot(), before_update);
    assert(bench_adapter.capacity(map) == capacity);
    assert(bench_adapter.size(map) == 2u);
    assert(bench_adapter.iterate(map, &checksum, &seen));
    assert(seen == 2u);
    assert(checksum == bench_checksum_add(
                           bench_checksum_add(0u, UINT64_C(17)),
                           UINT64_C(42)));
    assert(bench_adapter.remove_cstr(map, lookup));
    assert(!bench_adapter.get_cstr(map, stored, &value));
    bench_adapter.destroy(map);
    assert(strcmp(retained, "retained-borrowed") == 0);
    assert(strcmp(stored, "same-content") == 0);
    assert(strcmp(lookup, "same-content") == 0);
    ended = bench_alloc_end();
    assert(ended.current_bytes == 0u);
}

static bool put_key(void *map, bench_key_kind kind, uint64_t key,
                    const char *string_key, uint64_t value, bool *inserted)
{
    if (kind == BENCH_KEYS_U64) {
        return bench_adapter.put_u64(map, key, value, inserted);
    }
    return bench_adapter.put_cstr(map, string_key, value, inserted);
}

static bool get_key(void *map, bench_key_kind kind, uint64_t key,
                    const char *string_key, uint64_t *value)
{
    if (kind == BENCH_KEYS_U64) {
        return bench_adapter.get_u64(map, key, value);
    }
    return bench_adapter.get_cstr(map, string_key, value);
}

static void test_reserved_insert_does_not_grow(bench_key_kind kind)
{
    enum { entry_count = 32, expected_buckets = 64 };
    char keys[entry_count][24];
    bench_alloc_stats constructor_stats;
    bench_alloc_stats ended;
    bool inserted = false;
    uint64_t value = 0u;
    size_t index;
    void *map;

    for (index = 0u; index < entry_count; ++index) {
        assert(snprintf(keys[index], sizeof(keys[index]), "key-%zu", index) >
               0);
    }
    bench_alloc_reset();
    bench_alloc_begin();
    map = bench_adapter.create(kind, entry_count);
    assert(map != NULL);
    assert(bench_adapter.capacity(map) == expected_buckets);
    constructor_stats = bench_alloc_snapshot();
    assert(bench_adapter.reserve(map, entry_count));
    assert_stats_equal(bench_alloc_snapshot(), constructor_stats);
    for (index = 0u; index < entry_count; ++index) {
        assert(put_key(map, kind, (uint64_t)index, keys[index],
                       (uint64_t)(index + 100u), &inserted));
        assert(inserted);
        assert(bench_adapter.capacity(map) == expected_buckets);
        assert_stats_equal(bench_alloc_snapshot(), constructor_stats);
    }
    assert(bench_adapter.size(map) == entry_count);
    for (index = 0u; index < entry_count; ++index) {
        assert(get_key(map, kind, (uint64_t)index, keys[index], &value));
        assert(value == (uint64_t)(index + 100u));
    }
    bench_adapter.destroy(map);
    ended = bench_alloc_end();
    assert(ended.current_bytes == 0u);
    assert(ended.allocations == ended.frees);
}

static void test_expected_entry_boundaries(bench_key_kind kind)
{
    static const size_t entries[] = {9u, 10u, 19u, 20u, 32u};
    size_t index;

    for (index = 0u; index < sizeof(entries) / sizeof(entries[0]); ++index) {
        bench_alloc_stats before_reserve;
        bench_alloc_stats ended;
        size_t buckets = 16u;
        size_t threshold;
        void *map;

        while (pinned_threshold_reference(buckets) < entries[index]) {
            assert(buckets <= SIZE_MAX / 2u);
            buckets *= 2u;
        }
        threshold = pinned_threshold_reference(buckets);
        bench_alloc_reset();
        bench_alloc_begin();
        map = bench_adapter.create(kind, entries[index]);
        assert(map != NULL);
        assert(bench_adapter.capacity(map) == buckets);
        before_reserve = bench_alloc_snapshot();
        assert(bench_adapter.reserve(map, threshold));
        assert(!bench_adapter.reserve(map, threshold + 1u));
        assert_stats_equal(bench_alloc_snapshot(), before_reserve);
        bench_adapter.destroy(map);
        ended = bench_alloc_end();
        assert(ended.current_bytes == 0u);
        assert(ended.allocations == ended.frees);
    }
}

static void test_failed_growth_preserves_map(bench_key_kind kind,
                                             size_t fail_on_call)
{
    char keys[10][24];
    bench_alloc_stats after_failure;
    bench_alloc_stats before_failure;
    bench_alloc_stats ended;
    bool inserted;
    uint64_t checksum = 0u;
    uint64_t expected_checksum = 0u;
    uint64_t value = 0u;
    size_t capacity;
    size_t index;
    size_t seen = 0u;
    void *map;

    assert(fail_on_call == 1u || fail_on_call == 2u);
    for (index = 0u; index < 10u; ++index) {
        assert(snprintf(keys[index], sizeof(keys[index]), "key-%zu", index) >
               0);
    }
    bench_alloc_reset();
    bench_alloc_begin();
    map = bench_adapter.create(kind, 0u);
    assert(map != NULL);
    for (index = 0u; index < 9u; ++index) {
        assert(put_key(map, kind, (uint64_t)index, keys[index],
                       (uint64_t)(index + 100u), &inserted));
        assert(inserted);
        expected_checksum = bench_checksum_add(
            expected_checksum, (uint64_t)(index + 100u));
    }
    assert(bench_adapter.size(map) == 9u);
    capacity = bench_adapter.capacity(map);
    before_failure = bench_alloc_snapshot();
    inserted = true;
    malloc_calls_until_failure = fail_on_call;
    assert(!put_key(map, kind, UINT64_C(9), keys[9], UINT64_C(109),
                    &inserted));
    assert(malloc_calls_until_failure == 0u);
    assert(!inserted);
    after_failure = bench_alloc_snapshot();
    assert(after_failure.current_bytes == before_failure.current_bytes);
    if (fail_on_call == 1u) {
        assert_stats_equal(after_failure, before_failure);
    } else {
        assert(after_failure.allocations == before_failure.allocations + 1u);
        assert(after_failure.frees == before_failure.frees + 1u);
    }
    assert(bench_adapter.size(map) == 9u);
    assert(bench_adapter.capacity(map) == capacity);
    for (index = 0u; index < 9u; ++index) {
        assert(get_key(map, kind, (uint64_t)index, keys[index], &value));
        assert(value == (uint64_t)(index + 100u));
    }
    assert(!get_key(map, kind, UINT64_C(9), keys[9], &value));
    assert(bench_adapter.iterate(map, &checksum, &seen));
    assert(seen == 9u);
    assert(checksum == expected_checksum);
    assert(put_key(map, kind, UINT64_C(9), keys[9], UINT64_C(109),
                   &inserted));
    assert(inserted);
    assert(bench_adapter.size(map) == 10u);
    assert(get_key(map, kind, UINT64_C(9), keys[9], &value));
    assert(value == UINT64_C(109));
    bench_adapter.destroy(map);
    ended = bench_alloc_end();
    assert(ended.current_bytes == 0u);
    assert(ended.allocations == ended.frees);
}

static void test_wrong_kind_and_invalid_create(void)
{
    bool inserted = true;
    uint64_t value = 0u;
    void *map = bench_adapter.create(BENCH_KEYS_U64, 0u);

    assert(map != NULL);
    assert(!bench_adapter.put_cstr(map, "wrong", UINT64_C(1), &inserted));
    assert(!inserted);
    assert(!bench_adapter.get_cstr(map, "wrong", &value));
    assert(!bench_adapter.remove_cstr(map, "wrong"));
    bench_adapter.destroy(map);
    assert(bench_adapter.create((bench_key_kind)99, 0u) == NULL);
    assert(bench_adapter.create(BENCH_KEYS_U64, SIZE_MAX) == NULL);
    assert(bench_adapter.create(BENCH_KEYS_CSTR, SIZE_MAX) == NULL);
}

int main(void)
{
    assert(strcmp(bench_adapter.name, "tidwall") == 0);
    test_pinned_resize_boundary();
    test_threshold_matches_pinned_expression();
    test_empty_map(BENCH_KEYS_U64);
    test_empty_map(BENCH_KEYS_CSTR);
    test_string_contract();
    test_reserved_insert_does_not_grow(BENCH_KEYS_U64);
    test_reserved_insert_does_not_grow(BENCH_KEYS_CSTR);
    test_expected_entry_boundaries(BENCH_KEYS_U64);
    test_expected_entry_boundaries(BENCH_KEYS_CSTR);
    test_failed_growth_preserves_map(BENCH_KEYS_U64, 1u);
    test_failed_growth_preserves_map(BENCH_KEYS_U64, 2u);
    test_failed_growth_preserves_map(BENCH_KEYS_CSTR, 1u);
    test_failed_growth_preserves_map(BENCH_KEYS_CSTR, 2u);
    test_wrong_kind_and_invalid_create();
    return 0;
}
