#include "../common/bench_adapter.h"
#include "../common/bench_alloc.h"

#include <string.h>

#include <stc/common.h>

static void *stc_bench_alloc_malloc(isize_t size)
{
    if (size < 0) return NULL;
    return bench_malloc((size_t)size);
}

static void *stc_bench_alloc_calloc(isize_t count, isize_t element_size)
{
    if (count < 0 || element_size < 0) return NULL;
    return bench_calloc((size_t)count, (size_t)element_size);
}

static void *stc_bench_alloc_realloc(void *pointer, isize_t old_size,
                                     isize_t new_size)
{
    if (old_size < 0 || new_size < 0) return NULL;
    return bench_realloc(pointer, (size_t)new_size);
}

static void stc_bench_alloc_free(void *pointer, isize_t size)
{
    (void)size;
    bench_free(pointer);
}

#define i_type stc_u64_map
#define i_key uint64_t
#define i_val uint64_t
#define i_allocator stc_bench_alloc
#include <stc/hashmap.h>

static size_t stc_bench_cstr_hash(const char *const *key)
{
    return c_hash_str(*key);
}

static bool stc_bench_cstr_equal(const char *const *left,
                                 const char *const *right)
{
    return strcmp(*left, *right) == 0;
}

#define i_type stc_cstr_map
#define i_key const char *
#define i_val uint64_t
#define i_hash stc_bench_cstr_hash
#define i_eq stc_bench_cstr_equal
#define i_allocator stc_bench_alloc
#include <stc/hashmap.h>

typedef struct stc_bench_map {
    bench_key_kind kind;
    union {
        stc_u64_map u64;
        stc_cstr_map cstr;
    } raw;
} stc_bench_map;

static stc_bench_map *stc_bench_cast(void *map)
{
    return (stc_bench_map *)map;
}

static const stc_bench_map *stc_bench_cast_const(const void *map)
{
    return (const stc_bench_map *)map;
}

static size_t stc_bench_max_entries(void)
{
    const size_t u64_value_size = sizeof(stc_u64_map_value);
    const size_t cstr_value_size = sizeof(stc_cstr_map_value);
    const size_t max_value_size = u64_value_size > cstr_value_size
                                      ? u64_value_size
                                      : cstr_value_size;

    return (size_t)PTRDIFF_MAX / (8u * max_value_size);
}

static bool stc_bench_reserve(void *raw_map, size_t expected)
{
    stc_bench_map *map = stc_bench_cast(raw_map);
    isize_t capacity;

    if (map == NULL || expected > stc_bench_max_entries()) return false;
    if (expected == 0u) return true;
    if (map->kind == BENCH_KEYS_U64) {
        capacity = stc_u64_map_capacity(&map->raw.u64);
        return (capacity >= 0 && (size_t)capacity >= expected) ||
               stc_u64_map_reserve(&map->raw.u64, (isize_t)expected);
    }
    if (map->kind == BENCH_KEYS_CSTR) {
        capacity = stc_cstr_map_capacity(&map->raw.cstr);
        return (capacity >= 0 && (size_t)capacity >= expected) ||
               stc_cstr_map_reserve(&map->raw.cstr, (isize_t)expected);
    }
    return false;
}

static void *stc_bench_create(bench_key_kind kind, size_t expected)
{
    stc_bench_map *map;

    if ((kind != BENCH_KEYS_U64 && kind != BENCH_KEYS_CSTR) ||
        expected > stc_bench_max_entries()) {
        return NULL;
    }
    map = (stc_bench_map *)stc_bench_alloc_realloc(
        NULL, 0, (isize_t)sizeof(*map));
    if (map == NULL) return NULL;
    map->kind = kind;
    if (kind == BENCH_KEYS_U64)
        map->raw.u64 = stc_u64_map_init();
    else
        map->raw.cstr = stc_cstr_map_init();
    if (expected != 0u && !stc_bench_reserve(map, expected)) {
        if (kind == BENCH_KEYS_U64)
            stc_u64_map_drop(&map->raw.u64);
        else
            stc_cstr_map_drop(&map->raw.cstr);
        stc_bench_alloc_free(map, (isize_t)sizeof(*map));
        return NULL;
    }
    return map;
}

static bool stc_bench_put_u64(void *raw_map, uint64_t key, uint64_t value,
                              bool *inserted)
{
    stc_bench_map *map = stc_bench_cast(raw_map);
    stc_u64_map_result result;

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_U64 ||
        (size_t)stc_u64_map_size(&map->raw.u64) >=
            stc_bench_max_entries()) {
        return false;
    }
    result = stc_u64_map_put(&map->raw.u64, key, value);
    if (result.ref == NULL) return false;
    if (inserted != NULL) *inserted = result.inserted;
    return true;
}

