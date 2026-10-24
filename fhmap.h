/*
   fhmap.h - fast, compact, single-header hash map for C11 on Linux

   SPDX-License-Identifier: MIT

   QUICK START

       // In exactly one .c file:
       #define FHMAP_IMPLEMENTATION
       #include "fhmap.h"

       // At file scope:
       FHMAP_DECLARE(id_map, uint64_t, const char *);

       // In a function:
       id_map map;
       if (!id_map_init(&map, 1024)) { ... }
       id_map_put(&map, 42, "answer");
       const char **value = id_map_get(&map, 42);
       id_map_destroy(&map);

   Define FHMAP_STATIC before FHMAP_IMPLEMENTATION to give the raw API internal
   linkage. Define FHMAP_DISABLE_SIMD to force the scalar control-byte scanner.
   Override FHMAP_MALLOC(user, size) and FHMAP_FREE(user, pointer) to install an
   allocator. See README.md and docs/API.md for the complete contract.
*/

#ifndef FHMAP_H_INCLUDED
#define FHMAP_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FHMAP_VERSION_MAJOR 1
#define FHMAP_VERSION_MINOR 0
#define FHMAP_VERSION_PATCH 0
#define FHMAP_VERSION_STRING "1.0.0"

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef FHMAP_API
#  ifdef FHMAP_STATIC
#    define FHMAP_API static
#  else
#    define FHMAP_API extern
#  endif
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  define FHMAP_ALIGNOF(type) _Alignof(type)
#elif defined(__GNUC__) || defined(__clang__)
#  define FHMAP_ALIGNOF(type) __alignof__(type)
#else
#  define FHMAP_ALIGNOF(type) sizeof(void *)
#endif

typedef uint64_t (*fhmap_hash_fn)(const void *key,
                                  size_t key_size,
                                  uint64_t seed,
                                  void *user);

typedef bool (*fhmap_equal_fn)(const void *stored_key,
                               const void *query_key,
                               size_t key_size,
                               void *user);

typedef struct fhmap_config {
    size_t key_size;
    size_t value_size;
    size_t initial_capacity; /* Number of elements, not raw buckets. */
    size_t key_align;
    size_t value_align;
    uint64_t seed;
    fhmap_hash_fn hash;
    fhmap_equal_fn equal;
    void *user;
} fhmap_config;

/* The fields are public for stack allocation and observability. Treat every
   field as read-only; direct mutation is unsupported. */
typedef struct fhmap {
    void *allocation;
    unsigned char *control;
    unsigned char *keys;
    unsigned char *values;
    size_t capacity;
    size_t size;
    size_t tombstones;
    size_t growth_limit;
    size_t key_size;
    size_t value_size;
    size_t key_stride;
    size_t value_stride;
    size_t key_align;
    size_t value_align;
    size_t allocation_bytes;
    uint64_t seed;
    uint64_t generation;
    fhmap_hash_fn hash;
    fhmap_equal_fn equal;
    void *user;
} fhmap;

typedef struct fhmap_iter {
    size_t next_index;
    uint64_t generation;
} fhmap_iter;

typedef enum fhmap_put_result {
    FHMAP_PUT_ERROR = -1,
    FHMAP_PUT_UPDATED = 0,
    FHMAP_PUT_INSERTED = 1
} fhmap_put_result;

#define FHMAP_ZERO_INIT {0}

FHMAP_API fhmap_config fhmap_config_make(size_t key_size, size_t value_size);
FHMAP_API uint64_t fhmap_random_seed(void);

FHMAP_API uint64_t fhmap_hash_bytes(const void *key, size_t key_size,
                                    uint64_t seed, void *user);
/* These two helpers expect a key whose bytes contain a `const char *`. The map
   borrows the string; it does not copy or free the pointed-to characters. */
FHMAP_API uint64_t fhmap_hash_cstr(const void *key, size_t key_size,
                                   uint64_t seed, void *user);
FHMAP_API bool fhmap_equal_cstr(const void *stored_key, const void *query_key,
                               size_t key_size, void *user);

FHMAP_API bool fhmap_init(fhmap *map, const fhmap_config *config);
FHMAP_API void fhmap_destroy(fhmap *map);
FHMAP_API void fhmap_clear(fhmap *map);
FHMAP_API bool fhmap_reserve(fhmap *map, size_t element_capacity);
FHMAP_API bool fhmap_compact(fhmap *map);

