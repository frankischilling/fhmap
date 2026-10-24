#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FHMAP_IMPLEMENTATION
#include "../fhmap.h"

FHMAP_DECLARE(u64_map, uint64_t, uint64_t);

typedef struct aligned_value {
    long double amount;
    void *pointer;
} aligned_value;

FHMAP_DECLARE(aligned_map, uint32_t, aligned_value);

static uint64_t constant_hash(const void *key, size_t key_size,
                              uint64_t seed, void *user)
{
    (void)key;
    (void)key_size;
    (void)seed;
    (void)user;
    return UINT64_C(0x123456789abcdef0);
}

static uint64_t rng_state = UINT64_C(0x6a09e667f3bcc909);

static uint64_t random_u64(void)
{
    uint64_t x = rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng_state = x;
    return x * UINT64_C(0x2545f4914f6cdd1d);
}

static void test_basic_typed_api(void)
{
    enum { COUNT = 100000 };
    u64_map map;
    size_t i;
    size_t iterated = 0;
    uint64_t sum = 0;

    assert(u64_map_init_seed(&map, 0, UINT64_C(0xfeedface12345678)));
    assert(u64_map_size(&map) == 0);
    assert(u64_map_capacity(&map) >= 16);

    for (i = 0; i < COUNT; ++i) {
        uint64_t key = (uint64_t)i;
        uint64_t value = key * 3u + 1u;
        assert(u64_map_put(&map, key, value) == FHMAP_PUT_INSERTED);
    }
    assert(u64_map_size(&map) == COUNT);

    for (i = 0; i < COUNT; ++i) {
        const uint64_t *value = u64_map_get_const(&map, (uint64_t)i);
        assert(value != NULL && *value == (uint64_t)i * 3u + 1u);
    }
    assert(u64_map_get(&map, UINT64_C(9000000)) == NULL);

    for (i = 0; i < COUNT; i += 2) {
        assert(u64_map_put(&map, (uint64_t)i, UINT64_C(777)) ==
               FHMAP_PUT_UPDATED);
    }
    assert(u64_map_size(&map) == COUNT);

    for (i = 0; i < COUNT; i += 3) {
        uint64_t removed = 0;
        assert(u64_map_remove_copy(&map, (uint64_t)i, &removed));
        assert(!u64_map_contains(&map, (uint64_t)i));
        assert(!u64_map_remove(&map, (uint64_t)i));
    }

    {
        u64_map_iter it = u64_map_iter_begin(&map);
        const uint64_t *key;
        uint64_t *value;
        while (u64_map_iter_next(&map, &it, &key, &value)) {
            assert((*key % 3u) != 0u);
            sum ^= *key + *value;
            ++iterated;
        }
    }
    assert(iterated == u64_map_size(&map));
    assert(sum != UINT64_C(0xffffffffffffffff));

    assert(u64_map_reserve(&map, 250000));
    assert(map.raw.growth_limit >= 250000);
    assert(u64_map_compact(&map));
    assert(map.raw.tombstones == 0);
    assert(u64_map_memory_usage(&map) > 0);

    u64_map_clear(&map);
    assert(u64_map_size(&map) == 0);
    assert(u64_map_get(&map, 1) == NULL);
    u64_map_destroy(&map);
    assert(map.raw.allocation == NULL);
}

static void test_emplace_and_alignment(void)
{
    aligned_map map;
    uint32_t key = 19;
    aligned_value *value;
    bool inserted = false;

    assert(aligned_map_init_seed(&map, 8, 7));
    value = aligned_map_emplace(&map, key, &inserted);
    assert(value != NULL && inserted);
    assert(((uintptr_t)value % _Alignof(aligned_value)) == 0);
    value->amount = 12.5L;
    value->pointer = &map;

    inserted = true;
    value = aligned_map_emplace(&map, key, &inserted);
    assert(value != NULL && !inserted);
    assert(value->amount == 12.5L && value->pointer == &map);
    aligned_map_destroy(&map);
}

static void test_extreme_collisions_and_tombstones(void)
{
    enum { COUNT = 2000 };
    fhmap map;
    fhmap_config config = fhmap_config_make(sizeof(uint32_t), sizeof(uint64_t));
    uint32_t i;

    config.key_align = _Alignof(uint32_t);
    config.value_align = _Alignof(uint64_t);
    config.hash = constant_hash;
    config.seed = 0;
    assert(fhmap_init(&map, &config));

    for (i = 0; i < COUNT; ++i) {
        uint64_t value = (uint64_t)i * (uint64_t)i;
        assert(fhmap_put(&map, &i, &value) == FHMAP_PUT_INSERTED);
    }
    for (i = 0; i < COUNT; i += 2) assert(fhmap_remove(&map, &i));
    for (i = 1; i < COUNT; i += 2) {
        const uint64_t *value = (const uint64_t *)fhmap_get_const(&map, &i);
        assert(value != NULL && *value == (uint64_t)i * (uint64_t)i);
    }
    for (i = 0; i < COUNT; i += 2) {
        uint64_t value = UINT64_C(0xdeadbeef00000000) | i;
        assert(fhmap_put(&map, &i, &value) == FHMAP_PUT_INSERTED);
    }
    assert(fhmap_size(&map) == COUNT);
    assert(fhmap_compact(&map));
    assert(map.tombstones == 0);
    fhmap_destroy(&map);
}

