#include "../common/bench_adapter.h"
#include "../common/bench_alloc.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define uthash_malloc(size) bench_malloc(size)
#define uthash_free(pointer, size) bench_free(pointer)
#include "uthash.h"

typedef struct uthash_u64_record {
    uint64_t key;
    uint64_t value;
    UT_hash_handle hh;
} uthash_u64_record;

typedef struct uthash_cstr_record {
    const char *key;
    uint64_t value;
    UT_hash_handle hh;
} uthash_cstr_record;

typedef struct uthash_bench_map {
    bench_key_kind kind;
    union {
        uthash_u64_record *u64;
        uthash_cstr_record *cstr;
    } head;
} uthash_bench_map;

static uthash_bench_map *uthash_bench_cast(void *map)
{
    return (uthash_bench_map *)map;
}

static const uthash_bench_map *uthash_bench_cast_const(const void *map)
{
    return (const uthash_bench_map *)map;
}

static void *uthash_bench_create(bench_key_kind kind, size_t expected)
{
    uthash_bench_map *map;

    if ((kind != BENCH_KEYS_U64 && kind != BENCH_KEYS_CSTR) ||
        expected == SIZE_MAX) {
        return NULL;
    }
    map = (uthash_bench_map *)bench_malloc(sizeof(*map));
    if (map == NULL) return NULL;
    map->kind = kind;
    map->head.u64 = NULL;
    return map;
}

static bool uthash_bench_reserve(void *raw_map, size_t expected)
{
    uthash_bench_map *map = uthash_bench_cast(raw_map);

    return map != NULL && expected != SIZE_MAX;
}

static bool uthash_bench_put_u64(void *raw_map, uint64_t key,
                                 uint64_t value, bool *inserted)
{
    uthash_bench_map *map = uthash_bench_cast(raw_map);
    uthash_u64_record *record;

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_U64) return false;
    HASH_FIND(hh, map->head.u64, &key, sizeof(key), record);
    if (record != NULL) {
        record->value = value;
        return true;
    }
    record = (uthash_u64_record *)bench_malloc(sizeof(*record));
    if (record == NULL) return false;
    record->key = key;
    record->value = value;
    HASH_ADD(hh, map->head.u64, key, sizeof(record->key), record);
    if (inserted != NULL) *inserted = true;
    return true;
}

static bool uthash_bench_get_u64(void *raw_map, uint64_t key,
                                 uint64_t *value)
{
    uthash_bench_map *map = uthash_bench_cast(raw_map);
    uthash_u64_record *record;

    if (map == NULL || map->kind != BENCH_KEYS_U64 || value == NULL)
        return false;
    HASH_FIND(hh, map->head.u64, &key, sizeof(key), record);
    if (record == NULL) return false;
    *value = record->value;
    return true;
}

static bool uthash_bench_remove_u64(void *raw_map, uint64_t key)
{
    uthash_bench_map *map = uthash_bench_cast(raw_map);
    uthash_u64_record *record;

    if (map == NULL || map->kind != BENCH_KEYS_U64) return false;
    HASH_FIND(hh, map->head.u64, &key, sizeof(key), record);
    if (record == NULL) return false;
    HASH_DEL(map->head.u64, record);
    bench_free(record);
    return true;
}

static bool uthash_bench_cstr_length(const char *key, size_t *length)
{
    size_t found;

    if (key == NULL || length == NULL) return false;
    found = strlen(key);
    if (found > UINT_MAX) return false;
    *length = found;
    return true;
}

static bool uthash_bench_put_cstr(void *raw_map, const char *key,
                                  uint64_t value, bool *inserted)
{
    uthash_bench_map *map = uthash_bench_cast(raw_map);
    uthash_cstr_record *record;
    size_t length;

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_CSTR ||
        !uthash_bench_cstr_length(key, &length)) {
        return false;
    }
    HASH_FIND(hh, map->head.cstr, key, length, record);
    if (record != NULL) {
        record->value = value;
        return true;
    }
    record = (uthash_cstr_record *)bench_malloc(sizeof(*record));
    if (record == NULL) return false;
    record->key = key;
    record->value = value;
    HASH_ADD_KEYPTR(hh, map->head.cstr, record->key, length, record);
    if (inserted != NULL) *inserted = true;
    return true;
}

