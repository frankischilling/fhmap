#ifndef FHMAP_BENCH_ADAPTER_H_INCLUDED
#define FHMAP_BENCH_ADAPTER_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum bench_key_kind {
    BENCH_KEYS_U64,
    BENCH_KEYS_CSTR
} bench_key_kind;

typedef struct bench_map_ops {
    const char *name;
    void *(*create)(bench_key_kind kind, size_t expected);
    bool (*reserve)(void *map, size_t expected);
    bool (*put_u64)(void *map, uint64_t key, uint64_t value, bool *inserted);
    bool (*get_u64)(void *map, uint64_t key, uint64_t *value);
    bool (*remove_u64)(void *map, uint64_t key);
    bool (*put_cstr)(void *map, const char *key, uint64_t value, bool *inserted);
    bool (*get_cstr)(void *map, const char *key, uint64_t *value);
    bool (*remove_cstr)(void *map, const char *key);
    bool (*iterate)(void *map, uint64_t *checksum, size_t *seen);
    size_t (*size)(const void *map);
    size_t (*capacity)(const void *map);
    void (*destroy)(void *map);
} bench_map_ops;

static inline uint64_t bench_checksum_add(uint64_t checksum, uint64_t value)
{
    value ^= value >> 30;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31;
    return checksum + value;
}

extern const bench_map_ops bench_adapter;

#endif