FHMAP_API void *fhmap_get(fhmap *map, const void *key);
FHMAP_API const void *fhmap_get_const(const fhmap *map, const void *key);
FHMAP_API bool fhmap_contains(const fhmap *map, const void *key);

/* On a new key, emplace returns uninitialized value storage and sets
   *inserted to true when supplied. The returned pointer is invalidated by a later structural
   mutation (insert, remove, reserve, compact, clear, or destroy). */
FHMAP_API void *fhmap_emplace(fhmap *map, const void *key, bool *inserted);
FHMAP_API fhmap_put_result fhmap_put(fhmap *map, const void *key,
                                    const void *value);
FHMAP_API bool fhmap_remove(fhmap *map, const void *key);
FHMAP_API bool fhmap_remove_copy(fhmap *map, const void *key, void *value_out);

FHMAP_API size_t fhmap_size(const fhmap *map);
FHMAP_API size_t fhmap_capacity(const fhmap *map);
FHMAP_API size_t fhmap_memory_usage(const fhmap *map);
FHMAP_API double fhmap_load_factor(const fhmap *map);

FHMAP_API fhmap_iter fhmap_iter_begin(const fhmap *map);
FHMAP_API bool fhmap_iter_next(fhmap *map, fhmap_iter *iter,
                               const void **key_out, void **value_out);

/* A declaration generates the whole facade; callers may use only part of it. */
#if defined(__GNUC__) || defined(__clang__)
#  define FHMAP_TYPED_INLINE static inline __attribute__((unused))
#else
#  define FHMAP_TYPED_INLINE static inline
#endif

/* Generate typed wrappers over the raw map. KEY_TYPE and
   VALUE_TYPE must be complete object types and should not contain commas. */
#define FHMAP_DECLARE(NAME, KEY_TYPE, VALUE_TYPE)                                  \
    typedef struct NAME { fhmap raw; } NAME;                                      \
    typedef fhmap_iter NAME##_iter;                                               \
    FHMAP_TYPED_INLINE bool NAME##_init_seed(NAME *m, size_t expected, uint64_t seed)  \
    {                                                                             \
        fhmap_config c = fhmap_config_make(sizeof(KEY_TYPE), sizeof(VALUE_TYPE)); \
        c.initial_capacity = expected;                                            \
        c.key_align = FHMAP_ALIGNOF(KEY_TYPE);                                    \
        c.value_align = FHMAP_ALIGNOF(VALUE_TYPE);                                \
        c.seed = seed;                                                            \
        return fhmap_init(&m->raw, &c);                                           \
    }                                                                             \
    FHMAP_TYPED_INLINE bool NAME##_init(NAME *m, size_t expected)                      \
    {                                                                             \
        return NAME##_init_seed(m, expected, fhmap_random_seed());                \
    }                                                                             \
    FHMAP_TYPED_INLINE void NAME##_destroy(NAME *m) { fhmap_destroy(&m->raw); }        \
    FHMAP_TYPED_INLINE void NAME##_clear(NAME *m) { fhmap_clear(&m->raw); }            \
    FHMAP_TYPED_INLINE bool NAME##_reserve(NAME *m, size_t n)                          \
    { return fhmap_reserve(&m->raw, n); }                                         \
    FHMAP_TYPED_INLINE bool NAME##_compact(NAME *m) { return fhmap_compact(&m->raw); } \
    FHMAP_TYPED_INLINE size_t NAME##_size(const NAME *m)                               \
    { return fhmap_size(&m->raw); }                                               \
    FHMAP_TYPED_INLINE size_t NAME##_capacity(const NAME *m)                           \
    { return fhmap_capacity(&m->raw); }                                           \
    FHMAP_TYPED_INLINE size_t NAME##_memory_usage(const NAME *m)                       \
    { return fhmap_memory_usage(&m->raw); }                                       \
    FHMAP_TYPED_INLINE VALUE_TYPE *NAME##_get(NAME *m, KEY_TYPE key)                   \
    { return (VALUE_TYPE *)fhmap_get(&m->raw, &key); }                            \
    FHMAP_TYPED_INLINE const VALUE_TYPE *NAME##_get_const(const NAME *m, KEY_TYPE key) \
    { return (const VALUE_TYPE *)fhmap_get_const(&m->raw, &key); }                 \
    FHMAP_TYPED_INLINE bool NAME##_contains(const NAME *m, KEY_TYPE key)               \
    { return fhmap_contains(&m->raw, &key); }                                     \
    FHMAP_TYPED_INLINE VALUE_TYPE *NAME##_emplace(NAME *m, KEY_TYPE key, bool *is_new) \
    { return (VALUE_TYPE *)fhmap_emplace(&m->raw, &key, is_new); }                 \
    FHMAP_TYPED_INLINE fhmap_put_result NAME##_put(NAME *m, KEY_TYPE key,              \
                                               VALUE_TYPE value)                   \
    { return fhmap_put(&m->raw, &key, &value); }                                  \
    FHMAP_TYPED_INLINE bool NAME##_remove(NAME *m, KEY_TYPE key)                       \
    { return fhmap_remove(&m->raw, &key); }                                       \
    FHMAP_TYPED_INLINE bool NAME##_remove_copy(NAME *m, KEY_TYPE key, VALUE_TYPE *out) \
    { return fhmap_remove_copy(&m->raw, &key, out); }                             \
    FHMAP_TYPED_INLINE NAME##_iter NAME##_iter_begin(const NAME *m)                    \
    { return fhmap_iter_begin(&m->raw); }                                         \
    FHMAP_TYPED_INLINE bool NAME##_iter_next(NAME *m, NAME##_iter *it,                 \
                                         const KEY_TYPE **key, VALUE_TYPE **value) \
    {                                                                             \
        const void *raw_key = NULL;                                               \
        void *raw_value = NULL;                                                   \
        bool ok = fhmap_iter_next(&m->raw, it, &raw_key, &raw_value);             \
        if (ok) {                                                                 \
            if (key) *key = (const KEY_TYPE *)raw_key;                            \
            if (value) *value = (VALUE_TYPE *)raw_value;                          \
        }                                                                         \
        return ok;                                                                \
    }                                                                             \
    typedef int NAME##_fhmap_declaration_requires_semicolon

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* FHMAP_H_INCLUDED */

