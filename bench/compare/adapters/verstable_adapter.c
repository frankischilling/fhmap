#include "../common/bench_adapter.h"
#include "../common/bench_alloc.h"

static void *verstable_bench_malloc(size_t size)
{
    return bench_malloc(size);
}

static void verstable_bench_free(void *pointer, size_t size)
{
    (void)size;
    bench_free(pointer);
}

#define VT_NO_C11_GENERIC_API
#define NAME verstable_u64_map
#define KEY_TY uint64_t
#define VAL_TY uint64_t
#define HASH_FN vt_hash_integer
#define CMPR_FN vt_cmpr_integer
#define MALLOC_FN verstable_bench_malloc
#define FREE_FN verstable_bench_free
#include <verstable.h>

#define NAME verstable_cstr_map
#define KEY_TY const char *
#define VAL_TY uint64_t
#define HASH_FN vt_hash_string
#define CMPR_FN vt_cmpr_string
#define MALLOC_FN verstable_bench_malloc
#define FREE_FN verstable_bench_free
#include <verstable.h>

typedef struct verstable_bench_map {
    bench_key_kind kind;
    union {
        verstable_u64_map u64;
        verstable_cstr_map cstr;
    } raw;
} verstable_bench_map;

static verstable_bench_map *verstable_bench_cast(void *map)
{
    return (verstable_bench_map *)map;
}

static const verstable_bench_map *verstable_bench_cast_const(const void *map)
{
    return (const verstable_bench_map *)map;
}

static bool verstable_bench_expected_is_safe(size_t expected,
                                              size_t bucket_size)
{
    const size_t per_entry_bound = 4u * (bucket_size + sizeof(uint16_t));

    return expected == 0u || expected <= (SIZE_MAX - 8u) / per_entry_bound;
}

static bool verstable_bench_reserve(void *raw_map, size_t expected)
{
    verstable_bench_map *map = verstable_bench_cast(raw_map);

    if (map == NULL) return false;
    if (expected == 0u) return true;
    if (map->kind == BENCH_KEYS_U64) {
        return verstable_bench_expected_is_safe(
                   expected, sizeof(verstable_u64_map_bucket)) &&
               verstable_u64_map_reserve(&map->raw.u64, expected);
    }
    if (map->kind == BENCH_KEYS_CSTR) {
        return verstable_bench_expected_is_safe(
                   expected, sizeof(verstable_cstr_map_bucket)) &&
               verstable_cstr_map_reserve(&map->raw.cstr, expected);
    }
    return false;
}

static void *verstable_bench_create(bench_key_kind kind, size_t expected)
{
    verstable_bench_map *map;

    if (kind != BENCH_KEYS_U64 && kind != BENCH_KEYS_CSTR) return NULL;
    map = (verstable_bench_map *)bench_malloc(sizeof(*map));
    if (map == NULL) return NULL;
    map->kind = kind;
    if (kind == BENCH_KEYS_U64)
        verstable_u64_map_init(&map->raw.u64);
    else
        verstable_cstr_map_init(&map->raw.cstr);
    if (expected != 0u && !verstable_bench_reserve(map, expected)) {
        if (kind == BENCH_KEYS_U64)
            verstable_u64_map_cleanup(&map->raw.u64);
        else
            verstable_cstr_map_cleanup(&map->raw.cstr);
        bench_free(map);
        return NULL;
    }
    return map;
}

static bool verstable_bench_put_u64(void *raw_map, uint64_t key,
                                    uint64_t value, bool *inserted)
{
    verstable_bench_map *map = verstable_bench_cast(raw_map);
    verstable_u64_map_itr iterator;
    size_t old_size;

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_U64) return false;
    old_size = verstable_u64_map_size(&map->raw.u64);
    iterator = verstable_u64_map_insert(&map->raw.u64, key, value);
    if (verstable_u64_map_is_end(iterator)) return false;
    if (inserted != NULL)
        *inserted = verstable_u64_map_size(&map->raw.u64) != old_size;
    return true;
}

static bool verstable_bench_get_u64(void *raw_map, uint64_t key,
                                    uint64_t *value)
{
    verstable_bench_map *map = verstable_bench_cast(raw_map);
    verstable_u64_map_itr iterator;

    if (map == NULL || map->kind != BENCH_KEYS_U64 || value == NULL)
        return false;
    iterator = verstable_u64_map_get(&map->raw.u64, key);
    if (verstable_u64_map_is_end(iterator)) return false;
    *value = iterator.data->val;
    return true;
}

