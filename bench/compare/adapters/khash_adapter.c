#include "../common/bench_adapter.h"
#include "../common/bench_alloc.h"

static void *khash_bench_calloc(size_t count, size_t size)
{
    if (count != 0u && size > SIZE_MAX / count) return NULL;
    return bench_calloc(count, size);
}

static void *khash_bench_malloc(size_t size)
{
    return bench_malloc(size);
}

static void *khash_bench_realloc(void *pointer, size_t size)
{
    return bench_realloc(pointer, size);
}

#define kcalloc(count, size) khash_bench_calloc((count), (size))
#define kmalloc(size) khash_bench_malloc(size)
#define krealloc(pointer, size) khash_bench_realloc((pointer), (size))
#define kfree(pointer) bench_free(pointer)
#include <khash.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
KHASH_MAP_INIT_INT64(khash_u64, uint64_t)
KHASH_MAP_INIT_STR(khash_cstr, uint64_t)
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

typedef struct khash_bench_map {
    bench_key_kind kind;
    union {
        khash_t(khash_u64) *u64;
        khash_t(khash_cstr) *cstr;
    } raw;
} khash_bench_map;

static khash_bench_map *khash_bench_cast(void *map)
{
    return (khash_bench_map *)map;
}

static const khash_bench_map *khash_bench_cast_const(const void *map)
{
    return (const khash_bench_map *)map;
}

static bool khash_bench_bucket_request(size_t expected, khint_t *buckets)
{
    const size_t max_capacity = (size_t)UINT32_C(1) << 31;
    const size_t max_count = max_capacity / 100u * 77u;
    const size_t key_size =
        sizeof(khint64_t) > sizeof(kh_cstr_t) ? sizeof(khint64_t)
                                               : sizeof(kh_cstr_t);
    const size_t allocation_bound =
        4u * (key_size + sizeof(uint64_t) + sizeof(khint32_t));
    size_t requested;
    size_t remainder;

    if (buckets == NULL) return false;
    if (expected == 0u) {
        *buckets = 0u;
        return true;
    }
    if (expected > max_count || expected > SIZE_MAX / allocation_bound)
        return false;
    requested = expected / 77u * 100u;
    remainder = expected % 77u;
    requested += (remainder * 100u + 76u) / 77u;
    if (requested > max_capacity) return false;
    *buckets = (khint_t)requested;
    return true;
}

static bool khash_bench_reserve(void *raw_map, size_t expected)
{
    khash_bench_map *map = khash_bench_cast(raw_map);
    khint_t requested;
    size_t size;
    size_t occupied;
    size_t upper_bound;
    khint_t current_buckets;

    if (map == NULL) return false;
    if (map->kind == BENCH_KEYS_U64 && map->raw.u64 != NULL) {
        size = (size_t)map->raw.u64->size;
        occupied = (size_t)map->raw.u64->n_occupied;
        upper_bound = (size_t)map->raw.u64->upper_bound;
        current_buckets = map->raw.u64->n_buckets;
    } else if (map->kind == BENCH_KEYS_CSTR && map->raw.cstr != NULL) {
        size = (size_t)map->raw.cstr->size;
        occupied = (size_t)map->raw.cstr->n_occupied;
        upper_bound = (size_t)map->raw.cstr->upper_bound;
        current_buckets = map->raw.cstr->n_buckets;
    } else {
        return false;
    }
    if (expected <= size ||
        (occupied <= upper_bound &&
         expected - size <= upper_bound - occupied)) {
        return true;
    }
    if (expected <= upper_bound) {
        requested = current_buckets;
    } else {
        if (!khash_bench_bucket_request(expected, &requested)) return false;
        if (requested < current_buckets) requested = current_buckets;
    }
    if (map->kind == BENCH_KEYS_U64)
        return kh_resize(khash_u64, map->raw.u64, requested) == 0;
    return kh_resize(khash_cstr, map->raw.cstr, requested) == 0;
}

static void *khash_bench_create(bench_key_kind kind, size_t expected)
{
    khash_bench_map *map;

    if (kind != BENCH_KEYS_U64 && kind != BENCH_KEYS_CSTR) return NULL;
    map = (khash_bench_map *)bench_malloc(sizeof(*map));
    if (map == NULL) return NULL;
    map->kind = kind;
    if (kind == BENCH_KEYS_U64)
        map->raw.u64 = kh_init(khash_u64);
    else
        map->raw.cstr = kh_init(khash_cstr);
    if ((kind == BENCH_KEYS_U64 && map->raw.u64 == NULL) ||
        (kind == BENCH_KEYS_CSTR && map->raw.cstr == NULL) ||
        (expected != 0u && !khash_bench_reserve(map, expected))) {
        if (kind == BENCH_KEYS_U64)
            kh_destroy(khash_u64, map->raw.u64);
        else
            kh_destroy(khash_cstr, map->raw.cstr);
        bench_free(map);
        return NULL;
    }
    return map;
}

static bool khash_bench_put_u64(void *raw_map, uint64_t key, uint64_t value,
                                bool *inserted)
{
    khash_bench_map *map = khash_bench_cast(raw_map);
    khiter_t iterator;
    int status;

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_U64) return false;
    iterator = kh_put(khash_u64, map->raw.u64, (khint64_t)key, &status);
    if (status < 0 || iterator == kh_end(map->raw.u64)) return false;
    kh_val(map->raw.u64, iterator) = value;
    if (inserted != NULL) *inserted = status > 0;
    return true;
}

