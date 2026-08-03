#include "../common/bench_adapter.h"
#include "../common/bench_alloc.h"

#include <stdlib.h>

#define FHMAP_MALLOC(user, size) bench_malloc(size)
#define FHMAP_FREE(user, pointer) bench_free(pointer)
#define FHMAP_IMPLEMENTATION
#include "../../../fhmap.h"

typedef struct fhmap_bench_map {
    bench_key_kind kind;
    fhmap raw;
#if defined(FHMAP_BENCH_TEST_PERMUTE_LOOKUP_HITS)
    uint64_t *inserted_values;
    size_t expected;
    size_t successful_puts;
    size_t returned_hits;
    bool reserved;
    bool saw_miss;
    bool iterated;
#endif
} fhmap_bench_map;

static fhmap_bench_map *fhmap_bench_cast(void *map)
{
    return (fhmap_bench_map *)map;
}

static const fhmap_bench_map *fhmap_bench_cast_const(const void *map)
{
    return (const fhmap_bench_map *)map;
}

static void *fhmap_bench_create(bench_key_kind kind, size_t expected)
{
    fhmap_bench_map *map;
    fhmap_config config;

    if (kind != BENCH_KEYS_U64 && kind != BENCH_KEYS_CSTR) return NULL;
    map = (fhmap_bench_map *)bench_malloc(sizeof(*map));
    if (map == NULL) return NULL;

    config = fhmap_config_make(kind == BENCH_KEYS_U64
                                   ? sizeof(uint64_t)
                                   : sizeof(const char *),
                               sizeof(uint64_t));
    config.initial_capacity = expected;
    config.key_align = kind == BENCH_KEYS_U64 ? _Alignof(uint64_t)
                                              : _Alignof(const char *);
    config.value_align = _Alignof(uint64_t);
    config.seed = UINT64_C(0x243f6a8885a308d3);
    if (kind == BENCH_KEYS_CSTR) {
        config.hash = fhmap_hash_cstr;
        config.equal = fhmap_equal_cstr;
    }
    if (!fhmap_init(&map->raw, &config)) {
        bench_free(map);
        return NULL;
    }
    map->kind = kind;
#if defined(FHMAP_BENCH_TEST_PERMUTE_LOOKUP_HITS)
    map->inserted_values = expected == 0u
                               ? NULL
                               : (uint64_t *)bench_calloc(
                                     expected, sizeof(*map->inserted_values));
    if (expected != 0u && map->inserted_values == NULL) {
        fhmap_destroy(&map->raw);
        bench_free(map);
        return NULL;
    }
    map->expected = expected;
    map->successful_puts = 0u;
    map->returned_hits = 0u;
    map->reserved = false;
    map->saw_miss = false;
    map->iterated = false;
#endif
    return map;
}

static bool fhmap_bench_reserve(void *raw_map, size_t expected)
{
    fhmap_bench_map *map = fhmap_bench_cast(raw_map);

    if (map == NULL || !fhmap_reserve(&map->raw, expected)) return false;
#if defined(FHMAP_BENCH_TEST_PERMUTE_LOOKUP_HITS)
    map->reserved = true;
#endif
    return true;
}

static bool fhmap_bench_put_result(fhmap_bench_map *map,
                                   fhmap_put_result result, uint64_t value,
                                   bool *inserted)
{
    if (inserted != NULL) *inserted = false;
    if (result == FHMAP_PUT_ERROR) return false;
    if (inserted != NULL) *inserted = result == FHMAP_PUT_INSERTED;
#if defined(FHMAP_BENCH_TEST_PERMUTE_LOOKUP_HITS)
    if (map->successful_puts < map->expected) {
        map->inserted_values[map->successful_puts] = value;
    }
    if (map->successful_puts != SIZE_MAX) ++map->successful_puts;
#else
    (void)map;
    (void)value;
#endif
    return result == FHMAP_PUT_INSERTED || result == FHMAP_PUT_UPDATED;
}

static void fhmap_bench_adjust_test_hit(fhmap_bench_map *map,
                                        uint64_t *value)
{
#if defined(FHMAP_BENCH_TEST_PERMUTE_LOOKUP_HITS)
    if (map->expected != 0u && !map->reserved && !map->saw_miss &&
        !map->iterated && map->successful_puts == map->expected &&
        map->returned_hits < map->expected) {
        size_t paired = map->returned_hits ^ 1u;

        if (paired >= map->expected) paired = map->returned_hits;
        *value = map->inserted_values[paired];
        ++map->returned_hits;
    }
#else
    (void)map;
    (void)value;
#endif
}

static void fhmap_bench_record_test_miss(fhmap_bench_map *map)
{
#if defined(FHMAP_BENCH_TEST_PERMUTE_LOOKUP_HITS)
    map->saw_miss = true;
#else
    (void)map;
#endif
}

