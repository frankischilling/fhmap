#include "../common/bench_adapter.h"
#include "../common/bench_alloc.h"

#include <stddef.h>

static void *stb_ds_bench_realloc(void *context, void *pointer, size_t size)
{
    (void)context;
    return bench_realloc(pointer, size);
}

static void stb_ds_bench_free(void *context, void *pointer)
{
    (void)context;
    bench_free(pointer);
}

#define STBDS_REALLOC(context, pointer, size)                                 \
    stb_ds_bench_realloc((context), (pointer), (size))
#define STBDS_FREE(context, pointer)                                         \
    stb_ds_bench_free((context), (pointer))
#define STB_DS_IMPLEMENTATION
#include <stb_ds.h>

#undef STBDS_ADDRESSOF
#define STBDS_ADDRESSOF(type_variable, value) (&(value))

typedef struct stb_ds_u64_entry {
    uint64_t key;
    uint64_t value;
} stb_ds_u64_entry;

typedef struct stb_ds_cstr_entry {
    char *key;
    uint64_t value;
} stb_ds_cstr_entry;

typedef struct stb_ds_bench_map {
    bench_key_kind kind;
    union {
        stb_ds_u64_entry *u64;
        stb_ds_cstr_entry *cstr;
    } raw;
} stb_ds_bench_map;

static stb_ds_bench_map *stb_ds_bench_cast(void *map)
{
    return (stb_ds_bench_map *)map;
}

static const stb_ds_bench_map *stb_ds_bench_cast_const(const void *map)
{
    return (const stb_ds_bench_map *)map;
}

static size_t stb_ds_bench_max_entries(void)
{
    const size_t u64_entry_size = sizeof(stb_ds_u64_entry);
    const size_t cstr_entry_size = sizeof(stb_ds_cstr_entry);
    const size_t max_entry_size = u64_entry_size > cstr_entry_size
                                      ? u64_entry_size
                                      : cstr_entry_size;
    const size_t pointer_limit = (size_t)PTRDIFF_MAX / 8u;
    const size_t allocation_limit = SIZE_MAX / (16u * max_entry_size);

    return pointer_limit < allocation_limit ? pointer_limit
                                             : allocation_limit;
}

static bool stb_ds_bench_reserve(void *raw_map, size_t expected)
{
    stb_ds_bench_map *map = stb_ds_bench_cast(raw_map);

    if (map == NULL || expected > stb_ds_bench_max_entries()) return false;
    return map->kind == BENCH_KEYS_U64 || map->kind == BENCH_KEYS_CSTR;
}

static void *stb_ds_bench_create(bench_key_kind kind, size_t expected)
{
    stb_ds_bench_map *map;

    if ((kind != BENCH_KEYS_U64 && kind != BENCH_KEYS_CSTR) ||
        expected > stb_ds_bench_max_entries()) {
        return NULL;
    }
    map = (stb_ds_bench_map *)bench_malloc(sizeof(*map));
    if (map == NULL) return NULL;
    map->kind = kind;
    if (kind == BENCH_KEYS_U64)
        map->raw.u64 = NULL;
    else
        map->raw.cstr = NULL;
    return map;
}

static bool stb_ds_bench_put_u64(void *raw_map, uint64_t key,
                                 uint64_t value, bool *inserted)
{
    stb_ds_bench_map *map = stb_ds_bench_cast(raw_map);
    ptrdiff_t old_size;
    ptrdiff_t new_size;

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_U64) return false;
    old_size = hmlen(map->raw.u64);
    if (old_size < 0 || (size_t)old_size >= stb_ds_bench_max_entries())
        return false;
    if (map->raw.u64 == NULL)
        stbds_rand_seed((size_t)UINT32_C(0x6a09e667));
    hmput(map->raw.u64, key, value);
    new_size = hmlen(map->raw.u64);
    if (new_size < old_size) return false;
    if (inserted != NULL) *inserted = new_size != old_size;
    return true;
}

static bool stb_ds_bench_get_u64(void *raw_map, uint64_t key,
                                 uint64_t *value)
{
    stb_ds_bench_map *map = stb_ds_bench_cast(raw_map);
    ptrdiff_t index;

    if (map == NULL || map->kind != BENCH_KEYS_U64 || value == NULL ||
        map->raw.u64 == NULL) {
        return false;
    }
    index = hmgeti(map->raw.u64, key);
    if (index < 0) return false;
    *value = map->raw.u64[index].value;
    return true;
}

