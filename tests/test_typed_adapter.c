#include "../bench/compare/common/bench_adapter.h"
#include "../bench/compare/common/bench_alloc.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void test_cstr_content_and_lifetime(void)
{
    char stored_key[] = "equal-content-key";
    char lookup_key[] = "equal-content-key";
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
    assert(bench_adapter.put_cstr(map, stored_key, UINT64_C(41), &inserted));
    assert(inserted);
    assert(bench_adapter.get_cstr(map, lookup_key, &value));
    assert(value == UINT64_C(41));
    assert(bench_adapter.put_cstr(map, lookup_key, UINT64_C(42), &inserted));
    assert(!inserted);
    assert(bench_adapter.size(map) == 1u);
    assert(bench_adapter.get_cstr(map, stored_key, &value));
    assert(value == UINT64_C(42));
    assert(bench_adapter.reserve(map, 1u));
    assert(bench_adapter.capacity(map) == capacity);
    assert(bench_adapter.reserve(map, 32u));
    assert(bench_adapter.capacity(map) == capacity);
    assert(!bench_adapter.reserve(map, SIZE_MAX));
    assert(bench_adapter.remove_cstr(map, lookup_key));
    assert(strcmp(stored_key, "equal-content-key") == 0);
    assert(strcmp(lookup_key, "equal-content-key") == 0);
    bench_adapter.destroy(map);
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
    test_cstr_content_and_lifetime();
    test_u64_contract();
    test_invalid_sizes();
    return 0;
}
