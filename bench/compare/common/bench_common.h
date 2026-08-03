#ifndef FHMAP_BENCH_COMMON_H_INCLUDED
#define FHMAP_BENCH_COMMON_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BENCH_DATASET_ZERO {0}

typedef struct bench_dataset {
    size_t count;
    uint64_t seed;
    uint64_t *u64_keys;
    uint64_t *u64_misses;
    char **str_keys;
    char **str_misses;
    char *string_storage;
} bench_dataset;

uint64_t bench_mix64(uint64_t value);
bool bench_dataset_init(bench_dataset *data, size_t count, uint64_t seed);
void bench_dataset_destroy(bench_dataset *data);
double bench_seconds_now(void);
double bench_median(double *values, size_t count);
double bench_mad(double *values, size_t count, double median);

#endif
