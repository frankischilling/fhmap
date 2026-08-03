#include "../bench/compare/adapters/stb_ds_adapter.c"

#include <assert.h>

static size_t actual_seed(const void *raw_map, bench_key_kind kind)
{
    const stb_ds_bench_map *map = stb_ds_bench_cast_const(raw_map);
    const stbds_hash_index *table;

    assert(map != NULL);
    assert(map->kind == kind);
    if (kind == BENCH_KEYS_U64) {
        assert(map->raw.u64 != NULL);
        table = stbds_hash_table(map->raw.u64 - 1);
    } else {
        assert(map->raw.cstr != NULL);
        table = stbds_hash_table(map->raw.cstr - 1);
    }
    assert(table != NULL);
    return table->seed;
}

static void put_u64(void *map, uint64_t key, uint64_t value)
{
    bool inserted = false;

    assert(bench_adapter.put_u64(map, key, value, &inserted));
    assert(inserted);
}

static void put_cstr(void *map, const char *key, uint64_t value)
{
    bool inserted = false;

    assert(bench_adapter.put_cstr(map, key, value, &inserted));
    assert(inserted);
}

int main(void)
{
    static const char *const cstr_keys[] = {
        "seed-key-00", "seed-key-01", "seed-key-02", "seed-key-03",
        "seed-key-04", "seed-key-05", "seed-key-06", "seed-key-07",
        "seed-key-08", "seed-key-09", "seed-key-10", "seed-key-11"
    };
    const size_t expected_seed = (size_t)UINT32_C(0x6a09e667);
    bench_alloc_stats ended;
    void *u64_first;
    void *u64_second;
    void *u64_noise;
    void *cstr_first;
    void *cstr_second;
    void *cstr_noise;
    size_t global_after_first;
    size_t index;

    bench_alloc_reset();
    bench_alloc_begin();
    u64_first = bench_adapter.create(BENCH_KEYS_U64, 0u);
    cstr_first = bench_adapter.create(BENCH_KEYS_CSTR, 0u);
    u64_second = bench_adapter.create(BENCH_KEYS_U64, 0u);
    cstr_second = bench_adapter.create(BENCH_KEYS_CSTR, 0u);
    u64_noise = bench_adapter.create(BENCH_KEYS_U64, 0u);
    cstr_noise = bench_adapter.create(BENCH_KEYS_CSTR, 0u);
    assert(u64_first != NULL && u64_second != NULL && u64_noise != NULL);
    assert(cstr_first != NULL && cstr_second != NULL && cstr_noise != NULL);

    put_cstr(cstr_second, cstr_keys[0], UINT64_C(1));
    global_after_first = stbds_hash_seed;
    put_cstr(cstr_second, cstr_keys[1], UINT64_C(2));
    put_cstr(cstr_second, cstr_keys[2], UINT64_C(3));
    assert(stbds_hash_seed == global_after_first);
    put_u64(u64_first, UINT64_C(2), UINT64_C(4));
    for (index = 0u; index < 24u; ++index)
        put_u64(u64_noise, (uint64_t)index * UINT64_C(2) + UINT64_C(100),
                (uint64_t)index);
    for (index = 3u; index < 12u; ++index)
        put_cstr(cstr_noise, cstr_keys[index], (uint64_t)index);
    put_cstr(cstr_first, "seed-key-first", UINT64_C(5));
    for (index = 0u; index < 17u; ++index)
        put_u64(u64_second, (uint64_t)index * UINT64_C(2) + UINT64_C(1000),
                (uint64_t)index);

    assert(actual_seed(u64_first, BENCH_KEYS_U64) == expected_seed);
    assert(actual_seed(u64_second, BENCH_KEYS_U64) == expected_seed);
    assert(actual_seed(cstr_first, BENCH_KEYS_CSTR) == expected_seed);
    assert(actual_seed(cstr_second, BENCH_KEYS_CSTR) == expected_seed);
    assert(actual_seed(u64_noise, BENCH_KEYS_U64) == expected_seed);
    assert(actual_seed(cstr_noise, BENCH_KEYS_CSTR) == expected_seed);

    bench_adapter.destroy(u64_first);
    bench_adapter.destroy(u64_second);
    bench_adapter.destroy(u64_noise);
    bench_adapter.destroy(cstr_first);
    bench_adapter.destroy(cstr_second);
    bench_adapter.destroy(cstr_noise);
    ended = bench_alloc_end();
    assert(ended.current_bytes == 0u);
    return 0;
}
