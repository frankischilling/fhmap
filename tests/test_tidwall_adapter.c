#include "../bench/compare/common/bench_adapter.h"
#include "../bench/compare/common/bench_alloc.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static size_t malloc_calls_until_failure;

void *__real_bench_malloc(size_t size);

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
    test_empty_map(BENCH_KEYS_U64);
    test_empty_map(BENCH_KEYS_CSTR);
    test_string_contract();
    test_failed_growth_preserves_map(BENCH_KEYS_U64, 1u);
    test_failed_growth_preserves_map(BENCH_KEYS_U64, 2u);
    test_failed_growth_preserves_map(BENCH_KEYS_CSTR, 1u);
    test_failed_growth_preserves_map(BENCH_KEYS_CSTR, 2u);
    test_wrong_kind_and_invalid_create();
    return 0;
}