/* ------------------------------------------------------------------------- */
/* Implementation                                                            */

#if defined(FHMAP_IMPLEMENTATION) && !defined(FHMAP_IMPLEMENTATION_INCLUDED)
#define FHMAP_IMPLEMENTATION_INCLUDED

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/random.h>

#if !defined(FHMAP_DISABLE_SIMD) && defined(__SSE2__)
#  include <emmintrin.h>
#  define FHMAP_HAS_SSE2 1
#else
#  define FHMAP_HAS_SSE2 0
#endif

#ifndef FHMAP_MALLOC
#  define FHMAP_MALLOC(user, size) malloc(size)
#endif

#ifndef FHMAP_FREE
#  define FHMAP_FREE(user, pointer) free(pointer)
#endif

#ifndef FHMAP_MAX_ALIGNMENT
#  define FHMAP_MAX_ALIGNMENT 4096u
#endif

#if defined(__GNUC__) || defined(__clang__)
#  define FHMAP_LIKELY(x) __builtin_expect(!!(x), 1)
#  define FHMAP_UNLIKELY(x) __builtin_expect(!!(x), 0)
#  define FHMAP_INLINE static inline __attribute__((always_inline))
#else
#  define FHMAP_LIKELY(x) (x)
#  define FHMAP_UNLIKELY(x) (x)
#  define FHMAP_INLINE static inline
#endif

#define FHMAP_GROUP_WIDTH 16u
#define FHMAP_MIN_CAPACITY 16u
#define FHMAP_CONTROL_EMPTY 0x80u
#define FHMAP_CONTROL_DELETED 0xfeu
#define FHMAP_NPOS ((size_t)-1)

FHMAP_INLINE bool fhmap__is_power_of_two(size_t x)
{
    return x != 0 && (x & (x - 1)) == 0;
}

FHMAP_INLINE bool fhmap__add_overflow(size_t a, size_t b, size_t *out)
{
    if (a > SIZE_MAX - b) return true;
    *out = a + b;
    return false;
}

FHMAP_INLINE bool fhmap__mul_overflow(size_t a, size_t b, size_t *out)
{
    if (a != 0 && b > SIZE_MAX / a) return true;
    *out = a * b;
    return false;
}

FHMAP_INLINE bool fhmap__align_up_checked(size_t value, size_t alignment,
                                          size_t *out)
{
    size_t mask = alignment - 1;
    if (value > SIZE_MAX - mask) return false;
    *out = (value + mask) & ~mask;
    return true;
}

FHMAP_INLINE size_t fhmap__growth_limit(size_t capacity)
{
    return capacity - capacity / 8u; /* 87.5% */
}

FHMAP_INLINE uint64_t fhmap__load64(const void *source)
{
    uint64_t value;
    memcpy(&value, source, sizeof(value));
    return value;
}

