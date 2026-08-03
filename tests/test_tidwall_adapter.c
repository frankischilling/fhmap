#include "../bench/compare/common/bench_adapter.h"
#include "../bench/compare/common/bench_alloc.h"

#include <assert.h>
#include <float.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
#endif
static size_t pinned_threshold_reference(size_t buckets)
{
    return (size_t)(buckets * (60u / 100.0));
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static void test_threshold_matches_pinned_expression(void)
{
#if FLT_EVAL_METHOD == 0
    static const struct threshold_vector {
        uint64_t buckets;
        uint64_t threshold;
    } vectors[] = {
        {UINT64_C(9007199254740992), UINT64_C(5404319552844595)},
        {UINT64_C(18014398509481984), UINT64_C(10808639105689190)},
        {UINT64_C(36028797018963968), UINT64_C(21617278211378380)},
        {UINT64_C(72057594037927936), UINT64_C(43234556422756760)},
        {UINT64_C(144115188075855872), UINT64_C(86469112845513520)},
        {UINT64_C(288230376151711744), UINT64_C(172938225691027040)},
        {UINT64_C(576460752303423488), UINT64_C(345876451382054080)},
        {UINT64_C(1152921504606846976), UINT64_C(691752902764108160)}
    };
#elif FLT_EVAL_METHOD == 2
    static const struct threshold_vector {
        uint64_t buckets;
        uint64_t threshold;
    } vectors[] = {
        {UINT64_C(9007199254740992), UINT64_C(5404319552844595)},
        {UINT64_C(18014398509481984), UINT64_C(10808639105689190)},
        {UINT64_C(36028797018963968), UINT64_C(21617278211378380)},
        {UINT64_C(72057594037927936), UINT64_C(43234556422756761)},
        {UINT64_C(144115188075855872), UINT64_C(86469112845513523)},
        {UINT64_C(288230376151711744), UINT64_C(172938225691027046)},
        {UINT64_C(576460752303423488), UINT64_C(345876451382054092)},
        {UINT64_C(1152921504606846976), UINT64_C(691752902764108185)}
    };
#endif
    bench_alloc_stats before;
    bench_alloc_stats ended;
    size_t actual;
    size_t buckets;
#if FLT_EVAL_METHOD == 0 || FLT_EVAL_METHOD == 2
    size_t index;
#endif

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
#if FLT_EVAL_METHOD == 0 || FLT_EVAL_METHOD == 2
    if (sizeof(size_t) >= sizeof(uint64_t)) {
        for (index = 0u; index < sizeof(vectors) / sizeof(vectors[0]);
             ++index) {
            assert(tidwall_bench_test_entry_threshold(
                (size_t)vectors[index].buckets, &actual));
            assert(actual == (size_t)vectors[index].threshold);
        }
    }
#endif
    assert_stats_equal(bench_alloc_snapshot(), before);
    ended = bench_alloc_end();
    assert(ended.current_bytes == 0u);
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
    static const struct expected_case {
        size_t entries;
        size_t buckets;
        size_t threshold;
    } cases[] = {
        {9u, 16u, 9u},
        {10u, 32u, 19u},
        {19u, 32u, 19u},
        {20u, 64u, 38u},
        {32u, 64u, 38u}
    };
    size_t index;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        bench_alloc_stats before_reserve;
        bench_alloc_stats ended;
        void *map;

        bench_alloc_reset();
        bench_alloc_begin();
        map = bench_adapter.create(kind, cases[index].entries);
        assert(map != NULL);
        assert(bench_adapter.capacity(map) == cases[index].buckets);
        before_reserve = bench_alloc_snapshot();
        assert(bench_adapter.reserve(map, cases[index].threshold));
        assert(!bench_adapter.reserve(map, cases[index].threshold + 1u));
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