static bool uthash_bench_get_cstr(void *raw_map, const char *key,
                                  uint64_t *value)
{
    uthash_bench_map *map = uthash_bench_cast(raw_map);
    uthash_cstr_record *record;
    size_t length;

    if (map == NULL || map->kind != BENCH_KEYS_CSTR || value == NULL ||
        !uthash_bench_cstr_length(key, &length)) {
        return false;
    }
    HASH_FIND(hh, map->head.cstr, key, length, record);
    if (record == NULL) return false;
    *value = record->value;
    return true;
}

static bool uthash_bench_remove_cstr(void *raw_map, const char *key)
{
    uthash_bench_map *map = uthash_bench_cast(raw_map);
    uthash_cstr_record *record;
    size_t length;

    if (map == NULL || map->kind != BENCH_KEYS_CSTR ||
        !uthash_bench_cstr_length(key, &length)) {
        return false;
    }
    HASH_FIND(hh, map->head.cstr, key, length, record);
    if (record == NULL) return false;
    HASH_DEL(map->head.cstr, record);
    bench_free(record);
    return true;
}

static bool uthash_bench_iterate(void *raw_map, uint64_t *checksum,
                                 size_t *seen)
{
    uthash_bench_map *map = uthash_bench_cast(raw_map);
    uint64_t result = 0u;
    size_t count = 0u;

    if (map == NULL || checksum == NULL || seen == NULL) return false;
    if (map->kind == BENCH_KEYS_U64) {
        uthash_u64_record *record;
        uthash_u64_record *temporary;

        HASH_ITER(hh, map->head.u64, record, temporary) {
            result = bench_checksum_add(result, record->value);
            ++count;
        }
    } else if (map->kind == BENCH_KEYS_CSTR) {
        uthash_cstr_record *record;
        uthash_cstr_record *temporary;

        HASH_ITER(hh, map->head.cstr, record, temporary) {
            result = bench_checksum_add(result, record->value);
            ++count;
        }
    } else {
        return false;
    }
    *checksum = result;
    *seen = count;
    return true;
}

static size_t uthash_bench_size(const void *raw_map)
{
    const uthash_bench_map *map = uthash_bench_cast_const(raw_map);

    if (map == NULL) return 0u;
    if (map->kind == BENCH_KEYS_U64)
        return (size_t)HASH_COUNT(map->head.u64);
    if (map->kind == BENCH_KEYS_CSTR)
        return (size_t)HASH_COUNT(map->head.cstr);
    return 0u;
}

static size_t uthash_bench_capacity(const void *raw_map)
{
    (void)raw_map;
    return 0u;
}

static void uthash_bench_destroy(void *raw_map)
{
    uthash_bench_map *map = uthash_bench_cast(raw_map);

    if (map == NULL) return;
    if (map->kind == BENCH_KEYS_U64) {
        uthash_u64_record *record;
        uthash_u64_record *temporary;

        HASH_ITER(hh, map->head.u64, record, temporary) {
            HASH_DEL(map->head.u64, record);
            bench_free(record);
        }
    } else if (map->kind == BENCH_KEYS_CSTR) {
        uthash_cstr_record *record;
        uthash_cstr_record *temporary;

        HASH_ITER(hh, map->head.cstr, record, temporary) {
            HASH_DEL(map->head.cstr, record);
            bench_free(record);
        }
    }
    bench_free(map);
}

const bench_map_ops bench_adapter = {
    "uthash",
    uthash_bench_create,
    uthash_bench_reserve,
    uthash_bench_put_u64,
    uthash_bench_get_u64,
    uthash_bench_remove_u64,
    uthash_bench_put_cstr,
    uthash_bench_get_cstr,
    uthash_bench_remove_cstr,
    uthash_bench_iterate,
    uthash_bench_size,
    uthash_bench_capacity,
    uthash_bench_destroy
};