static bool stc_bench_get_u64(void *raw_map, uint64_t key, uint64_t *value)
{
    stc_bench_map *map = stc_bench_cast(raw_map);
    const stc_u64_map_value *found;

    if (map == NULL || map->kind != BENCH_KEYS_U64 || value == NULL)
        return false;
    found = stc_u64_map_get(&map->raw.u64, key);
    if (found == NULL) return false;
    *value = found->second;
    return true;
}

static bool stc_bench_remove_u64(void *raw_map, uint64_t key)
{
    stc_bench_map *map = stc_bench_cast(raw_map);

    return map != NULL && map->kind == BENCH_KEYS_U64 &&
           stc_u64_map_erase(&map->raw.u64, key) != 0;
}

static bool stc_bench_put_cstr(void *raw_map, const char *key,
                               uint64_t value, bool *inserted)
{
    stc_bench_map *map = stc_bench_cast(raw_map);
    stc_cstr_map_result result;

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL ||
        (size_t)stc_cstr_map_size(&map->raw.cstr) >=
            stc_bench_max_entries()) {
        return false;
    }
    result = stc_cstr_map_put(&map->raw.cstr, key, value);
    if (result.ref == NULL) return false;
    if (inserted != NULL) *inserted = result.inserted;
    return true;
}

static bool stc_bench_get_cstr(void *raw_map, const char *key,
                               uint64_t *value)
{
    stc_bench_map *map = stc_bench_cast(raw_map);
    const stc_cstr_map_value *found;

    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL ||
        value == NULL) {
        return false;
    }
    found = stc_cstr_map_get(&map->raw.cstr, key);
    if (found == NULL) return false;
    *value = found->second;
    return true;
}

static bool stc_bench_remove_cstr(void *raw_map, const char *key)
{
    stc_bench_map *map = stc_bench_cast(raw_map);

    return map != NULL && map->kind == BENCH_KEYS_CSTR && key != NULL &&
           stc_cstr_map_erase(&map->raw.cstr, key) != 0;
}

static bool stc_bench_iterate(void *raw_map, uint64_t *checksum, size_t *seen)
{
    stc_bench_map *map = stc_bench_cast(raw_map);
    uint64_t result = 0;
    size_t count = 0;

    if (map == NULL || checksum == NULL || seen == NULL) return false;
    if (map->kind == BENCH_KEYS_U64) {
        stc_u64_map_iter iterator = stc_u64_map_begin(&map->raw.u64);

        while (iterator.ref != NULL) {
            result = bench_checksum_add(result, iterator.ref->second);
            ++count;
            stc_u64_map_next(&iterator);
        }
    } else if (map->kind == BENCH_KEYS_CSTR) {
        stc_cstr_map_iter iterator = stc_cstr_map_begin(&map->raw.cstr);

        while (iterator.ref != NULL) {
            result = bench_checksum_add(result, iterator.ref->second);
            ++count;
            stc_cstr_map_next(&iterator);
        }
    } else {
        return false;
    }
    *checksum = result;
    *seen = count;
    return true;
}

static size_t stc_bench_size(const void *raw_map)
{
    const stc_bench_map *map = stc_bench_cast_const(raw_map);
    isize_t size;

    if (map == NULL) return 0u;
    if (map->kind == BENCH_KEYS_U64)
        size = stc_u64_map_size(&map->raw.u64);
    else if (map->kind == BENCH_KEYS_CSTR)
        size = stc_cstr_map_size(&map->raw.cstr);
    else
        return 0u;
    return size < 0 ? 0u : (size_t)size;
}

static size_t stc_bench_capacity(const void *raw_map)
{
    stc_bench_map *map = stc_bench_cast((void *)raw_map);
    isize_t bucket_count;

    if (map == NULL) return 0u;
    if (map->kind == BENCH_KEYS_U64)
        bucket_count = stc_u64_map_bucket_count(&map->raw.u64);
    else if (map->kind == BENCH_KEYS_CSTR)
        bucket_count = stc_cstr_map_bucket_count(&map->raw.cstr);
    else
        return 0u;
    if (bucket_count <= 0 || (uintmax_t)bucket_count > (uintmax_t)SIZE_MAX)
        return 0u;
    return (size_t)bucket_count;
}

static void stc_bench_destroy(void *raw_map)
{
    stc_bench_map *map = stc_bench_cast(raw_map);

    if (map == NULL) return;
    if (map->kind == BENCH_KEYS_U64)
        stc_u64_map_drop(&map->raw.u64);
    else if (map->kind == BENCH_KEYS_CSTR)
        stc_cstr_map_drop(&map->raw.cstr);
    stc_bench_alloc_free(map, (isize_t)sizeof(*map));
}

const bench_map_ops bench_adapter = {
    "stc",
    stc_bench_create,
    stc_bench_reserve,
    stc_bench_put_u64,
    stc_bench_get_u64,
    stc_bench_remove_u64,
    stc_bench_put_cstr,
    stc_bench_get_cstr,
    stc_bench_remove_cstr,
    stc_bench_iterate,
    stc_bench_size,
    stc_bench_capacity,
    stc_bench_destroy
};
