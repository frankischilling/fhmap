#include "../common/bench_adapter.h"
#include "../common/bench_alloc.h"

#include <string.h>

static void *mlib_bench_allocate(size_t size)
{
    return bench_malloc(size);
}

static void *mlib_bench_reallocate(void *pointer, size_t element_size,
                                   size_t old_count, size_t new_count)
{
    (void)old_count;
    if (new_count != 0u && element_size > SIZE_MAX / new_count) return NULL;
    return bench_realloc(pointer, new_count * element_size);
}

static void mlib_bench_deallocate(void *pointer)
{
    bench_free(pointer);
}

static void mlib_bench_deallocate_array(void *pointer, size_t element_size,
                                        size_t old_count)
{
    (void)element_size;
    (void)old_count;
    bench_free(pointer);
}

#define M_MEMORY_ALLOC(context, type)                                        \
    ((type *)mlib_bench_allocate(sizeof(type)))
#define M_MEMORY_DEL(context, pointer)                                       \
    mlib_bench_deallocate(pointer)
#define M_MEMORY_REALLOC(context, type, pointer, old_count, new_count)        \
    ((type *)mlib_bench_reallocate((pointer), sizeof(type),                   \
                                    (size_t)(old_count),                       \
                                    (size_t)(new_count)))
#define M_MEMORY_FREE(context, type, pointer, old_count)                      \
    mlib_bench_deallocate_array((pointer), sizeof(type),                      \
                                (size_t)(old_count))
#include <m-dict.h>

static bool mlib_u64_oor_equal(uint64_t key, unsigned char marker)
{
    return key == UINT64_MAX - (uint64_t)marker * UINT64_C(2);
}

static void mlib_u64_oor_set(uint64_t *key, unsigned char marker)
{
    *key = UINT64_MAX - (uint64_t)marker * UINT64_C(2);
}

static const char mlib_cstr_empty[] = "\001";
static const char mlib_cstr_deleted[] = "\002";
static const char *const mlib_cstr_oor[] = {
    mlib_cstr_empty,
    mlib_cstr_deleted
};

static bool mlib_cstr_equal(const char *left, const char *right)
{
    if (left == right) return true;
    if (left == mlib_cstr_oor[0] || left == mlib_cstr_oor[1] ||
        right == mlib_cstr_oor[0] || right == mlib_cstr_oor[1]) {
        return false;
    }
    return strcmp(left, right) == 0;
}

static bool mlib_cstr_oor_equal(const char *key, unsigned char marker)
{
    return key == mlib_cstr_oor[marker];
}

static void mlib_cstr_oor_set(const char **key, unsigned char marker)
{
    *key = mlib_cstr_oor[marker];
}

DICT_OA_DEF2(
    mlib_u64_map, uint64_t,
    M_OPEXTEND(M_BASIC_OPLIST, OOR_EQUAL(mlib_u64_oor_equal),
               OOR_SET(API_2(mlib_u64_oor_set))),
    uint64_t, M_BASIC_OPLIST)

DICT_OA_DEF2(
    mlib_cstr_map, const char *,
    M_OPEXTEND(M_CSTR_OPLIST, EQUAL(mlib_cstr_equal),
               OOR_EQUAL(mlib_cstr_oor_equal),
               OOR_SET(API_2(mlib_cstr_oor_set))),
    uint64_t, M_BASIC_OPLIST)

typedef struct mlib_bench_map {
    bench_key_kind kind;
    union {
        mlib_u64_map_t u64;
        mlib_cstr_map_t cstr;
    } raw;
} mlib_bench_map;

static mlib_bench_map *mlib_bench_cast(void *map)
{
    return (mlib_bench_map *)map;
}

static const mlib_bench_map *mlib_bench_cast_const(const void *map)
{
    return (const mlib_bench_map *)map;
}

static bool mlib_bench_expected_is_safe(size_t expected)
{
    const size_t u64_pair_size = sizeof(mlib_u64_map_pair_ct);
    const size_t cstr_pair_size = sizeof(mlib_cstr_map_pair_ct);
    const size_t max_pair_size =
        u64_pair_size > cstr_pair_size ? u64_pair_size : cstr_pair_size;

    return expected <= SIZE_MAX / (8u * max_pair_size);
}

static bool mlib_bench_reserve(void *raw_map, size_t expected)
{
    mlib_bench_map *map = mlib_bench_cast(raw_map);

    if (map == NULL || !mlib_bench_expected_is_safe(expected)) return false;
    if (expected == 0u) return true;
    if (map->kind == BENCH_KEYS_U64) {
        mlib_u64_map_reserve(map->raw.u64, expected);
        return true;
    }
    if (map->kind == BENCH_KEYS_CSTR) {
        mlib_cstr_map_reserve(map->raw.cstr, expected);
        return true;
    }
    return false;
}

static void *mlib_bench_create(bench_key_kind kind, size_t expected)
{
    mlib_bench_map *map;

    if ((kind != BENCH_KEYS_U64 && kind != BENCH_KEYS_CSTR) ||
        !mlib_bench_expected_is_safe(expected)) {
        return NULL;
    }
    map = (mlib_bench_map *)mlib_bench_allocate(sizeof(*map));
    if (map == NULL) return NULL;
    map->kind = kind;
    if (kind == BENCH_KEYS_U64)
        mlib_u64_map_init(map->raw.u64);
    else
        mlib_cstr_map_init(map->raw.cstr);
    if (expected != 0u && !mlib_bench_reserve(map, expected)) {
        if (kind == BENCH_KEYS_U64)
            mlib_u64_map_clear(map->raw.u64);
        else
            mlib_cstr_map_clear(map->raw.cstr);
        mlib_bench_deallocate(map);
        return NULL;
    }
    return map;
}