static void test_borrowed_strings(void)
{
    static const char *words[] = {"amber", "blue", "coral", "dahlia", "ember"};
    fhmap map;
    fhmap_config config = fhmap_config_make(sizeof(const char *), sizeof(int));
    size_t i;

    config.key_align = _Alignof(const char *);
    config.value_align = _Alignof(int);
    config.hash = fhmap_hash_cstr;
    config.equal = fhmap_equal_cstr;
    assert(fhmap_init(&map, &config));

    for (i = 0; i < sizeof(words) / sizeof(words[0]); ++i) {
        const char *key = words[i];
        int value = (int)i + 10;
        assert(fhmap_put(&map, &key, &value) == FHMAP_PUT_INSERTED);
    }
    {
        char query_buffer[] = "coral";
        const char *query = query_buffer;
        const int *value = (const int *)fhmap_get_const(&map, &query);
        assert(value != NULL && *value == 12);
    }
    fhmap_destroy(&map);
}

static void test_iterator_invalidation(void)
{
    u64_map map;
    u64_map_iter it;
    const uint64_t *key;
    uint64_t *value;

    assert(u64_map_init_seed(&map, 2, 10));
    assert(u64_map_put(&map, 1, 11) == FHMAP_PUT_INSERTED);
    it = u64_map_iter_begin(&map);
    assert(u64_map_put(&map, 2, 22) == FHMAP_PUT_INSERTED);
    assert(!u64_map_iter_next(&map, &it, &key, &value));
    u64_map_destroy(&map);
}

static void test_differential_random_operations(void)
{
    enum { KEY_SPACE = 4096, OPERATIONS = 300000 };
    unsigned char present[KEY_SPACE];
    uint64_t oracle[KEY_SPACE];
    u64_map map;
    size_t expected_size = 0;
    size_t op;

    memset(present, 0, sizeof(present));
    memset(oracle, 0, sizeof(oracle));
    assert(u64_map_init_seed(&map, KEY_SPACE / 2, 99));

    for (op = 0; op < OPERATIONS; ++op) {
        uint64_t random = random_u64();
        uint64_t key = random % KEY_SPACE;
        unsigned action = (unsigned)((random >> 32) % 3u);
        if (action == 0u) {
            uint64_t value = random_u64();
            bool was_present = present[key] != 0;
            fhmap_put_result result = u64_map_put(&map, key, value);
            assert(result == (was_present ? FHMAP_PUT_UPDATED :
                                           FHMAP_PUT_INSERTED));
            if (!was_present) ++expected_size;
            present[key] = 1;
            oracle[key] = value;
        } else if (action == 1u) {
            bool removed = u64_map_remove(&map, key);
            assert(removed == (present[key] != 0));
            if (removed) {
                present[key] = 0;
                --expected_size;
            }
        } else {
            const uint64_t *value = u64_map_get_const(&map, key);
            assert((value != NULL) == (present[key] != 0));
            if (value != NULL) assert(*value == oracle[key]);
        }

        if ((op & 4095u) == 0u) assert(u64_map_size(&map) == expected_size);
    }

    assert(u64_map_size(&map) == expected_size);
    for (op = 0; op < KEY_SPACE; ++op) {
        const uint64_t *value = u64_map_get_const(&map, (uint64_t)op);
        assert((value != NULL) == (present[op] != 0));
        if (value != NULL) assert(*value == oracle[op]);
    }
    u64_map_destroy(&map);
}

/* Check every probe start, including loads through the mirrored tail. */
static uint64_t positioned_hash(const void *key, size_t key_size,
                                uint64_t seed, void *user)
{
    (void)key; (void)key_size; (void)seed;
    return *(const uint64_t *)user;
}

static void test_probe_boundaries(void)
{
    size_t start;
    for (start = 0; start < 64u; ++start) {
        uint64_t hash = ((uint64_t)start << 7u) | 37u;
        fhmap_config cfg = fhmap_config_make(sizeof(uint64_t), sizeof(uint64_t));
        fhmap map;
        uint64_t key;
        cfg.initial_capacity = 56;
        cfg.hash = positioned_hash;
        cfg.user = &hash;
        assert(fhmap_init(&map, &cfg));
        assert(map.capacity == 64u);
        for (key = 0; key < 48u; ++key)
            assert(fhmap_put(&map, &key, &key) == FHMAP_PUT_INSERTED);
        /* Holes in the first and subsequent groups must not hide duplicates. */
        for (key = 0; key < 48u; key += 7u) assert(fhmap_remove(&map, &key));
        for (key = 0; key < 48u; ++key) {
            bool erased = key % 7u == 0u;
            const uint64_t *value = fhmap_get_const(&map, &key);
            assert(erased ? value == NULL : value != NULL && *value == key);
            if (!erased) assert(fhmap_put(&map, &key, &key) == FHMAP_PUT_UPDATED);
        }
        key = 1000;
        assert(fhmap_get_const(&map, &key) == NULL);
        assert(fhmap_put(&map, &key, &key) == FHMAP_PUT_INSERTED);
        assert(*(uint64_t *)fhmap__key_at(&map, start) == key);
        assert(fhmap_compact(&map));
        assert(*(uint64_t *)fhmap_get(&map, &key) == key);
        fhmap_destroy(&map);
    }
}

