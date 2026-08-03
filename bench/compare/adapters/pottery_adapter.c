#include "../common/bench_adapter.h"
#include "../common/bench_alloc.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "pottery/pottery_dependencies.h"

typedef struct pottery_u64_item {
    uint64_t key;
    uint64_t value;
} pottery_u64_item;

typedef struct pottery_cstr_item {
    const char *key;
    uint64_t value;
} pottery_cstr_item;

static size_t pottery_u64_hash(uint64_t key)
{
    return (size_t)pottery_knuth_hash_u64(
        key, sizeof(size_t) * (size_t)CHAR_BIT);
}

static size_t pottery_cstr_hash(const char *key)
{
    uint32_t hash = UINT32_C(2166136261);

    while (*key != '\0') {
        hash = (hash ^ (uint32_t)(unsigned char)*key) * UINT32_C(16777619);
        ++key;
    }
    return (size_t)hash;
}

static void *pottery_bench_zalloc(size_t size)
{
    return bench_calloc(1u, size);
}

#define POTTERY_OPEN_HASH_MAP_PREFIX pottery_u64_bench
#define POTTERY_OPEN_HASH_MAP_VALUE_TYPE pottery_u64_item
#define POTTERY_OPEN_HASH_MAP_KEY_TYPE uint64_t
#define POTTERY_OPEN_HASH_MAP_REF_KEY(entry) ((entry)->key)
#define POTTERY_OPEN_HASH_MAP_KEY_HASH pottery_u64_hash
#define POTTERY_OPEN_HASH_MAP_KEY_EQUAL(left, right) ((left) == (right))
#define POTTERY_OPEN_HASH_MAP_LIFECYCLE_BY_VALUE 1
#define POTTERY_OPEN_HASH_MAP_ALLOC_MALLOC bench_malloc
#define POTTERY_OPEN_HASH_MAP_ALLOC_ZALLOC pottery_bench_zalloc
#define POTTERY_OPEN_HASH_MAP_ALLOC_FREE bench_free
#define pottery_ohm_entry_destroy pottery_u64_bench_entry_destroy_internal
#include "pottery/open_hash_map/pottery_open_hash_map_static.t.h"
#undef pottery_ohm_entry_destroy

#define POTTERY_OPEN_HASH_MAP_PREFIX pottery_cstr_bench
#define POTTERY_OPEN_HASH_MAP_VALUE_TYPE pottery_cstr_item
#define POTTERY_OPEN_HASH_MAP_KEY_TYPE const char *
#define POTTERY_OPEN_HASH_MAP_REF_KEY(entry) ((entry)->key)
#define POTTERY_OPEN_HASH_MAP_KEY_HASH pottery_cstr_hash
#define POTTERY_OPEN_HASH_MAP_KEY_EQUAL(left, right) \
    (strcmp((left), (right)) == 0)
#define POTTERY_OPEN_HASH_MAP_LIFECYCLE_BY_VALUE 1
#define POTTERY_OPEN_HASH_MAP_ALLOC_MALLOC bench_malloc
#define POTTERY_OPEN_HASH_MAP_ALLOC_ZALLOC pottery_bench_zalloc
#define POTTERY_OPEN_HASH_MAP_ALLOC_FREE bench_free
#define pottery_ohm_entry_destroy pottery_cstr_bench_entry_destroy_internal
#include "pottery/open_hash_map/pottery_open_hash_map_static.t.h"
#undef pottery_ohm_entry_destroy

typedef struct pottery_bench_map {
    bench_key_kind kind;
    union {
        pottery_u64_bench_t u64;
        pottery_cstr_bench_t cstr;
    } map;
} pottery_bench_map;

static pottery_bench_map *pottery_bench_cast(void *map)
{
    return (pottery_bench_map *)map;
}

static const pottery_bench_map *pottery_bench_cast_const(const void *map)
{
    return (const pottery_bench_map *)map;
}

static void *pottery_bench_create(bench_key_kind kind, size_t expected)
{
    pottery_bench_map *map;
    pottery_error_t error;

    if ((kind != BENCH_KEYS_U64 && kind != BENCH_KEYS_CSTR) ||
        expected == SIZE_MAX) {
        return NULL;
    }
    map = (pottery_bench_map *)bench_malloc(sizeof(*map));
    if (map == NULL) return NULL;
    map->kind = kind;
    if (kind == BENCH_KEYS_U64)
        error = pottery_u64_bench_init(&map->map.u64);
    else
        error = pottery_cstr_bench_init(&map->map.cstr);
    if (error != POTTERY_OK) {
        bench_free(map);
        return NULL;
    }
    return map;
}

static bool pottery_bench_reserve(void *raw_map, size_t expected)
{
    pottery_bench_map *map = pottery_bench_cast(raw_map);

    return map != NULL && expected != SIZE_MAX;
}

static bool pottery_bench_put_u64(void *raw_map, uint64_t key,
                                  uint64_t value, bool *inserted)
{
    pottery_bench_map *map = pottery_bench_cast(raw_map);
    pottery_u64_bench_entry_t entry;
    bool created;

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_U64) return false;
    if (pottery_u64_bench_emplace_key(&map->map.u64, key, &entry,
                                      &created) != POTTERY_OK) {
        return false;
    }
    if (created) entry->key = key;
    entry->value = value;
    if (inserted != NULL) *inserted = created;
    return true;
}

