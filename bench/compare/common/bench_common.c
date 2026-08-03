#define _POSIX_C_SOURCE 200809L

#include "bench_common.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { BENCH_STRING_BYTES = 17 };

static bool bench_size_multiply(size_t left, size_t right, size_t *result)
{
    if (left != 0 && right > SIZE_MAX / left) return false;
    *result = left * right;
    return true;
}

static uint64_t bench_xorshift64(uint64_t *state)
{
    uint64_t value = *state;

    value ^= value << 13;
    value ^= value >> 7;
    value ^= value << 17;
    *state = value;
    return value;
}

static int bench_compare_double(const void *left_pointer,
                                const void *right_pointer)
{
    const double left = *(const double *)left_pointer;
    const double right = *(const double *)right_pointer;
    return (left > right) - (left < right);
}

static double bench_sorted_median(double *values, size_t count)
{
    const size_t middle = count / 2u;

    qsort(values, count, sizeof(*values), bench_compare_double);
    if ((count & 1u) != 0) return values[middle];
    return (double)(((long double)values[middle - 1u] +
                     (long double)values[middle]) /
                    2.0L);
}

uint64_t bench_mix64(uint64_t value)
{
    value ^= value >> 30;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27;
    value *= UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31);
}

bool bench_dataset_init(bench_dataset *data, size_t count, uint64_t seed)
{
    bench_dataset result = BENCH_DATASET_ZERO;
    uint64_t random_state;
    size_t integer_bytes;
    size_t pointer_bytes;
    size_t string_count;
    size_t storage_bytes;
    size_t i;

    if (data == NULL) return false;
    memset(data, 0, sizeof(*data));
    if (count == 0 ||
        (uint64_t)(count - 1u) > UINT64_MAX / UINT64_C(2) ||
        !bench_size_multiply(count, sizeof(*result.u64_keys),
                             &integer_bytes) ||
        !bench_size_multiply(count, sizeof(*result.str_keys),
                             &pointer_bytes) ||
        !bench_size_multiply(count, 2u, &string_count) ||
        !bench_size_multiply(string_count, BENCH_STRING_BYTES,
                             &storage_bytes)) {
        return false;
    }

    result.u64_keys = (uint64_t *)malloc(integer_bytes);
    result.u64_misses = (uint64_t *)malloc(integer_bytes);
    result.str_keys = (char **)malloc(pointer_bytes);
    result.str_misses = (char **)malloc(pointer_bytes);
    result.string_storage = (char *)malloc(storage_bytes);
    if (result.u64_keys == NULL || result.u64_misses == NULL ||
        result.str_keys == NULL || result.str_misses == NULL ||
        result.string_storage == NULL) {
        bench_dataset_destroy(&result);
        return false;
    }

    result.count = count;
    result.seed = seed;
    for (i = 0; i < count; ++i) {
        result.u64_keys[i] = (uint64_t)i * UINT64_C(2);
        result.u64_misses[i] = result.u64_keys[i] + UINT64_C(1);
    }

    random_state = seed ^ UINT64_C(0xd1b54a32d192ed03);
    if (random_state == 0) random_state = UINT64_C(0x9e3779b97f4a7c15);
    for (i = count; i > 1u; --i) {
        const size_t swap_index = (size_t)(bench_xorshift64(&random_state) %
                                                (uint64_t)i);
        const uint64_t key = result.u64_keys[i - 1u];
        const uint64_t miss = result.u64_misses[i - 1u];

        result.u64_keys[i - 1u] = result.u64_keys[swap_index];
        result.u64_misses[i - 1u] = result.u64_misses[swap_index];
        result.u64_keys[swap_index] = key;
        result.u64_misses[swap_index] = miss;
    }

    for (i = 0; i < count; ++i) {
        char *key_string = result.string_storage + i * BENCH_STRING_BYTES;
        char *miss_string = result.string_storage +
                            (count + i) * BENCH_STRING_BYTES;
        const int key_length = snprintf(key_string, BENCH_STRING_BYTES,
                                        "%016" PRIx64,
                                        bench_mix64(result.u64_keys[i]));
        const int miss_length = snprintf(miss_string, BENCH_STRING_BYTES,
                                         "%016" PRIx64,
                                         bench_mix64(result.u64_misses[i]));

        if (key_length != BENCH_STRING_BYTES - 1 ||
            miss_length != BENCH_STRING_BYTES - 1) {
            bench_dataset_destroy(&result);
            return false;
        }
        result.str_keys[i] = key_string;
        result.str_misses[i] = miss_string;
    }

    *data = result;
    return true;
}

void bench_dataset_destroy(bench_dataset *data)
{
    if (data == NULL) return;
    free(data->string_storage);
    free(data->str_misses);
    free(data->str_keys);
    free(data->u64_misses);
    free(data->u64_keys);
    memset(data, 0, sizeof(*data));
}

double bench_seconds_now(void)
{
    struct timespec current;

    if (clock_gettime(CLOCK_MONOTONIC, &current) != 0) return 0.0;
    return (double)current.tv_sec + (double)current.tv_nsec * 1e-9;
}

double bench_median(double *values, size_t count)
{
    double *copy;
    double result;
    size_t bytes;

    if (values == NULL || count == 0 ||
        !bench_size_multiply(count, sizeof(*copy), &bytes)) {
        return 0.0;
    }
    copy = (double *)malloc(bytes);
    if (copy == NULL) return 0.0;
    memcpy(copy, values, bytes);
    result = bench_sorted_median(copy, count);
    free(copy);
    return result;
}

double bench_mad(double *values, size_t count, double median)
{
    double *deviations;
    double result;
    size_t bytes;
    size_t i;

    if (values == NULL || count == 0 ||
        !bench_size_multiply(count, sizeof(*deviations), &bytes)) {
        return 0.0;
    }
    deviations = (double *)malloc(bytes);
    if (deviations == NULL) return 0.0;
    for (i = 0; i < count; ++i) {
        deviations[i] = values[i] >= median ? values[i] - median
                                             : median - values[i];
    }
    result = bench_sorted_median(deviations, count);
    free(deviations);
    return result;
}