FHMAP_INLINE uint32_t fhmap__load32(const void *source)
{
    uint32_t value;
    memcpy(&value, source, sizeof(value));
    return value;
}

FHMAP_INLINE uint64_t fhmap__mum(uint64_t a, uint64_t b)
{
#if defined(__SIZEOF_INT128__)
    __uint128_t product = (__uint128_t)a * (__uint128_t)b;
    return (uint64_t)product ^ (uint64_t)(product >> 64);
#else
    uint64_t ah = a >> 32, al = (uint32_t)a;
    uint64_t bh = b >> 32, bl = (uint32_t)b;
    uint64_t hi = ah * bh;
    uint64_t mid1 = ah * bl;
    uint64_t mid2 = al * bh;
    uint64_t lo = al * bl;
    uint64_t carry = ((lo >> 32) + (uint32_t)mid1 + (uint32_t)mid2) >> 32;
    hi += (mid1 >> 32) + (mid2 >> 32) + carry;
    lo += mid1 << 32;
    lo += mid2 << 32;
    return hi ^ lo;
#endif
}

FHMAP_INLINE uint64_t fhmap__avalanche(uint64_t x)
{
    x ^= x >> 32;
    x *= UINT64_C(0xd6e8feb86659fd93);
    x ^= x >> 32;
    x *= UINT64_C(0xd6e8feb86659fd93);
    return x ^ (x >> 32);
}

FHMAP_API uint64_t fhmap_hash_bytes(const void *key, size_t key_size,
                                    uint64_t seed, void *user)
{
    const unsigned char *p = (const unsigned char *)key;
    const size_t original_size = key_size;
    uint64_t a = seed ^ UINT64_C(0xa0761d6478bd642f);
    uint64_t b = (uint64_t)key_size ^ UINT64_C(0xe7037ed1a0b428db);
    (void)user;

    /* Fixed 4-, 8-, and 16-byte keys skip the generic tail loop. The 4- and
       8-byte paths also avoid the folded wide multiplication. */
    if (key_size == 4u) {
        return fhmap__avalanche((uint64_t)fhmap__load32(p) ^ seed ^
                                UINT64_C(0xa0761d6478bd642f));
    }
    if (key_size == 8u) {
        return fhmap__avalanche(fhmap__load64(p) ^ seed ^
                                UINT64_C(0xe7037ed1a0b428db));
    }
    if (key_size == 16u) {
        return fhmap__avalanche(
            fhmap__mum(fhmap__load64(p) ^ seed ^
                           UINT64_C(0xa0761d6478bd642f),
                       fhmap__load64(p + 8u) ^
                           UINT64_C(0x8ebc6af09c88c6e3)));
    }

    while (key_size >= 16u) {
        uint64_t x = fhmap__load64(p);
        uint64_t y = fhmap__load64(p + 8u);
        a = fhmap__mum(x ^ a, y ^ UINT64_C(0x8ebc6af09c88c6e3));
        b ^= a;
        p += 16u;
        key_size -= 16u;
    }

    if (key_size >= 8u) {
        a ^= fhmap__load64(p);
        p += 8u;
        key_size -= 8u;
    }
    if (key_size >= 4u) {
        b ^= (uint64_t)fhmap__load32(p);
        p += 4u;
        key_size -= 4u;
    }
    if (key_size != 0u) {
        uint64_t tail = 0;
        memcpy(&tail, p, key_size);
        b ^= tail;
    }

    return fhmap__avalanche(
        fhmap__mum(a ^ UINT64_C(0x589965cc75374cc3),
                   b ^ UINT64_C(0x1d8e4e27c47d124f)) ^
        (uint64_t)original_size);
}

FHMAP_API uint64_t fhmap_hash_cstr(const void *key, size_t key_size,
                                   uint64_t seed, void *user)
{
    const char *text = NULL;
    (void)key_size;
    memcpy(&text, key, sizeof(text));
    if (text == NULL) {
        return fhmap__avalanche(seed ^ UINT64_C(0x9e3779b97f4a7c15));
    }
    return fhmap_hash_bytes(text, strlen(text), seed, user);
}

FHMAP_API bool fhmap_equal_cstr(const void *stored_key, const void *query_key,
                                size_t key_size, void *user)
{
    const char *a = NULL;
    const char *b = NULL;
    (void)key_size;
    (void)user;
    memcpy(&a, stored_key, sizeof(a));
    memcpy(&b, query_key, sizeof(b));
    if (a == NULL || b == NULL) return a == b;
    return strcmp(a, b) == 0;
}

