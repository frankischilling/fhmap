#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define FHMAP_IMPLEMENTATION
#include "../fhmap.h"

FHMAP_DECLARE(bench_map, uint64_t, uint64_t);

static uint64_t mix64(uint64_t x)
{
    x ^= x >> 30;
    x *= UINT64_C(0xbf58476d1ce4e5b9);
    x ^= x >> 27;
    x *= UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
}

static double seconds_now(void)
{
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time) != 0) return 0.0;
    return (double)time.tv_sec + (double)time.tv_nsec * 1e-9;
}

static size_t resident_bytes(void)
{
    unsigned long total_pages = 0;
    unsigned long resident_pages = 0;
    long page_size = sysconf(_SC_PAGESIZE);
    FILE *file = fopen("/proc/self/statm", "r");
    if (file == NULL) return 0;
    if (fscanf(file, "%lu %lu", &total_pages, &resident_pages) != 2)
        resident_pages = 0;
    fclose(file);
    (void)total_pages;
    return resident_pages > 0 && page_size > 0
               ? (size_t)resident_pages * (size_t)page_size
               : 0;
}

static void print_rate(const char *name, size_t operations, double seconds)
{
    double millions = seconds > 0.0 ? (double)operations / seconds / 1e6 : 0.0;
    printf("%-20s %9.2f Mops/s  (%7.3f s)\n", name, millions, seconds);
}

int main(int argc, char **argv)
{
    size_t count = 1000000;
    uint64_t *keys;
    bench_map map;
    uint64_t checksum = 0;
    double start, elapsed;
    size_t i;

    if (argc > 1) {
        char *end = NULL;
        uintmax_t parsed;
        errno = 0;
        parsed = strtoumax(argv[1], &end, 10);
        if (errno != 0 || end == argv[1] || *end != '\0' || parsed == 0 ||
            parsed > SIZE_MAX / sizeof(*keys)) {
            fprintf(stderr, "invalid element count: %s\n", argv[1]);
            return 2;
        }
        count = (size_t)parsed;
    }

    keys = (uint64_t *)malloc(count * sizeof(*keys));
    if (keys == NULL) {
        fputs("could not allocate key array\n", stderr);
        return 1;
    }
    for (i = 0; i < count; ++i) keys[i] = (uint64_t)i * 2u;
    for (i = count; i > 1; --i) {
        size_t j = (size_t)(mix64((uint64_t)i) % i);
        uint64_t temporary = keys[i - 1u];
        keys[i - 1u] = keys[j];
        keys[j] = temporary;
    }

    if (!bench_map_init_seed(&map, count, UINT64_C(0x243f6a8885a308d3))) {
        fputs("could not allocate map\n", stderr);
        free(keys);
        return 1;
    }

    printf("fhmap benchmark: %zu uint64_t -> uint64_t entries\n", count);
    printf("capacity: %zu buckets, table allocation: %.2f MiB\n",
           bench_map_capacity(&map),
           (double)bench_map_memory_usage(&map) / (1024.0 * 1024.0));

    start = seconds_now();
    for (i = 0; i < count; ++i) {
        uint64_t value = mix64(keys[i]);
        if (bench_map_put(&map, keys[i], value) == FHMAP_PUT_ERROR) {
            fputs("insert failed\n", stderr);
            bench_map_destroy(&map);
            free(keys);
            return 1;
        }
    }
    elapsed = seconds_now() - start;
    print_rate("insert", count, elapsed);

    start = seconds_now();
    for (i = 0; i < count; ++i) {
        const uint64_t *value = bench_map_get_const(&map, keys[i]);
        if (value == NULL) {
            fputs("unexpected lookup miss\n", stderr);
            return 1;
        }
        checksum ^= *value + (uint64_t)i;
    }
    elapsed = seconds_now() - start;
    print_rate("lookup hit", count, elapsed);

    start = seconds_now();
    for (i = 0; i < count; ++i) {
        uint64_t missing = keys[i] + 1u;
        checksum ^= (uint64_t)(bench_map_get_const(&map, missing) != NULL);
    }
    elapsed = seconds_now() - start;
    print_rate("lookup miss", count, elapsed);

    {
        bench_map_iter iter = bench_map_iter_begin(&map);
        const uint64_t *key;
        uint64_t *value;
        size_t seen = 0;
        start = seconds_now();
        while (bench_map_iter_next(&map, &iter, &key, &value)) {
            checksum ^= *key ^ *value;
            ++seen;
        }
        elapsed = seconds_now() - start;
        if (seen != count) {
            fputs("iteration count mismatch\n", stderr);
            return 1;
        }
        print_rate("iterate", count, elapsed);
    }

    start = seconds_now();
    for (i = 0; i < count; ++i) {
        if (!bench_map_remove(&map, keys[i])) {
            fputs("erase failed\n", stderr);
            return 1;
        }
    }
    elapsed = seconds_now() - start;
    print_rate("erase", count, elapsed);

    printf("resident set after run: %.2f MiB; checksum: 0x%016" PRIx64 "\n",
           (double)resident_bytes() / (1024.0 * 1024.0), checksum);

    bench_map_destroy(&map);
    free(keys);
    return 0;
}
