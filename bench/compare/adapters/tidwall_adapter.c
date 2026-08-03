#include "../common/bench_adapter.h"
#include "../common/bench_alloc.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hashmap.h"

typedef struct tidwall_u64_item {
    uint64_t key;
    uint64_t value;
} tidwall_u64_item;

typedef struct tidwall_cstr_item {
    const char *key;
    uint64_t value;
} tidwall_cstr_item;

typedef struct tidwall_bench_map {
    bench_key_kind kind;
    struct hashmap *map;
} tidwall_bench_map;

static const uint64_t tidwall_seed_zero =
    UINT64_C(0x243f6a8885a308d3);
static const uint64_t tidwall_seed_one =
    UINT64_C(0x13198a2e03707344);

static tidwall_bench_map *tidwall_bench_cast(void *map)
{
    return (tidwall_bench_map *)map;
}

static const tidwall_bench_map *tidwall_bench_cast_const(const void *map)
{
    return (const tidwall_bench_map *)map;
}

static uint64_t tidwall_u64_hash(const void *item, uint64_t seed_zero,
                                 uint64_t seed_one)
{
    const tidwall_u64_item *entry = (const tidwall_u64_item *)item;

    return hashmap_xxhash3(&entry->key, sizeof(entry->key), seed_zero,
                           seed_one);
}

static int tidwall_u64_compare(const void *left, const void *right,
                               void *user_data)
{
    const tidwall_u64_item *a = (const tidwall_u64_item *)left;
    const tidwall_u64_item *b = (const tidwall_u64_item *)right;

    (void)user_data;
    return a->key == b->key ? 0 : 1;
}

static uint64_t tidwall_cstr_hash(const void *item, uint64_t seed_zero,
                                  uint64_t seed_one)
{
    const tidwall_cstr_item *entry = (const tidwall_cstr_item *)item;

    return hashmap_sip(entry->key, strlen(entry->key), seed_zero, seed_one);
}

static int tidwall_cstr_compare(const void *left, const void *right,
                                void *user_data)
{
    const tidwall_cstr_item *a = (const tidwall_cstr_item *)left;
    const tidwall_cstr_item *b = (const tidwall_cstr_item *)right;

    (void)user_data;
    return strcmp(a->key, b->key);
}

static size_t tidwall_entry_threshold(size_t buckets)
{
    return (buckets / 5u) * 3u + ((buckets % 5u) * 3u) / 5u;
}

static bool tidwall_raw_bucket_request(size_t expected, size_t element_size,
                                       size_t *request)
{
    size_t bucket_size;
    size_t buckets = 16u;
    const size_t alignment = sizeof(uintptr_t);

    if (request == NULL) return false;
    if (element_size > SIZE_MAX - sizeof(uint64_t)) return false;
    bucket_size = sizeof(uint64_t) + element_size;
    if (bucket_size > SIZE_MAX - (alignment - 1u)) return false;
    bucket_size = (bucket_size + alignment - 1u) & ~(alignment - 1u);
    while (tidwall_entry_threshold(buckets) < expected) {
        if (buckets > SIZE_MAX / 2u) return false;
        buckets *= 2u;
    }
    if (buckets > SIZE_MAX / bucket_size) return false;
    *request = buckets;
    return true;
}

static void *tidwall_bench_create(bench_key_kind kind, size_t expected)
{
    uint64_t (*hash)(const void *, uint64_t, uint64_t);
    int (*compare)(const void *, const void *, void *);
    tidwall_bench_map *map;
    size_t element_size;
    size_t raw_buckets;

    if (kind == BENCH_KEYS_U64) {
        element_size = sizeof(tidwall_u64_item);
        hash = tidwall_u64_hash;
        compare = tidwall_u64_compare;
    } else if (kind == BENCH_KEYS_CSTR) {
        element_size = sizeof(tidwall_cstr_item);
        hash = tidwall_cstr_hash;
        compare = tidwall_cstr_compare;
    } else {
        return NULL;
    }
    if (!tidwall_raw_bucket_request(expected, element_size, &raw_buckets))
        return NULL;

    map = (tidwall_bench_map *)bench_malloc(sizeof(*map));
    if (map == NULL) return NULL;
    map->kind = kind;
    map->map = hashmap_new_with_allocator(
        bench_malloc, bench_realloc, bench_free, element_size, raw_buckets,
        tidwall_seed_zero, tidwall_seed_one, hash, compare, NULL, NULL);
    if (map->map == NULL) {
        bench_free(map);
        return NULL;
    }
    return map;
}

static bool tidwall_bench_reserve(void *raw_map, size_t expected)
{
    tidwall_bench_map *map = tidwall_bench_cast(raw_map);
    size_t buckets;

    if (map == NULL) return false;
    if (map->kind != BENCH_KEYS_U64 && map->kind != BENCH_KEYS_CSTR)
        return false;
    buckets = hashmap_nbuckets(map->map);
    return expected <= tidwall_entry_threshold(buckets);
}