FHMAP_API uint64_t fhmap_random_seed(void)
{
    uint64_t seed = 0;
    ssize_t got;
    do {
        got = getrandom(&seed, sizeof(seed), GRND_NONBLOCK);
    } while (got < 0 && errno == EINTR);
    if (got == (ssize_t)sizeof(seed)) return seed;

    {
        struct timespec now;
        uint64_t fallback = (uint64_t)(uintptr_t)&seed;
        if (timespec_get(&now, TIME_UTC) == TIME_UTC) {
            fallback ^= (uint64_t)now.tv_sec;
            fallback ^= (uint64_t)now.tv_nsec << 32;
        }
        fallback ^= (uint64_t)(unsigned long)getpid() << 17;
        return fhmap__avalanche(fallback);
    }
}

FHMAP_API fhmap_config fhmap_config_make(size_t key_size, size_t value_size)
{
    fhmap_config config;
    memset(&config, 0, sizeof(config));
    config.key_size = key_size;
    config.value_size = value_size;
    config.key_align = 1;
    config.value_align = 1;
    config.seed = fhmap_random_seed();
    config.hash = fhmap_hash_bytes;
    return config;
}

FHMAP_INLINE uint32_t fhmap__group_match(const unsigned char *control,
                                         unsigned char byte)
{
#if FHMAP_HAS_SSE2
    __m128i group = _mm_loadu_si128((const __m128i *)(const void *)control);
    __m128i target = _mm_set1_epi8((char)byte);
    return (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(group, target));
#else
    uint32_t result = 0;
    size_t i;
    for (i = 0; i < FHMAP_GROUP_WIDTH; ++i) {
        result |= (uint32_t)(control[i] == byte) << i;
    }
    return result;
#endif
}

FHMAP_INLINE unsigned fhmap__first_bit(uint32_t mask)
{
#if defined(__GNUC__) || defined(__clang__)
    return (unsigned)__builtin_ctz(mask);
#else
    unsigned bit = 0;
    while ((mask & 1u) == 0u) { mask >>= 1; ++bit; }
    return bit;
#endif
}

FHMAP_INLINE unsigned char *fhmap__key_at(const fhmap *map, size_t index)
{
    return map->keys + index * map->key_stride;
}

FHMAP_INLINE unsigned char *fhmap__value_at(const fhmap *map, size_t index)
{
    return map->values + index * map->value_stride;
}

FHMAP_INLINE bool fhmap__keys_equal(const fhmap *map, const void *stored,
                                    const void *query)
{
    if (map->equal != NULL) {
        return map->equal(stored, query, map->key_size, map->user);
    }
    switch (map->key_size) {
        case 4:
            return fhmap__load32(stored) == fhmap__load32(query);
        case 8:
            return fhmap__load64(stored) == fhmap__load64(query);
        case 16:
            return memcmp(stored, query, 16u) == 0;
        default:
            return memcmp(stored, query, map->key_size) == 0;
    }
}

FHMAP_INLINE void fhmap__set_control(fhmap *map, size_t index,
                                     unsigned char value)
{
    map->control[index] = value;
    if (index < FHMAP_GROUP_WIDTH) {
        map->control[map->capacity + index] = value;
    }
}

FHMAP_INLINE uint64_t fhmap__hash_key(const fhmap *map, const void *key)
{
    return map->hash(key, map->key_size, map->seed, map->user);
}

FHMAP_INLINE size_t fhmap__find_index_hashed(const fhmap *map, const void *key,
                                             uint64_t hash)
{
    size_t position = (size_t)(hash >> 7u) & (map->capacity - 1u);
    const unsigned char fingerprint = (unsigned char)(hash & 0x7fu);
    size_t scanned = 0;

    while (scanned < map->capacity) {
        uint32_t matches = fhmap__group_match(map->control + position,
                                              fingerprint);
        while (matches != 0u) {
            unsigned bit = fhmap__first_bit(matches);
            size_t index = (position + bit) & (map->capacity - 1u);
            if (fhmap__keys_equal(map, fhmap__key_at(map, index), key)) {
                return index;
            }
            matches &= matches - 1u;
        }
        /* A matching key cannot lie past an empty bucket in its probe
           sequence. Check candidates first, avoiding the empty mask on hits. */
        if (fhmap__group_match(map->control + position,
                              FHMAP_CONTROL_EMPTY) != 0u) return FHMAP_NPOS;
        position = (position + FHMAP_GROUP_WIDTH) & (map->capacity - 1u);
        scanned += FHMAP_GROUP_WIDTH;
    }
    return FHMAP_NPOS;
}

