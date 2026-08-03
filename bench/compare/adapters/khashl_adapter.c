#include "../common/bench_adapter.h"
#include "../common/bench_alloc.h"

static void *khashl_bench_malloc_array(size_t count, size_t size)
{
    if (count != 0u && size > SIZE_MAX / count) return NULL;
    return bench_malloc(count * size);
}

static void *khashl_bench_calloc_array(size_t count, size_t size)
{
    if (count != 0u && size > SIZE_MAX / count) return NULL;
    return bench_calloc(count, size);
}

static void *khashl_bench_realloc_array(void *pointer, size_t count,
                                        size_t size)
{
    if (count != 0u && size > SIZE_MAX / count) return NULL;
    return bench_realloc(pointer, count * size);
}

#define Kmalloc(km, type, count)                                             \
    ((void)(km), (type *)khashl_bench_malloc_array((size_t)(count),          \
                                                    sizeof(type)))
#define Kcalloc(km, type, count)                                             \
    ((void)(km), (type *)khashl_bench_calloc_array((size_t)(count),          \
                                                    sizeof(type)))
#define Krealloc(km, type, pointer, count)                                   \
    ((void)(km), (type *)khashl_bench_realloc_array(                         \
                    (pointer), (size_t)(count), sizeof(type)))
#define Kfree(km, pointer) ((void)(km), bench_free(pointer))
#include <khashl.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
KHASHL_MAP_INIT(KH_LOCAL, khashl_u64_map, khashl_u64, uint64_t, uint64_t,
                kh_hash_uint64, kh_eq_generic)
KHASHL_MAP_INIT(KH_LOCAL, khashl_cstr_map, khashl_cstr, const char *,
                uint64_t, kh_hash_str, kh_eq_str)
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

typedef struct khashl_bench_map {
    bench_key_kind kind;
    union {
        khashl_u64_map *u64;
        khashl_cstr_map *cstr;
    } raw;
} khashl_bench_map;

static khashl_bench_map *khashl_bench_cast(void *map)
{
    return (khashl_bench_map *)map;
}

static const khashl_bench_map *khashl_bench_cast_const(const void *map)
{
    return (const khashl_bench_map *)map;
}

static bool khashl_bench_bucket_request(size_t expected, khint_t *buckets)
{
    const khint_t max_capacity = UINT32_C(1) << 31;
    const size_t max_count = (size_t)kh_max_count(max_capacity);
    const size_t u64_entry_size = sizeof(khashl_u64_map_m_bucket_t);
    const size_t cstr_entry_size = sizeof(khashl_cstr_map_m_bucket_t);
    const size_t max_entry_size =
        u64_entry_size > cstr_entry_size ? u64_entry_size : cstr_entry_size;
    const size_t allocation_bound =
        4u * (max_entry_size + sizeof(khint32_t));
    size_t requested;

    if (buckets == NULL) return false;
    if (expected == 0u) {
        *buckets = 0u;
        return true;
    }
    if (expected > max_count || expected > SIZE_MAX / allocation_bound)
        return false;
    requested = expected / 3u * 4u;
    if (expected % 3u != 0u) requested += 4u;
    if (requested > (size_t)UINT32_MAX) return false;
    *buckets = (khint_t)requested;
    return true;
}

static bool khashl_bench_reserve(void *raw_map, size_t expected)
{
    khashl_bench_map *map = khashl_bench_cast(raw_map);
    khint_t requested;
    size_t capacity;

    if (map == NULL) return false;
    if (map->kind == BENCH_KEYS_U64 && map->raw.u64 != NULL) {
        capacity = (size_t)kh_capacity(map->raw.u64);
    } else if (map->kind == BENCH_KEYS_CSTR && map->raw.cstr != NULL) {
        capacity = (size_t)kh_capacity(map->raw.cstr);
    } else {
        return false;
    }
    if ((size_t)kh_max_count(capacity) >= expected) return true;
    if (!khashl_bench_bucket_request(expected, &requested)) return false;
    if ((size_t)requested < capacity) requested = (khint_t)capacity;
    if (map->kind == BENCH_KEYS_U64) {
        khashl_u64_resize(map->raw.u64, requested);
        return (size_t)kh_max_count(kh_capacity(map->raw.u64)) >= expected;
    }
    khashl_cstr_resize(map->raw.cstr, requested);
    return (size_t)kh_max_count(kh_capacity(map->raw.cstr)) >= expected;
}

static void *khashl_bench_create(bench_key_kind kind, size_t expected)
{
    khashl_bench_map *map;

    if (kind != BENCH_KEYS_U64 && kind != BENCH_KEYS_CSTR) return NULL;
    map = (khashl_bench_map *)bench_malloc(sizeof(*map));
    if (map == NULL) return NULL;
    map->kind = kind;
    if (kind == BENCH_KEYS_U64) {
        map->raw.u64 = (khashl_u64_map *)khashl_bench_calloc_array(
            1u, sizeof(*map->raw.u64));
    } else {
        map->raw.cstr = (khashl_cstr_map *)khashl_bench_calloc_array(
            1u, sizeof(*map->raw.cstr));
    }
    if ((kind == BENCH_KEYS_U64 && map->raw.u64 == NULL) ||
        (kind == BENCH_KEYS_CSTR && map->raw.cstr == NULL) ||
        (expected != 0u && !khashl_bench_reserve(map, expected))) {
        if (kind == BENCH_KEYS_U64)
            khashl_u64_destroy(map->raw.u64);
        else
            khashl_cstr_destroy(map->raw.cstr);
        bench_free(map);
        return NULL;
    }
    return map;
}