static bool fhmap_bench_put_u64(void *raw_map, uint64_t key,
                                uint64_t value, bool *inserted)
{
    fhmap_bench_map *map = fhmap_bench_cast(raw_map);

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_U64) return false;
    return fhmap_bench_put_result(map, fhmap_put(&map->raw, &key, &value),
                                  value, inserted);
}

static bool fhmap_bench_get_u64(void *raw_map, uint64_t key, uint64_t *value)
{
    fhmap_bench_map *map = fhmap_bench_cast(raw_map);
    const uint64_t *found;

    if (map == NULL || map->kind != BENCH_KEYS_U64 || value == NULL)
        return false;
    found = (const uint64_t *)fhmap_get_const(&map->raw, &key);
    if (found == NULL) {
        fhmap_bench_record_test_miss(map);
        return false;
    }
    *value = *found;
    fhmap_bench_adjust_test_hit(map, value);
    return true;
}

static bool fhmap_bench_remove_u64(void *raw_map, uint64_t key)
{
    fhmap_bench_map *map = fhmap_bench_cast(raw_map);

    return map != NULL && map->kind == BENCH_KEYS_U64 &&
           fhmap_remove(&map->raw, &key);
}

static bool fhmap_bench_put_cstr(void *raw_map, const char *key,
                                 uint64_t value, bool *inserted)
{
    fhmap_bench_map *map = fhmap_bench_cast(raw_map);

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL)
        return false;
    return fhmap_bench_put_result(map, fhmap_put(&map->raw, &key, &value),
                                  value, inserted);
}

static bool fhmap_bench_get_cstr(void *raw_map, const char *key,
                                 uint64_t *value)
{
    fhmap_bench_map *map = fhmap_bench_cast(raw_map);
    const uint64_t *found;

    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL ||
        value == NULL) {
        return false;
    }
    found = (const uint64_t *)fhmap_get_const(&map->raw, &key);
    if (found == NULL) {
        fhmap_bench_record_test_miss(map);
        return false;
    }
    *value = *found;
    fhmap_bench_adjust_test_hit(map, value);
    return true;
}

static bool fhmap_bench_remove_cstr(void *raw_map, const char *key)
{
    fhmap_bench_map *map = fhmap_bench_cast(raw_map);

    return map != NULL && map->kind == BENCH_KEYS_CSTR && key != NULL &&
           fhmap_remove(&map->raw, &key);
}

static bool fhmap_bench_iterate(void *raw_map, uint64_t *checksum,
                                size_t *seen)
{
    fhmap_bench_map *map = fhmap_bench_cast(raw_map);
    fhmap_iter iterator;
    const void *key;
    void *value;
    uint64_t result = 0;
    size_t count = 0;

    if (map == NULL || checksum == NULL || seen == NULL) return false;
#if defined(FHMAP_BENCH_TEST_PERMUTE_LOOKUP_HITS)
    map->iterated = true;
#endif
    iterator = fhmap_iter_begin(&map->raw);
    while (fhmap_iter_next(&map->raw, &iterator, &key, &value)) {
        if (key == NULL || value == NULL) return false;
        result = bench_checksum_add(result, *(const uint64_t *)value);
        ++count;
    }
    *checksum = result;
    *seen = count;
    return true;
}

static size_t fhmap_bench_size(const void *raw_map)
{
    const fhmap_bench_map *map = fhmap_bench_cast_const(raw_map);

    return map == NULL ? 0u : fhmap_size(&map->raw);
}

static size_t fhmap_bench_capacity(const void *raw_map)
{
    const fhmap_bench_map *map = fhmap_bench_cast_const(raw_map);

    return map == NULL ? 0u : fhmap_capacity(&map->raw);
}

static void fhmap_bench_destroy(void *raw_map)
{
    fhmap_bench_map *map = fhmap_bench_cast(raw_map);

    if (map == NULL) return;
#if defined(FHMAP_BENCH_TEST_PERMUTE_LOOKUP_HITS)
    bench_free(map->inserted_values);
#endif
    fhmap_destroy(&map->raw);
    bench_free(map);
}

const bench_map_ops bench_adapter = {
    "fhmap",
    fhmap_bench_create,
    fhmap_bench_reserve,
    fhmap_bench_put_u64,
    fhmap_bench_get_u64,
    fhmap_bench_remove_u64,
    fhmap_bench_put_cstr,
    fhmap_bench_get_cstr,
    fhmap_bench_remove_cstr,
    fhmap_bench_iterate,
    fhmap_bench_size,
    fhmap_bench_capacity,
    fhmap_bench_destroy
};