static bool pottery_bench_get_u64(void *raw_map, uint64_t key,
                                  uint64_t *value)
{
    pottery_bench_map *map = pottery_bench_cast(raw_map);
    pottery_u64_bench_entry_t entry;

    if (map == NULL || map->kind != BENCH_KEYS_U64 || value == NULL)
        return false;
    entry = pottery_u64_bench_find(&map->map.u64, key);
    if (!pottery_u64_bench_entry_exists(&map->map.u64, entry)) return false;
    *value = entry->value;
    return true;
}

static bool pottery_bench_remove_u64(void *raw_map, uint64_t key)
{
    pottery_bench_map *map = pottery_bench_cast(raw_map);

    if (map == NULL || map->kind != BENCH_KEYS_U64) return false;
    return pottery_u64_bench_remove_key(&map->map.u64, key);
}

static bool pottery_bench_put_cstr(void *raw_map, const char *key,
                                   uint64_t value, bool *inserted)
{
    pottery_bench_map *map = pottery_bench_cast(raw_map);
    pottery_cstr_bench_entry_t entry;
    bool created;

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL)
        return false;
    if (pottery_cstr_bench_emplace_key(&map->map.cstr, key, &entry,
                                       &created) != POTTERY_OK) {
        return false;
    }
    if (created) entry->key = key;
    entry->value = value;
    if (inserted != NULL) *inserted = created;
    return true;
}

static bool pottery_bench_get_cstr(void *raw_map, const char *key,
                                   uint64_t *value)
{
    pottery_bench_map *map = pottery_bench_cast(raw_map);
    pottery_cstr_bench_entry_t entry;

    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL ||
        value == NULL) {
        return false;
    }
    entry = pottery_cstr_bench_find(&map->map.cstr, key);
    if (!pottery_cstr_bench_entry_exists(&map->map.cstr, entry)) return false;
    *value = entry->value;
    return true;
}

static bool pottery_bench_remove_cstr(void *raw_map, const char *key)
{
    pottery_bench_map *map = pottery_bench_cast(raw_map);

    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL)
        return false;
    return pottery_cstr_bench_remove_key(&map->map.cstr, key);
}

static bool pottery_bench_iterate(void *raw_map, uint64_t *checksum,
                                  size_t *seen)
{
    pottery_bench_map *map = pottery_bench_cast(raw_map);
    uint64_t result = 0u;
    size_t count = 0u;

    if (map == NULL || checksum == NULL || seen == NULL) return false;
    if (map->kind == BENCH_KEYS_U64) {
        pottery_u64_bench_entry_t entry =
            pottery_u64_bench_begin(&map->map.u64);

        while (pottery_u64_bench_entry_exists(&map->map.u64, entry)) {
            result = bench_checksum_add(result, entry->value);
            ++count;
            entry = pottery_u64_bench_next(&map->map.u64, entry);
        }
    } else if (map->kind == BENCH_KEYS_CSTR) {
        pottery_cstr_bench_entry_t entry =
            pottery_cstr_bench_begin(&map->map.cstr);

        while (pottery_cstr_bench_entry_exists(&map->map.cstr, entry)) {
            result = bench_checksum_add(result, entry->value);
            ++count;
            entry = pottery_cstr_bench_next(&map->map.cstr, entry);
        }
    } else {
        return false;
    }
    *checksum = result;
    *seen = count;
    return true;
}

static size_t pottery_bench_size(const void *raw_map)
{
    const pottery_bench_map *map = pottery_bench_cast_const(raw_map);

    if (map == NULL) return 0u;
    if (map->kind == BENCH_KEYS_U64)
        return pottery_u64_bench_count((pottery_u64_bench_t *)&map->map.u64);
    if (map->kind == BENCH_KEYS_CSTR)
        return pottery_cstr_bench_count(
            (pottery_cstr_bench_t *)&map->map.cstr);
    return 0u;
}

static size_t pottery_bench_capacity(const void *raw_map)
{
    const pottery_bench_map *map = pottery_bench_cast_const(raw_map);

    if (map == NULL) return 0u;
    if (map->kind == BENCH_KEYS_U64)
        return pottery_u64_bench_capacity(
            (pottery_u64_bench_t *)&map->map.u64);
    if (map->kind == BENCH_KEYS_CSTR)
        return pottery_cstr_bench_capacity(
            (pottery_cstr_bench_t *)&map->map.cstr);
    return 0u;
}

static void pottery_bench_destroy(void *raw_map)
{
    pottery_bench_map *map = pottery_bench_cast(raw_map);

    if (map == NULL) return;
    if (map->kind == BENCH_KEYS_U64)
        pottery_u64_bench_destroy(&map->map.u64);
    else if (map->kind == BENCH_KEYS_CSTR)
        pottery_cstr_bench_destroy(&map->map.cstr);
    bench_free(map);
}

const bench_map_ops bench_adapter = {
    "pottery",
    pottery_bench_create,
    pottery_bench_reserve,
    pottery_bench_put_u64,
    pottery_bench_get_u64,
    pottery_bench_remove_u64,
    pottery_bench_put_cstr,
    pottery_bench_get_cstr,
    pottery_bench_remove_cstr,
    pottery_bench_iterate,
    pottery_bench_size,
    pottery_bench_capacity,
    pottery_bench_destroy
};