/* Empty and deleted bytes both have their high bit set. This search is used
   only after absence has been established, so the first reusable slot wins. */
FHMAP_INLINE uint32_t fhmap__group_available(const unsigned char *control)
{
#if FHMAP_HAS_SSE2
    return (uint32_t)_mm_movemask_epi8(
        _mm_loadu_si128((const __m128i *)(const void *)control));
#else
    uint32_t result = 0;
    unsigned i;
    for (i = 0; i < FHMAP_GROUP_WIDTH; ++i)
        result |= (uint32_t)(control[i] >> 7u) << i;
    return result;
#endif
}

FHMAP_INLINE size_t fhmap__find_insert_slot(const fhmap *map, uint64_t hash)
{
    size_t position = (size_t)(hash >> 7u) & (map->capacity - 1u);
    size_t scanned = 0;
    while (scanned < map->capacity) {
        uint32_t available = fhmap__group_available(map->control + position);
        if (available != 0u)
            return (position + fhmap__first_bit(available)) & (map->capacity - 1u);
        position = (position + FHMAP_GROUP_WIDTH) & (map->capacity - 1u);
        scanned += FHMAP_GROUP_WIDTH;
    }
    return FHMAP_NPOS;
}

static bool fhmap__capacity_for(size_t elements, size_t *capacity_out)
{
    size_t capacity = FHMAP_MIN_CAPACITY;
    while (fhmap__growth_limit(capacity) < elements) {
        if (capacity > SIZE_MAX / 2u) return false;
        capacity *= 2u;
    }
    *capacity_out = capacity;
    return true;
}

static bool fhmap__allocate_exact(fhmap *map, size_t capacity)
{
    size_t key_bytes, value_bytes, key_offset, value_offset;
    size_t raw_size, block_size, key_stride, value_stride;
    size_t base_alignment = FHMAP_GROUP_WIDTH;
    uintptr_t raw_address, aligned_address;
    void *allocation;

    if (!fhmap__align_up_checked(map->key_size, map->key_align, &key_stride) ||
        !fhmap__align_up_checked(map->value_size, map->value_align, &value_stride))
        return false;
    if (fhmap__mul_overflow(capacity, key_stride, &key_bytes) ||
        fhmap__mul_overflow(capacity, value_stride, &value_bytes))
        return false;
    if (fhmap__add_overflow(capacity, FHMAP_GROUP_WIDTH, &block_size) ||
        !fhmap__align_up_checked(block_size, map->key_align, &key_offset) ||
        fhmap__add_overflow(key_offset, key_bytes, &block_size) ||
        !fhmap__align_up_checked(block_size, map->value_align, &value_offset) ||
        fhmap__add_overflow(value_offset, value_bytes, &block_size))
        return false;

    if (map->key_align > base_alignment) base_alignment = map->key_align;
    if (map->value_align > base_alignment) base_alignment = map->value_align;
    if (fhmap__add_overflow(block_size, base_alignment - 1u, &raw_size))
        return false;

    allocation = FHMAP_MALLOC(map->user, raw_size);
    if (allocation == NULL) return false;
    raw_address = (uintptr_t)allocation;
    aligned_address = (raw_address + (base_alignment - 1u)) &
                      ~(uintptr_t)(base_alignment - 1u);

    map->allocation = allocation;
    map->control = (unsigned char *)aligned_address;
    map->keys = map->control + key_offset;
    map->values = map->control + value_offset;
    map->capacity = capacity;
    map->growth_limit = fhmap__growth_limit(capacity);
    map->key_stride = key_stride;
    map->value_stride = value_stride;
    map->allocation_bytes = raw_size;
    memset(map->control, FHMAP_CONTROL_EMPTY, capacity + FHMAP_GROUP_WIDTH);
    return true;
}