static bool khashl_bench_put_u64(void *raw_map, uint64_t key,
                                 uint64_t value, bool *inserted)
{
    khashl_bench_map *map = khashl_bench_cast(raw_map);
    khint_t iterator;
    int absent;

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_U64) return false;
    iterator = khashl_u64_put(map->raw.u64, key, &absent);
    if (absent < 0 || iterator == kh_end(map->raw.u64)) return false;
    kh_val(map->raw.u64, iterator) = value;
    if (inserted != NULL) *inserted = absent != 0;
    return true;
}

static bool khashl_bench_get_u64(void *raw_map, uint64_t key,
                                 uint64_t *value)
{
    khashl_bench_map *map = khashl_bench_cast(raw_map);
    khint_t iterator;

    if (map == NULL || map->kind != BENCH_KEYS_U64 || value == NULL)
        return false;
    iterator = khashl_u64_get(map->raw.u64, key);
    if (iterator == kh_end(map->raw.u64)) return false;
    *value = kh_val(map->raw.u64, iterator);
    return true;
}

static bool khashl_bench_remove_u64(void *raw_map, uint64_t key)
{
    khashl_bench_map *map = khashl_bench_cast(raw_map);
    khint_t iterator;

    if (map == NULL || map->kind != BENCH_KEYS_U64) return false;
    iterator = khashl_u64_get(map->raw.u64, key);
    return iterator != kh_end(map->raw.u64) &&
           khashl_u64_del(map->raw.u64, iterator) != 0;
}

static bool khashl_bench_put_cstr(void *raw_map, const char *key,
                                  uint64_t value, bool *inserted)
{
    khashl_bench_map *map = khashl_bench_cast(raw_map);
    khint_t iterator;
    int absent;

    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL)
        return false;
    iterator = khashl_cstr_put(map->raw.cstr, key, &absent);
    if (absent < 0 || iterator == kh_end(map->raw.cstr)) return false;
    kh_val(map->raw.cstr, iterator) = value;
    if (inserted != NULL) *inserted = absent != 0;
    return true;
}

static bool khashl_bench_get_cstr(void *raw_map, const char *key,
                                  uint64_t *value)
{
    khashl_bench_map *map = khashl_bench_cast(raw_map);
    khint_t iterator;

    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL ||
        value == NULL) {
        return false;
    }
    iterator = khashl_cstr_get(map->raw.cstr, key);
    if (iterator == kh_end(map->raw.cstr)) return false;
    *value = kh_val(map->raw.cstr, iterator);
    return true;
}

static bool khashl_bench_remove_cstr(void *raw_map, const char *key)
{
    khashl_bench_map *map = khashl_bench_cast(raw_map);
    khint_t iterator;

    if (map == NULL || map->kind != BENCH_KEYS_CSTR || key == NULL)
        return false;
    iterator = khashl_cstr_get(map->raw.cstr, key);
    return iterator != kh_end(map->raw.cstr) &&
           khashl_cstr_del(map->raw.cstr, iterator) != 0;
}

static bool khashl_bench_iterate(void *raw_map, uint64_t *checksum,
                                 size_t *seen)
{
    khashl_bench_map *map = khashl_bench_cast(raw_map);
    uint64_t result = 0;
    size_t count = 0;
    khint_t iterator;

    if (map == NULL || checksum == NULL || seen == NULL) return false;
    if (map->kind == BENCH_KEYS_U64) {
        kh_foreach(map->raw.u64, iterator)
        {
            result = bench_checksum_add(result,
                                        kh_val(map->raw.u64, iterator));
            ++count;
        }
    } else if (map->kind == BENCH_KEYS_CSTR) {
        kh_foreach(map->raw.cstr, iterator)
        {
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

static size_t khashl_bench_size(const void *raw_map)
{
    const khashl_bench_map *map = khashl_bench_cast_const(raw_map);

    if (map == NULL) return 0u;
    if (map->kind == BENCH_KEYS_U64) return (size_t)kh_size(map->raw.u64);
    if (map->kind == BENCH_KEYS_CSTR) return (size_t)kh_size(map->raw.cstr);
    return 0u;
}

static size_t khashl_bench_capacity(const void *raw_map)
{
    const khashl_bench_map *map = khashl_bench_cast_const(raw_map);

    if (map == NULL) return 0u;
    if (map->kind == BENCH_KEYS_U64)
        return (size_t)kh_capacity(map->raw.u64);
    if (map->kind == BENCH_KEYS_CSTR)
        return (size_t)kh_capacity(map->raw.cstr);
    return 0u;
}

static void khashl_bench_destroy(void *raw_map)
{
    khashl_bench_map *map = khashl_bench_cast(raw_map);

    if (map == NULL) return;
    if (map->kind == BENCH_KEYS_U64)
        khashl_u64_destroy(map->raw.u64);
    else if (map->kind == BENCH_KEYS_CSTR)
        khashl_cstr_destroy(map->raw.cstr);
    bench_free(map);
}

const bench_map_ops bench_adapter = {
    "khashl",
    khashl_bench_create,
    khashl_bench_reserve,
    khashl_bench_put_u64,
    khashl_bench_get_u64,
    khashl_bench_remove_u64,
    khashl_bench_put_cstr,
    khashl_bench_get_cstr,
    khashl_bench_remove_cstr,
    khashl_bench_iterate,
    khashl_bench_size,
    khashl_bench_capacity,
    khashl_bench_destroy
};