static bool tidwall_bench_put_u64(void *raw_map, uint64_t key,
                                  uint64_t value, bool *inserted)
{
    tidwall_bench_map *map = tidwall_bench_cast(raw_map);
    tidwall_u64_item item;
    const void *previous;

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_U64) return false;
    item.key = key;
    item.value = value;
    previous = hashmap_set(map->map, &item);
    if (hashmap_oom(map->map)) return false;
    if (inserted != NULL) *inserted = previous == NULL;
    return true;
}

static bool tidwall_bench_get_u64(void *raw_map, uint64_t key,
                                  uint64_t *value)
{
    tidwall_bench_map *map = tidwall_bench_cast(raw_map);
    tidwall_u64_item query;
    const tidwall_u64_item *found;

    if (map == NULL || map->kind != BENCH_KEYS_U64 || value == NULL)
        return false;
    query.key = key;
    query.value = 0u;
    found = (const tidwall_u64_item *)hashmap_get(map->map, &query);
    if (found == NULL) return false;
    *value = found->value;
    return true;
}

static bool tidwall_bench_remove_u64(void *raw_map, uint64_t key)
{
    tidwall_bench_map *map = tidwall_bench_cast(raw_map);
    tidwall_u64_item query;

    if (map == NULL || map->kind != BENCH_KEYS_U64) return false;
    query.key = key;
    query.value = 0u;
    return hashmap_delete(map->map, &query) != NULL;
}

static bool tidwall_bench_put_cstr(void *raw_map, const char *key,
                                   uint64_t value, bool *inserted)
{
    tidwall_bench_map *map = tidwall_bench_cast(raw_map);
    tidwall_cstr_item item;
    const void *previous;

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL)
        return false;
    item.key = key;
    item.value = value;
    previous = hashmap_set(map->map, &item);
    if (hashmap_oom(map->map)) return false;
    if (inserted != NULL) *inserted = previous == NULL;
    return true;
}

static bool tidwall_bench_get_cstr(void *raw_map, const char *key,
                                   uint64_t *value)
{
    tidwall_bench_map *map = tidwall_bench_cast(raw_map);
    tidwall_cstr_item query;
    const tidwall_cstr_item *found;

    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL ||
        value == NULL) {
        return false;
    }
    query.key = key;
    query.value = 0u;
    found = (const tidwall_cstr_item *)hashmap_get(map->map, &query);
    if (found == NULL) return false;
    *value = found->value;
    return true;
}

static bool tidwall_bench_remove_cstr(void *raw_map, const char *key)
{
    tidwall_bench_map *map = tidwall_bench_cast(raw_map);
    tidwall_cstr_item query;

    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL)
        return false;
    query.key = key;
    query.value = 0u;
    return hashmap_delete(map->map, &query) != NULL;
}

static bool tidwall_bench_iterate(void *raw_map, uint64_t *checksum,
                                  size_t *seen)
{
    tidwall_bench_map *map = tidwall_bench_cast(raw_map);
    uint64_t result = 0u;
    size_t count = 0u;
    size_t iterator = 0u;
    void *item;

    if (map == NULL || checksum == NULL || seen == NULL) return false;
    if (map->kind == BENCH_KEYS_U64) {
        while (hashmap_iter(map->map, &iterator, &item)) {
            const tidwall_u64_item *entry =
                (const tidwall_u64_item *)item;

            result = bench_checksum_add(result, entry->value);
            ++count;
        }
    } else if (map->kind == BENCH_KEYS_CSTR) {
        while (hashmap_iter(map->map, &iterator, &item)) {
            const tidwall_cstr_item *entry =
                (const tidwall_cstr_item *)item;

            result = bench_checksum_add(result, entry->value);
            ++count;
        }
    } else {
        return false;
    }
    *checksum = result;
    *seen = count;
    return true;
}

static size_t tidwall_bench_size(const void *raw_map)
{
    const tidwall_bench_map *map = tidwall_bench_cast_const(raw_map);

    return map == NULL ? 0u : hashmap_count(map->map);
}

static size_t tidwall_bench_capacity(const void *raw_map)
{
    const tidwall_bench_map *map = tidwall_bench_cast_const(raw_map);

    return map == NULL ? 0u : hashmap_nbuckets(map->map);
}

static void tidwall_bench_destroy(void *raw_map)
{
    tidwall_bench_map *map = tidwall_bench_cast(raw_map);

    if (map == NULL) return;
    hashmap_free(map->map);
    bench_free(map);
}

const bench_map_ops bench_adapter = {
    "tidwall",
    tidwall_bench_create,
    tidwall_bench_reserve,
    tidwall_bench_put_u64,
    tidwall_bench_get_u64,
    tidwall_bench_remove_u64,
    tidwall_bench_put_cstr,
    tidwall_bench_get_cstr,
    tidwall_bench_remove_cstr,
    tidwall_bench_iterate,
    tidwall_bench_size,
    tidwall_bench_capacity,
    tidwall_bench_destroy
};
