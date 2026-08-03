#include "../common/bench_adapter.h"
#include "../common/bench_alloc.h"

#define CC_NO_SHORT_NAMES
#define CC_REALLOC bench_realloc
#define CC_FREE bench_free
#include <cc.h>

typedef struct cc_bench_map {
    bench_key_kind kind;
    union {
        cc_map(uint64_t, uint64_t) u64;
        cc_map(const char *, uint64_t) cstr;
    } raw;
} cc_bench_map;

static cc_bench_map *cc_bench_cast(void *map)
{
    return (cc_bench_map *)map;
}

static const cc_bench_map *cc_bench_cast_const(const void *map)
{
    return (const cc_bench_map *)map;
}

static bool cc_bench_reserve(void *raw_map, size_t expected)
{
    cc_bench_map *map = cc_bench_cast(raw_map);

    if (map == NULL) return false;
    if (expected == 0u) return true;
    if (expected > (SIZE_MAX - 64u) / 128u) return false;
    if (map->kind == BENCH_KEYS_U64)
        return cc_reserve(&map->raw.u64, expected);
    if (map->kind == BENCH_KEYS_CSTR)
        return cc_reserve(&map->raw.cstr, expected);
    return false;
}

static void *cc_bench_create(bench_key_kind kind, size_t expected)
{
    cc_bench_map *map;

    if (kind != BENCH_KEYS_U64 && kind != BENCH_KEYS_CSTR) return NULL;
    map = (cc_bench_map *)bench_malloc(sizeof(*map));
    if (map == NULL) return NULL;
    map->kind = kind;
    if (kind == BENCH_KEYS_U64)
        cc_init(&map->raw.u64);
    else
        cc_init(&map->raw.cstr);
    if (expected != 0u && !cc_bench_reserve(map, expected)) {
        if (kind == BENCH_KEYS_U64)
            cc_cleanup(&map->raw.u64);
        else
            cc_cleanup(&map->raw.cstr);
        bench_free(map);
        return NULL;
    }
    return map;
}

static bool cc_bench_put_u64(void *raw_map, uint64_t key, uint64_t value,
                             bool *inserted)
{
    cc_bench_map *map = cc_bench_cast(raw_map);
    size_t old_size;

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_U64) return false;
    old_size = cc_size(&map->raw.u64);
    if (cc_insert(&map->raw.u64, key, value) == NULL) return false;
    if (inserted != NULL) *inserted = cc_size(&map->raw.u64) != old_size;
    return true;
}

static bool cc_bench_get_u64(void *raw_map, uint64_t key, uint64_t *value)
{
    cc_bench_map *map = cc_bench_cast(raw_map);
    const uint64_t *found;

    if (map == NULL || map->kind != BENCH_KEYS_U64 || value == NULL)
        return false;
    found = cc_get(&map->raw.u64, key);
    if (found == NULL) return false;
    *value = *found;
    return true;
}

static bool cc_bench_remove_u64(void *raw_map, uint64_t key)
{
    cc_bench_map *map = cc_bench_cast(raw_map);

    return map != NULL && map->kind == BENCH_KEYS_U64 &&
           cc_erase(&map->raw.u64, key);
}

static bool cc_bench_put_cstr(void *raw_map, const char *key, uint64_t value,
                              bool *inserted)
{
    cc_bench_map *map = cc_bench_cast(raw_map);
    size_t old_size;

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL)
        return false;
    old_size = cc_size(&map->raw.cstr);
    if (cc_insert(&map->raw.cstr, key, value) == NULL) return false;
    if (inserted != NULL) *inserted = cc_size(&map->raw.cstr) != old_size;
    return true;
}

static bool cc_bench_get_cstr(void *raw_map, const char *key, uint64_t *value)
{
    cc_bench_map *map = cc_bench_cast(raw_map);
    const uint64_t *found;

    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL ||
        value == NULL) {
        return false;
    }
    found = cc_get(&map->raw.cstr, key);
    if (found == NULL) return false;
    *value = *found;
    return true;
}

static bool cc_bench_remove_cstr(void *raw_map, const char *key)
{
    cc_bench_map *map = cc_bench_cast(raw_map);

    return map != NULL && map->kind == BENCH_KEYS_CSTR && key != NULL &&
           cc_erase(&map->raw.cstr, key);
}

static bool cc_bench_iterate(void *raw_map, uint64_t *checksum, size_t *seen)
{
    cc_bench_map *map = cc_bench_cast(raw_map);
    uint64_t result = 0;
    size_t count = 0;

    if (map == NULL || checksum == NULL || seen == NULL) return false;
    if (map->kind == BENCH_KEYS_U64) {
        cc_for_each(&map->raw.u64, value)
        {
            result = bench_checksum_add(result, *value);
            ++count;
        }
    } else if (map->kind == BENCH_KEYS_CSTR) {
        cc_for_each(&map->raw.cstr, value)
        {
            result = bench_checksum_add(result, *value);
            ++count;
        }
    } else {
        return false;
    }
    *checksum = result;
    *seen = count;
    return true;
}

static size_t cc_bench_size(const void *raw_map)
{
    const cc_bench_map *map = cc_bench_cast_const(raw_map);

    if (map == NULL) return 0u;
    if (map->kind == BENCH_KEYS_U64) return cc_size(&map->raw.u64);
    if (map->kind == BENCH_KEYS_CSTR) return cc_size(&map->raw.cstr);
    return 0u;
}

static size_t cc_bench_capacity(const void *raw_map)
{
    const cc_bench_map *map = cc_bench_cast_const(raw_map);

    if (map == NULL) return 0u;
    if (map->kind == BENCH_KEYS_U64) return cc_cap(&map->raw.u64);
    if (map->kind == BENCH_KEYS_CSTR) return cc_cap(&map->raw.cstr);
    return 0u;
}

static void cc_bench_destroy(void *raw_map)
{
    cc_bench_map *map = cc_bench_cast(raw_map);

    if (map == NULL) return;
    if (map->kind == BENCH_KEYS_U64)
        cc_cleanup(&map->raw.u64);
    else if (map->kind == BENCH_KEYS_CSTR)
        cc_cleanup(&map->raw.cstr);
    bench_free(map);
}

const bench_map_ops bench_adapter = {
    "cc",
    cc_bench_create,
    cc_bench_reserve,
    cc_bench_put_u64,
    cc_bench_get_u64,
    cc_bench_remove_u64,
    cc_bench_put_cstr,
    cc_bench_get_cstr,
    cc_bench_remove_cstr,
    cc_bench_iterate,
    cc_bench_size,
    cc_bench_capacity,
    cc_bench_destroy
};