static bool mlib_bench_put_u64(void *raw_map, uint64_t key, uint64_t value,
                               bool *inserted)
{
    mlib_bench_map *map = mlib_bench_cast(raw_map);
    size_t old_size;

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_U64 ||
        mlib_u64_oor_equal(key, 0u) || mlib_u64_oor_equal(key, 1u)) {
        return false;
    }
    old_size = mlib_u64_map_size(map->raw.u64);
    if (old_size >= SIZE_MAX / (8u * sizeof(mlib_u64_map_pair_ct)))
        return false;
    mlib_u64_map_set_at(map->raw.u64, key, value);
    if (inserted != NULL)
        *inserted = mlib_u64_map_size(map->raw.u64) != old_size;
    return true;
}

static bool mlib_bench_get_u64(void *raw_map, uint64_t key, uint64_t *value)
{
    mlib_bench_map *map = mlib_bench_cast(raw_map);
    const uint64_t *found;

    if (map == NULL || map->kind != BENCH_KEYS_U64 || value == NULL ||
        mlib_u64_oor_equal(key, 0u) || mlib_u64_oor_equal(key, 1u)) {
        return false;
    }
    found = mlib_u64_map_get(map->raw.u64, key);
    if (found == NULL) return false;
    *value = *found;
    return true;
}

static bool mlib_bench_remove_u64(void *raw_map, uint64_t key)
{
    mlib_bench_map *map = mlib_bench_cast(raw_map);

    return map != NULL && map->kind == BENCH_KEYS_U64 &&
           !mlib_u64_oor_equal(key, 0u) && !mlib_u64_oor_equal(key, 1u) &&
           mlib_u64_map_erase(map->raw.u64, key);
}

static bool mlib_bench_put_cstr(void *raw_map, const char *key,
                                uint64_t value, bool *inserted)
{
    mlib_bench_map *map = mlib_bench_cast(raw_map);
    size_t old_size;

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL)
        return false;
    old_size = mlib_cstr_map_size(map->raw.cstr);
    if (old_size >= SIZE_MAX / (8u * sizeof(mlib_cstr_map_pair_ct)))
        return false;
    mlib_cstr_map_set_at(map->raw.cstr, key, value);
    if (inserted != NULL)
        *inserted = mlib_cstr_map_size(map->raw.cstr) != old_size;
    return true;
}

static bool mlib_bench_get_cstr(void *raw_map, const char *key,
                                uint64_t *value)
{
    mlib_bench_map *map = mlib_bench_cast(raw_map);
    const uint64_t *found;

    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL ||
        value == NULL) {
        return false;
    }
    found = mlib_cstr_map_get(map->raw.cstr, key);
    if (found == NULL) return false;
    *value = *found;
    return true;
}

static bool mlib_bench_remove_cstr(void *raw_map, const char *key)
{
    mlib_bench_map *map = mlib_bench_cast(raw_map);

    return map != NULL && map->kind == BENCH_KEYS_CSTR && key != NULL &&
           mlib_cstr_map_erase(map->raw.cstr, key);
}

static bool mlib_bench_iterate(void *raw_map, uint64_t *checksum,
                               size_t *seen)
{
    mlib_bench_map *map = mlib_bench_cast(raw_map);
    uint64_t result = 0;
    size_t count = 0;

    if (map == NULL || checksum == NULL || seen == NULL) return false;
    if (map->kind == BENCH_KEYS_U64) {
        mlib_u64_map_it_t iterator;

        for (mlib_u64_map_it(iterator, map->raw.u64);
             !mlib_u64_map_end_p(iterator);
             mlib_u64_map_next(iterator)) {
            result = bench_checksum_add(
                result, mlib_u64_map_cref(iterator)->value);
            ++count;
        }
    } else if (map->kind == BENCH_KEYS_CSTR) {
        mlib_cstr_map_it_t iterator;

        for (mlib_cstr_map_it(iterator, map->raw.cstr);
             !mlib_cstr_map_end_p(iterator);
             mlib_cstr_map_next(iterator)) {
            result = bench_checksum_add(
                result, mlib_cstr_map_cref(iterator)->value);
            ++count;
        }
    } else {
        return false;
    }
    *checksum = result;
    *seen = count;
    return true;
}

static size_t mlib_bench_size(const void *raw_map)
{
    const mlib_bench_map *map = mlib_bench_cast_const(raw_map);

    if (map == NULL) return 0u;
    if (map->kind == BENCH_KEYS_U64)
        return mlib_u64_map_size(map->raw.u64);
    if (map->kind == BENCH_KEYS_CSTR)
        return mlib_cstr_map_size(map->raw.cstr);
    return 0u;
}

static size_t mlib_bench_capacity(const void *raw_map)
{
    (void)raw_map;
    return 0u;
}

static void mlib_bench_destroy(void *raw_map)
{
    mlib_bench_map *map = mlib_bench_cast(raw_map);

    if (map == NULL) return;
    if (map->kind == BENCH_KEYS_U64)
        mlib_u64_map_clear(map->raw.u64);
    else if (map->kind == BENCH_KEYS_CSTR)
        mlib_cstr_map_clear(map->raw.cstr);
    mlib_bench_deallocate(map);
}

const bench_map_ops bench_adapter = {
    "mlib",
    mlib_bench_create,
    mlib_bench_reserve,
    mlib_bench_put_u64,
    mlib_bench_get_u64,
    mlib_bench_remove_u64,
    mlib_bench_put_cstr,
    mlib_bench_get_cstr,
    mlib_bench_remove_cstr,
    mlib_bench_iterate,
    mlib_bench_size,
    mlib_bench_capacity,
    mlib_bench_destroy
};