static uint64_t identity_hash(const void *key, size_t size, uint64_t seed, void *user)
{
    uint64_t value;
    (void)size; (void)seed; (void)user;
    memcpy(&value, key, sizeof(value));
    return value;
}

static bool counted_equal(const void *stored, const void *query, size_t size, void *user)
{
    ++*(unsigned *)user;
    return memcmp(stored, query, size) == 0;
}

static void test_custom_equality_stops_at_empty(void)
{
    size_t start, offset;
    for (start = 0; start < 64; ++start) {
        for (offset = 1; offset < 16; ++offset) {
            fhmap map;
            unsigned comparisons = 0;
            fhmap_config cfg = fhmap_config_make(sizeof(uint64_t), sizeof(uint64_t));
            uint64_t stored = (uint64_t)((start + offset) % 64) * 128u + 37u;
            uint64_t missing = (uint64_t)start * 128u + 37u;
            cfg.initial_capacity = 56;
            cfg.hash = identity_hash;
            cfg.equal = counted_equal;
            cfg.user = &comparisons;
            assert(fhmap_init(&map, &cfg));
            assert(fhmap_put(&map, &stored, &stored) == FHMAP_PUT_INSERTED);
            assert(fhmap_get(&map, &missing) == NULL);
            assert(comparisons == 0);
            assert(*(uint64_t *)fhmap_get(&map, &stored) == stored);
            assert(comparisons == 1);
            fhmap_destroy(&map);
        }
    }
}

static void test_128bit_keys(void)
{
    fhmap map;
    fhmap_config cfg = fhmap_config_make(2 * sizeof(uint64_t), sizeof(uint64_t));
    uint64_t key[2], value;
    cfg.hash = constant_hash;
    cfg.seed = 17;
    assert(fhmap_init(&map, &cfg));
    for (value = 0; value < 128; ++value) {
        key[0] = value & 15u;
        key[1] = value >> 4u;
        assert(fhmap_put(&map, key, &value) == FHMAP_PUT_INSERTED);
    }
    for (value = 0; value < 128; ++value) {
        key[0] = value & 15u;
        key[1] = value >> 4u;
        assert(*(uint64_t *)fhmap_get(&map, key) == value);
        key[1] += 16;
        assert(fhmap_get(&map, key) == NULL);
        key[0] += 16; key[1] -= 16;
        assert(fhmap_get(&map, key) == NULL);
    }
    fhmap_destroy(&map);
}

static void test_noop_lifetime(void)
{
    u64_map map;
    fhmap_iter it;
    size_t generation;
    assert(u64_map_init_seed(&map, 1, 17));
    assert(u64_map_put(&map, 1, 2) == FHMAP_PUT_INSERTED);
    generation = map.raw.generation;
    it = u64_map_iter_begin(&map);
    assert(!fhmap_init(&map.raw, NULL));
    assert(u64_map_reserve(&map, 1));
    assert(u64_map_compact(&map));
    assert(!u64_map_remove(&map, 9));
    assert(u64_map_put(&map, 1, 3) == FHMAP_PUT_UPDATED);
    assert(map.raw.generation == generation);
    assert(u64_map_iter_next(&map, &it, NULL, NULL));
    assert(*u64_map_get(&map, 1) == 3);
    u64_map_destroy(&map);
}

static void test_available_masks(void)
{
    unsigned char control[32];
    unsigned offset, mask, bit;
    for (offset = 0; offset < 16u; ++offset) {
        for (mask = 0; mask < 65536u; ++mask) {
            for (bit = 0; bit < 16u; ++bit)
                control[offset + bit] = (unsigned char)((mask & (1u << bit))
                    ? (bit & 1u ? 0xfeu : 0x80u) : (bit * 7u));
            assert(fhmap__group_available(control + offset) == mask);
        }
    }
}

int main(void)
{
    test_custom_equality_stops_at_empty();
    test_128bit_keys();
    test_noop_lifetime();
    test_available_masks();
    test_probe_boundaries();
    test_basic_typed_api();
    test_emplace_and_alignment();
    test_extreme_collisions_and_tombstones();
    test_borrowed_strings();
    test_iterator_invalidation();
    test_differential_random_operations();
    puts("fhmap: all tests passed");
    return 0;
}