static bool verstable_bench_remove_u64(void *raw_map, uint64_t key)
{
    verstable_bench_map *map = verstable_bench_cast(raw_map);

    return map != NULL && map->kind == BENCH_KEYS_U64 &&
           verstable_u64_map_erase(&map->raw.u64, key);
}

static bool verstable_bench_put_cstr(void *raw_map, const char *key,
                                     uint64_t value, bool *inserted)
{
    verstable_bench_map *map = verstable_bench_cast(raw_map);
    verstable_cstr_map_itr iterator;
    size_t old_size;

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL)
        return false;
    old_size = verstable_cstr_map_size(&map->raw.cstr);
    iterator = verstable_cstr_map_insert(&map->raw.cstr, key, value);
    if (verstable_cstr_map_is_end(iterator)) return false;
    if (inserted != NULL)
        *inserted = verstable_cstr_map_size(&map->raw.cstr) != old_size;
    return true;
}

static bool verstable_bench_get_cstr(void *raw_map, const char *key,
                                     uint64_t *value)
{
    verstable_bench_map *map = verstable_bench_cast(raw_map);
    verstable_cstr_map_itr iterator;

    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL ||
        value == NULL) {
        return false;
    }
    iterator = verstable_cstr_map_get(&map->raw.cstr, key);
    if (verstable_cstr_map_is_end(iterator)) return false;
    *value = iterator.data->val;
    return true;
}

static bool verstable_bench_remove_cstr(void *raw_map, const char *key)
{
    verstable_bench_map *map = verstable_bench_cast(raw_map);

    return map != NULL && map->kind == BENCH_KEYS_CSTR && key != NULL &&
           verstable_cstr_map_erase(&map->raw.cstr, key);
}

static bool verstable_bench_iterate(void *raw_map, uint64_t *checksum,
                                    size_t *seen)
{
    verstable_bench_map *map = verstable_bench_cast(raw_map);
    uint64_t result = 0;
    size_t count = 0;

    if (map == NULL || checksum == NULL || seen == NULL) return false;
    if (map->kind == BENCH_KEYS_U64) {
        verstable_u64_map_itr iterator =
            verstable_u64_map_first(&map->raw.u64);

        while (!verstable_u64_map_is_end(iterator)) {
            result = bench_checksum_add(result, iterator.data->val);
            ++count;
            iterator = verstable_u64_map_next(iterator);
        }
    } else if (map->kind == BENCH_KEYS_CSTR) {
        verstable_cstr_map_itr iterator =
            verstable_cstr_map_first(&map->raw.cstr);

        while (!verstable_cstr_map_is_end(iterator)) {
            result = bench_checksum_add(result, iterator.data->val);
            ++count;
            iterator = verstable_cstr_map_next(iterator);
        }
    } else {
        return false;
    }
    *checksum = result;
    *seen = count;
    return true;
}

static size_t verstable_bench_size(const void *raw_map)
{
    const verstable_bench_map *map = verstable_bench_cast_const(raw_map);

    if (map == NULL) return 0u;
    if (map->kind == BENCH_KEYS_U64)
        return verstable_u64_map_size(&map->raw.u64);
    if (map->kind == BENCH_KEYS_CSTR)
        return verstable_cstr_map_size(&map->raw.cstr);
    return 0u;
}

static size_t verstable_bench_capacity(const void *raw_map)
{
    const verstable_bench_map *map = verstable_bench_cast_const(raw_map);

    if (map == NULL) return 0u;
    if (map->kind == BENCH_KEYS_U64)
        return verstable_u64_map_bucket_count(&map->raw.u64);
    if (map->kind == BENCH_KEYS_CSTR)
        return verstable_cstr_map_bucket_count(&map->raw.cstr);
    return 0u;
}

static void verstable_bench_destroy(void *raw_map)
{
    verstable_bench_map *map = verstable_bench_cast(raw_map);

    if (map == NULL) return;
    if (map->kind == BENCH_KEYS_U64)
        verstable_u64_map_cleanup(&map->raw.u64);
    else if (map->kind == BENCH_KEYS_CSTR)
        verstable_cstr_map_cleanup(&map->raw.cstr);
    bench_free(map);
}

const bench_map_ops bench_adapter = {
    "verstable",
    verstable_bench_create,
    verstable_bench_reserve,
    verstable_bench_put_u64,
    verstable_bench_get_u64,
    verstable_bench_remove_u64,
    verstable_bench_put_cstr,
    verstable_bench_get_cstr,
    verstable_bench_remove_cstr,
    verstable_bench_iterate,
    verstable_bench_size,
    verstable_bench_capacity,
    verstable_bench_destroy
};