static bool khash_bench_get_u64(void *raw_map, uint64_t key, uint64_t *value)
{
    khash_bench_map *map = khash_bench_cast(raw_map);
    khiter_t iterator;

    if (map == NULL || map->kind != BENCH_KEYS_U64 || value == NULL)
        return false;
    iterator = kh_get(khash_u64, map->raw.u64, (khint64_t)key);
    if (iterator == kh_end(map->raw.u64)) return false;
    *value = kh_val(map->raw.u64, iterator);
    return true;
}

static bool khash_bench_remove_u64(void *raw_map, uint64_t key)
{
    khash_bench_map *map = khash_bench_cast(raw_map);
    khiter_t iterator;

    if (map == NULL || map->kind != BENCH_KEYS_U64) return false;
    iterator = kh_get(khash_u64, map->raw.u64, (khint64_t)key);
    if (iterator == kh_end(map->raw.u64)) return false;
    kh_del(khash_u64, map->raw.u64, iterator);
    return true;
}

static bool khash_bench_put_cstr(void *raw_map, const char *key,
                                 uint64_t value, bool *inserted)
{
    khash_bench_map *map = khash_bench_cast(raw_map);
    khiter_t iterator;
    int status;

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL)
        return false;
    iterator = kh_put(khash_cstr, map->raw.cstr, key, &status);
    if (status < 0 || iterator == kh_end(map->raw.cstr)) return false;
    kh_val(map->raw.cstr, iterator) = value;
    if (inserted != NULL) *inserted = status > 0;
    return true;
}

static bool khash_bench_get_cstr(void *raw_map, const char *key,
                                 uint64_t *value)
{
    khash_bench_map *map = khash_bench_cast(raw_map);
    khiter_t iterator;

    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL ||
        value == NULL) {
        return false;
    }
    iterator = kh_get(khash_cstr, map->raw.cstr, key);
    if (iterator == kh_end(map->raw.cstr)) return false;
    *value = kh_val(map->raw.cstr, iterator);
    return true;
}

static bool khash_bench_remove_cstr(void *raw_map, const char *key)
{
    khash_bench_map *map = khash_bench_cast(raw_map);
    khiter_t iterator;

    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL)
        return false;
    iterator = kh_get(khash_cstr, map->raw.cstr, key);
    if (iterator == kh_end(map->raw.cstr)) return false;
    kh_del(khash_cstr, map->raw.cstr, iterator);
    return true;
}

static bool khash_bench_iterate(void *raw_map, uint64_t *checksum,
                                size_t *seen)
{
    khash_bench_map *map = khash_bench_cast(raw_map);
    uint64_t result = 0;
    size_t count = 0;
    khiter_t iterator;

    if (map == NULL || checksum == NULL || seen == NULL) return false;
    if (map->kind == BENCH_KEYS_U64) {
        for (iterator = kh_begin(map->raw.u64);
             iterator != kh_end(map->raw.u64); ++iterator) {
            if (!kh_exist(map->raw.u64, iterator)) continue;
            result = bench_checksum_add(result,
                                        kh_val(map->raw.u64, iterator));
            ++count;
        }
    } else if (map->kind == BENCH_KEYS_CSTR) {
        for (iterator = kh_begin(map->raw.cstr);
             iterator != kh_end(map->raw.cstr); ++iterator) {
            if (!kh_exist(map->raw.cstr, iterator)) continue;
            result = bench_checksum_add(result,
                                        kh_val(map->raw.cstr, iterator));
            ++count;
        }
    } else {
        return false;
    }
    *checksum = result;
    *seen = count;
    return true;
}

static size_t khash_bench_size(const void *raw_map)
{
    const khash_bench_map *map = khash_bench_cast_const(raw_map);

    if (map == NULL) return 0u;
    if (map->kind == BENCH_KEYS_U64) return (size_t)kh_size(map->raw.u64);
    if (map->kind == BENCH_KEYS_CSTR) return (size_t)kh_size(map->raw.cstr);
    return 0u;
}

static size_t khash_bench_capacity(const void *raw_map)
{
    const khash_bench_map *map = khash_bench_cast_const(raw_map);

    if (map == NULL) return 0u;
    if (map->kind == BENCH_KEYS_U64)
        return (size_t)kh_n_buckets(map->raw.u64);
    if (map->kind == BENCH_KEYS_CSTR)
        return (size_t)kh_n_buckets(map->raw.cstr);
    return 0u;
}

static void khash_bench_destroy(void *raw_map)
{
    khash_bench_map *map = khash_bench_cast(raw_map);

    if (map == NULL) return;
    if (map->kind == BENCH_KEYS_U64)
        kh_destroy(khash_u64, map->raw.u64);
    else if (map->kind == BENCH_KEYS_CSTR)
        kh_destroy(khash_cstr, map->raw.cstr);
    bench_free(map);
}

const bench_map_ops bench_adapter = {
    "khash",
    khash_bench_create,
    khash_bench_reserve,
    khash_bench_put_u64,
    khash_bench_get_u64,
    khash_bench_remove_u64,
    khash_bench_put_cstr,
    khash_bench_get_cstr,
    khash_bench_remove_cstr,
    khash_bench_iterate,
    khash_bench_size,
    khash_bench_capacity,
    khash_bench_destroy
};