static bool fhmap__rehash(fhmap *map, size_t new_capacity)
{
    fhmap replacement;
    size_t i;
    memset(&replacement, 0, sizeof(replacement));
    replacement.key_size = map->key_size;
    replacement.value_size = map->value_size;
    replacement.key_align = map->key_align;
    replacement.value_align = map->value_align;
    replacement.seed = map->seed;
    replacement.hash = map->hash;
    replacement.equal = map->equal;
    replacement.user = map->user;
    replacement.generation = map->generation + 1u;

    if (!fhmap__allocate_exact(&replacement, new_capacity)) return false;

    for (i = 0; i < map->capacity; ++i) {
        unsigned char c = map->control[i];
        if (c < FHMAP_CONTROL_EMPTY) {
            const void *old_key = fhmap__key_at(map, i);
            uint64_t hash = fhmap__hash_key(map, old_key);
            size_t target = fhmap__find_insert_slot(&replacement, hash);
            if (target == FHMAP_NPOS) {
                FHMAP_FREE(replacement.user, replacement.allocation);
                return false;
            }
            fhmap__set_control(&replacement, target,
                               (unsigned char)(hash & 0x7fu));
            memcpy(fhmap__key_at(&replacement, target), old_key, map->key_size);
            memcpy(fhmap__value_at(&replacement, target),
                   fhmap__value_at(map, i), map->value_size);
            ++replacement.size;
        }
    }

    FHMAP_FREE(map->user, map->allocation);
    *map = replacement;
    return true;
}

FHMAP_API bool fhmap_init(fhmap *map, const fhmap_config *config)
{
    fhmap fresh;
    size_t capacity;
    if (map == NULL || config == NULL) return false;
    memset(map, 0, sizeof(*map));
    if (config->key_size == 0u || config->value_size == 0u) return false;
    if (!fhmap__is_power_of_two(config->key_align) ||
        !fhmap__is_power_of_two(config->value_align) ||
        config->key_align > FHMAP_MAX_ALIGNMENT ||
        config->value_align > FHMAP_MAX_ALIGNMENT)
        return false;
    if (!fhmap__capacity_for(config->initial_capacity, &capacity)) return false;

    memset(&fresh, 0, sizeof(fresh));
    fresh.key_size = config->key_size;
    fresh.value_size = config->value_size;
    fresh.key_align = config->key_align;
    fresh.value_align = config->value_align;
    fresh.seed = config->seed;
    fresh.hash = config->hash != NULL ? config->hash : fhmap_hash_bytes;
    fresh.equal = config->equal;
    fresh.user = config->user;
    fresh.generation = 1;
    if (!fhmap__allocate_exact(&fresh, capacity)) return false;
    *map = fresh;
    return true;
}

FHMAP_API void fhmap_destroy(fhmap *map)
{
    if (map == NULL) return;
    if (map->allocation != NULL) FHMAP_FREE(map->user, map->allocation);
    memset(map, 0, sizeof(*map));
}

FHMAP_API void fhmap_clear(fhmap *map)
{
    if (map == NULL || map->allocation == NULL) return;
    memset(map->control, FHMAP_CONTROL_EMPTY,
           map->capacity + FHMAP_GROUP_WIDTH);
    map->size = 0;
    map->tombstones = 0;
    ++map->generation;
}

FHMAP_API bool fhmap_reserve(fhmap *map, size_t element_capacity)
{
    size_t capacity;
    if (map == NULL || map->allocation == NULL) return false;
    if (element_capacity <= map->growth_limit) return true;
    if (!fhmap__capacity_for(element_capacity, &capacity)) return false;
    return fhmap__rehash(map, capacity);
}

FHMAP_API bool fhmap_compact(fhmap *map)
{
    size_t capacity;
    if (map == NULL || map->allocation == NULL) return false;
    if (!fhmap__capacity_for(map->size, &capacity)) return false;
    if (capacity == map->capacity && map->tombstones == 0u) return true;
    return fhmap__rehash(map, capacity);
}

FHMAP_API const void *fhmap_get_const(const fhmap *map, const void *key)
{
    uint64_t hash;
    size_t index;
    if (map == NULL || map->allocation == NULL || key == NULL || map->size == 0u)
        return NULL;
    hash = fhmap__hash_key(map, key);
    index = fhmap__find_index_hashed(map, key, hash);
    return index == FHMAP_NPOS ? NULL : fhmap__value_at(map, index);
}

FHMAP_API void *fhmap_get(fhmap *map, const void *key)
{
    return (void *)fhmap_get_const(map, key);
}

FHMAP_API bool fhmap_contains(const fhmap *map, const void *key)
{
    return fhmap_get_const(map, key) != NULL;
}

