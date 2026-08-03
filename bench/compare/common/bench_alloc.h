#ifndef FHMAP_BENCH_ALLOC_H_INCLUDED
#define FHMAP_BENCH_ALLOC_H_INCLUDED

#include <stddef.h>

typedef struct bench_alloc_stats {
    size_t current_bytes;
    size_t peak_bytes;
    size_t allocations;
    size_t frees;
} bench_alloc_stats;

void bench_alloc_reset(void);
void bench_alloc_begin(void);
bench_alloc_stats bench_alloc_end(void);
bench_alloc_stats bench_alloc_snapshot(void);
void *bench_malloc(size_t size);
void *bench_calloc(size_t count, size_t size);
void *bench_realloc(void *pointer, size_t size);
void bench_free(void *pointer);

#endif