static bool stb_ds_bench_remove_u64(void *raw_map, uint64_t key)
{
    stb_ds_bench_map *map = stb_ds_bench_cast(raw_map);

    return map != NULL && map->kind == BENCH_KEYS_U64 &&
           map->raw.u64 != NULL &&
           hmdel(map->raw.u64, key) != 0;
}

static bool stb_ds_bench_put_cstr(void *raw_map, const char *key,
                                  uint64_t value, bool *inserted)
{
    stb_ds_bench_map *map = stb_ds_bench_cast(raw_map);
    char *borrowed_key;
    ptrdiff_t old_size;
    ptrdiff_t new_size;

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL)
        return false;
    old_size = shlen(map->raw.cstr);
    if (old_size < 0 || (size_t)old_size >= stb_ds_bench_max_entries())
        return false;
    borrowed_key = (char *)key;
    if (map->raw.cstr == NULL)
        stbds_rand_seed((size_t)UINT32_C(0x6a09e667));
    shput(map->raw.cstr, borrowed_key, value);
    new_size = shlen(map->raw.cstr);
    if (new_size < old_size) return false;
    if (inserted != NULL) *inserted = new_size != old_size;
    return true;
}

static bool stb_ds_bench_get_cstr(void *raw_map, const char *key,
                                  uint64_t *value)
{
    stb_ds_bench_map *map = stb_ds_bench_cast(raw_map);
    ptrdiff_t index;

    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL ||
        value == NULL || map->raw.cstr == NULL) {
        return false;
    }
    index = shgeti(map->raw.cstr, (char *)key);
    if (index < 0) return false;
    *value = map->raw.cstr[index].value;
    return true;
}

static bool stb_ds_bench_remove_cstr(void *raw_map, const char *key)
{
    stb_ds_bench_map *map = stb_ds_bench_cast(raw_map);

    return map != NULL && map->kind == BENCH_KEYS_CSTR && key != NULL &&
           map->raw.cstr != NULL &&
           shdel(map->raw.cstr, (char *)key) != 0;
}

static bool stb_ds_bench_iterate(void *raw_map, uint64_t *checksum,
                                 size_t *seen)
{
    stb_ds_bench_map *map = stb_ds_bench_cast(raw_map);
    uint64_t result = 0;
    size_t count;
    size_t index;

    if (map == NULL || checksum == NULL || seen == NULL) return false;
    if (map->kind == BENCH_KEYS_U64) {
        count = hmlenu(map->raw.u64);
        for (index = 0; index < count; ++index) {
            result = bench_checksum_add(result, map->raw.u64[index].value);
        }
    } else if (map->kind == BENCH_KEYS_CSTR) {
        count = shlenu(map->raw.cstr);
        for (index = 0; index < count; ++index) {
            result = bench_checksum_add(result, map->raw.cstr[index].value);
        }
    } else {
        return false;
    }
    *checksum = result;
    *seen = count;
    return true;
}

static size_t stb_ds_bench_size(const void *raw_map)
{
    const stb_ds_bench_map *map = stb_ds_bench_cast_const(raw_map);

    if (map == NULL) return 0u;
    if (map->kind == BENCH_KEYS_U64) return hmlenu(map->raw.u64);
    if (map->kind == BENCH_KEYS_CSTR) return shlenu(map->raw.cstr);
    return 0u;
}

static size_t stb_ds_bench_capacity(const void *raw_map)
{
    (void)raw_map;
    return 0u;
}

static void stb_ds_bench_destroy(void *raw_map)
{
    stb_ds_bench_map *map = stb_ds_bench_cast(raw_map);

    if (map == NULL) return;
    if (map->kind == BENCH_KEYS_U64)
        hmfree(map->raw.u64);
    else if (map->kind == BENCH_KEYS_CSTR)
        shfree(map->raw.cstr);
    bench_free(map);
}

const bench_map_ops bench_adapter = {
    "stb_ds",
    stb_ds_bench_create,
    stb_ds_bench_reserve,
    stb_ds_bench_put_u64,
    stb_ds_bench_get_u64,
    stb_ds_bench_remove_u64,
    stb_ds_bench_put_cstr,
    stb_ds_bench_get_cstr,
    stb_ds_bench_remove_cstr,
    stb_ds_bench_iterate,
    stb_ds_bench_size,
    stb_ds_bench_capacity,
    stb_ds_bench_destroy
};