FHMAP_API void *fhmap_emplace(fhmap *map, const void *key, bool *inserted)
{
    uint64_t hash;
    size_t index;
    size_t occupied;
    if (inserted != NULL) *inserted = false;
    if (map == NULL || map->allocation == NULL || key == NULL) return NULL;

    hash = fhmap__hash_key(map, key);
    index = fhmap__find_index_hashed(map, key, hash);
    if (index != FHMAP_NPOS) return fhmap__value_at(map, index);

    if (map->tombstones > map->capacity / 4u &&
        map->tombstones > map->size) {
        if (!fhmap__rehash(map, map->capacity)) return NULL;
        hash = fhmap__hash_key(map, key);
    }

    occupied = map->size + map->tombstones;
    if (occupied >= map->growth_limit) {
        size_t target = map->capacity;
        if (map->size + 1u > map->growth_limit) {
            if (map->capacity > SIZE_MAX / 2u) return NULL;
            target = map->capacity * 2u;
        }
        if (!fhmap__rehash(map, target)) return NULL;
        hash = fhmap__hash_key(map, key);
    }

    index = fhmap__find_insert_slot(map, hash);
    if (index == FHMAP_NPOS) return NULL;
    if (map->control[index] == FHMAP_CONTROL_DELETED) --map->tombstones;
    fhmap__set_control(map, index, (unsigned char)(hash & 0x7fu));
    memcpy(fhmap__key_at(map, index), key, map->key_size);
    ++map->size;
    ++map->generation;
    if (inserted != NULL) *inserted = true;
    return fhmap__value_at(map, index);
}

FHMAP_API fhmap_put_result fhmap_put(fhmap *map, const void *key,
                                    const void *value)
{
    bool inserted = false;
    void *destination;
    if (value == NULL) return FHMAP_PUT_ERROR;
    destination = fhmap_emplace(map, key, &inserted);
    if (destination == NULL) return FHMAP_PUT_ERROR;
    memmove(destination, value, map->value_size);
    return inserted ? FHMAP_PUT_INSERTED : FHMAP_PUT_UPDATED;
}

FHMAP_API bool fhmap_remove_copy(fhmap *map, const void *key, void *value_out)
{
    uint64_t hash;
    size_t index;
    if (map == NULL || map->allocation == NULL || key == NULL || map->size == 0u)
        return false;
    hash = fhmap__hash_key(map, key);
    index = fhmap__find_index_hashed(map, key, hash);
    if (index == FHMAP_NPOS) return false;
    if (value_out != NULL) {
        memmove(value_out, fhmap__value_at(map, index), map->value_size);
    }
    fhmap__set_control(map, index, FHMAP_CONTROL_DELETED);
    --map->size;
    ++map->tombstones;
    ++map->generation;
    if (map->size == 0u) {
        memset(map->control, FHMAP_CONTROL_EMPTY,
               map->capacity + FHMAP_GROUP_WIDTH);
        map->tombstones = 0;
    }
    return true;
}

FHMAP_API bool fhmap_remove(fhmap *map, const void *key)
{
    return fhmap_remove_copy(map, key, NULL);
}

FHMAP_API size_t fhmap_size(const fhmap *map)
{
    return map == NULL ? 0u : map->size;
}

FHMAP_API size_t fhmap_capacity(const fhmap *map)
{
    return map == NULL ? 0u : map->capacity;
}

FHMAP_API size_t fhmap_memory_usage(const fhmap *map)
{
    return map == NULL ? 0u : map->allocation_bytes;
}

FHMAP_API double fhmap_load_factor(const fhmap *map)
{
    if (map == NULL || map->capacity == 0u) return 0.0;
    return (double)map->size / (double)map->capacity;
}

FHMAP_API fhmap_iter fhmap_iter_begin(const fhmap *map)
{
    fhmap_iter iter;
    iter.next_index = 0;
    iter.generation = map == NULL ? 0u : map->generation;
    return iter;
}

FHMAP_API bool fhmap_iter_next(fhmap *map, fhmap_iter *iter,
                               const void **key_out, void **value_out)
{
    if (map == NULL || iter == NULL || iter->generation != map->generation)
        return false;
    while (iter->next_index < map->capacity) {
        size_t index = iter->next_index++;
        if (map->control[index] < FHMAP_CONTROL_EMPTY) {
            if (key_out != NULL) *key_out = fhmap__key_at(map, index);
            if (value_out != NULL) *value_out = fhmap__value_at(map, index);
            return true;
        }
    }
    return false;
}

#undef FHMAP_GROUP_WIDTH
#undef FHMAP_MIN_CAPACITY
#undef FHMAP_CONTROL_EMPTY
#undef FHMAP_CONTROL_DELETED
#undef FHMAP_NPOS
#undef FHMAP_HAS_SSE2
#undef FHMAP_LIKELY
#undef FHMAP_UNLIKELY
#undef FHMAP_INLINE

#endif /* FHMAP_IMPLEMENTATION */
