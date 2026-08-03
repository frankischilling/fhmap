#include "bench_adapter.h"
#include "bench_alloc.h"
#include "bench_common.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum bench_workload {
    BENCH_INSERT_RESERVED,
    BENCH_INSERT_GROWING,
    BENCH_UPDATE,
    BENCH_LOOKUP_HIT,
    BENCH_LOOKUP_MISS,
    BENCH_ERASE,
    BENCH_ITERATE,
    BENCH_WORKLOAD_COUNT
} bench_workload;

typedef struct bench_options {
    size_t count;
    size_t samples;
    uint64_t seed;
    bool smoke;
} bench_options;

typedef struct bench_sample_result {
    uint64_t checksum;
    size_t final_size;
    bench_alloc_stats memory;
    size_t capacity;
} bench_sample_result;

static const char *const bench_workload_names[BENCH_WORKLOAD_COUNT] = {
    "insert_reserved",
    "insert_growing",
    "update",
    "lookup_hit",
    "lookup_miss",
    "erase",
    "iterate"
};

static const char *bench_key_kind_name(bench_key_kind kind)
{
    return kind == BENCH_KEYS_U64 ? "u64" : "cstr";
}

static bool bench_parse_u64(const char *text, uint64_t limit,
                            uint64_t *result)
{
    uint64_t value = 0;
    const unsigned char *cursor = (const unsigned char *)text;

    if (text == NULL || *text == '\0' || result == NULL) return false;
    while (*cursor != '\0') {
        const uint64_t digit = (uint64_t)(*cursor - (unsigned char)'0');

        if (*cursor < (unsigned char)'0' || *cursor > (unsigned char)'9')
            return false;
        if (value > (limit - digit) / UINT64_C(10)) return false;
        value = value * UINT64_C(10) + digit;
        ++cursor;
    }
    if (value == 0) return false;
    *result = value;
    return true;
}

static bool bench_parse_options(int argc, char **argv, bench_options *options)
{
    bench_options parsed = {
        262144u,
        9u,
        UINT64_C(0x6a09e667f3bcc909),
        false
    };
    bool count_seen = false;
    bool samples_seen = false;
    bool seed_seen = false;
    bool smoke_seen = false;
    int index;

    if (options == NULL) return false;
    for (index = 1; index < argc; ++index) {
        const char *option = argv[index];
        uint64_t value;

        if (strcmp(option, "--smoke") == 0) {
            if (smoke_seen) goto invalid;
            smoke_seen = true;
            parsed.smoke = true;
            continue;
        }
        if (strcmp(option, "--count") == 0) {
            if (count_seen || index + 1 >= argc ||
                !bench_parse_u64(argv[++index], (uint64_t)SIZE_MAX, &value)) {
                goto invalid;
            }
            count_seen = true;
            parsed.count = (size_t)value;
            continue;
        }
        if (strcmp(option, "--samples") == 0) {
            if (samples_seen || index + 1 >= argc ||
                !bench_parse_u64(argv[++index], (uint64_t)SIZE_MAX, &value)) {
                goto invalid;
            }
            samples_seen = true;
            parsed.samples = (size_t)value;
            continue;
        }
        if (strcmp(option, "--seed") == 0) {
            if (seed_seen || index + 1 >= argc ||
                !bench_parse_u64(argv[++index], UINT64_MAX, &value)) {
                goto invalid;
            }
            seed_seen = true;
            parsed.seed = value;
            continue;
        }
        goto invalid;
    }

    if (parsed.smoke) {
        if (!count_seen) parsed.count = 32u;
        if (!samples_seen) parsed.samples = 3u;
    }
    *options = parsed;
    return true;

invalid:
    fputs("invalid benchmark arguments\n", stderr);
    return false;
}

static bool bench_adapter_is_complete(void)
{
    return bench_adapter.name != NULL && bench_adapter.name[0] != '\0' &&
           bench_adapter.create != NULL && bench_adapter.reserve != NULL &&
           bench_adapter.put_u64 != NULL && bench_adapter.get_u64 != NULL &&
           bench_adapter.remove_u64 != NULL &&
           bench_adapter.put_cstr != NULL &&
           bench_adapter.get_cstr != NULL &&
           bench_adapter.remove_cstr != NULL &&
           bench_adapter.iterate != NULL && bench_adapter.size != NULL &&
           bench_adapter.capacity != NULL && bench_adapter.destroy != NULL;
}

static uint64_t bench_value_at(const bench_dataset *data, size_t index,
                               bool updated)
{
    const uint64_t version = updated
                                 ? UINT64_C(0xa4093822299f31d0)
                                 : UINT64_C(0x13198a2e03707344);

    return bench_mix64(data->u64_keys[index] ^ data->seed ^ version);
}

static bool bench_put_item(void *map, bench_key_kind kind,
                           const bench_dataset *data, size_t index,
                           uint64_t value, bool *inserted)
{
    if (kind == BENCH_KEYS_U64) {
        return bench_adapter.put_u64(map, data->u64_keys[index], value,
                                     inserted);
    }
    return bench_adapter.put_cstr(map, data->str_keys[index], value,
                                  inserted);
}

static bool bench_get_item(void *map, bench_key_kind kind,
                           const bench_dataset *data, size_t index,
                           bool miss, uint64_t *value)
{
    if (kind == BENCH_KEYS_U64) {
        const uint64_t key = miss ? data->u64_misses[index]
                                  : data->u64_keys[index];
        return bench_adapter.get_u64(map, key, value);
    }
    return bench_adapter.get_cstr(map, miss ? data->str_misses[index]
                                            : data->str_keys[index],
                                  value);
}

static bool bench_remove_item(void *map, bench_key_kind kind,
                              const bench_dataset *data, size_t index)
{
    if (kind == BENCH_KEYS_U64) {
        return bench_adapter.remove_u64(map, data->u64_keys[index]);
    }
    return bench_adapter.remove_cstr(map, data->str_keys[index]);
}

static bool bench_wrong_kind_is_safe(void *map, bench_key_kind kind)
{
    bool inserted = true;
    uint64_t value = 0;

    if (kind == BENCH_KEYS_U64) {
        if (bench_adapter.put_cstr(map, "wrong-kind", 1u, &inserted) ||
            inserted || bench_adapter.get_cstr(map, "wrong-kind", &value) ||
            bench_adapter.remove_cstr(map, "wrong-kind")) {
            return false;
        }
    } else {
        if (bench_adapter.put_u64(map, UINT64_C(1), UINT64_C(1), &inserted) ||
            inserted || bench_adapter.get_u64(map, UINT64_C(1), &value) ||
            bench_adapter.remove_u64(map, UINT64_C(1))) {
            return false;
        }
    }
    return bench_adapter.size(map) == 0u;
}

static uint64_t bench_values_checksum(const bench_dataset *data, bool updated)
{
    uint64_t checksum = 0;
    size_t index;

    for (index = 0; index < data->count; ++index) {
        checksum = bench_checksum_add(checksum,
                                      bench_value_at(data, index, updated));
    }
    return checksum;
}

static uint64_t bench_keys_checksum(const bench_dataset *data, bool miss)
{
    uint64_t checksum = 0;
    size_t index;

    for (index = 0; index < data->count; ++index) {
        checksum = bench_checksum_add(checksum,
                                      miss ? data->u64_misses[index]
                                           : data->u64_keys[index]);
    }
    return checksum;
}

static bool bench_populate(void *map, bench_key_kind kind,
                           const bench_dataset *data)
{
    size_t index;

    for (index = 0; index < data->count; ++index) {
        bool inserted = false;

        if (!bench_put_item(map, kind, data, index,
                            bench_value_at(data, index, false), &inserted) ||
            !inserted) {
            return false;
        }
    }
    return bench_adapter.size(map) == data->count;
}

static bool bench_validate_contents(void *map, bench_key_kind kind,
                                    const bench_dataset *data, bool empty,
                                    bool updated)
{
    uint64_t iteration_checksum = 0;
    size_t seen = 0;
    size_t index;

    if (bench_adapter.size(map) != (empty ? 0u : data->count)) return false;
    for (index = 0; index < data->count; ++index) {
        uint64_t value = 0;

        if (empty) {
            if (bench_get_item(map, kind, data, index, false, &value))
                return false;
        } else if (!bench_get_item(map, kind, data, index, false, &value) ||
                   value != bench_value_at(data, index, updated)) {
            return false;
        }
        if (bench_get_item(map, kind, data, index, true, &value)) return false;
    }
    if (!bench_adapter.iterate(map, &iteration_checksum, &seen)) return false;
    return seen == (empty ? 0u : data->count) &&
           iteration_checksum == (empty ? UINT64_C(0)
                                         : bench_values_checksum(data,
                                                                 updated));
}

static uint64_t bench_expected_checksum(const bench_dataset *data,
                                        bench_workload workload)
{
    switch (workload) {
        case BENCH_UPDATE:
            return bench_values_checksum(data, true);
        case BENCH_LOOKUP_MISS:
            return bench_keys_checksum(data, true);
        case BENCH_ERASE:
            return bench_keys_checksum(data, false);
        case BENCH_INSERT_RESERVED:
        case BENCH_INSERT_GROWING:
        case BENCH_LOOKUP_HIT:
        case BENCH_ITERATE:
            return bench_values_checksum(data, false);
        case BENCH_WORKLOAD_COUNT:
            break;
    }
    return 0;
}

static bool bench_execute_workload(void *map, bench_key_kind kind,
                                   bench_workload workload,
                                   const bench_dataset *data,
                                   uint64_t *checksum, double *elapsed)
{
    uint64_t result = 0;
    double start;
    double finish;
    bool ok = true;
    size_t index;

    start = bench_seconds_now();
    if (!(start > 0.0) || !isfinite(start)) return false;
    switch (workload) {
        case BENCH_INSERT_RESERVED:
        case BENCH_INSERT_GROWING:
            for (index = 0; index < data->count; ++index) {
                const uint64_t value = bench_value_at(data, index, false);
                bool inserted = false;

                if (!bench_put_item(map, kind, data, index, value, &inserted) ||
                    !inserted) {
                    ok = false;
                    break;
                }
                result = bench_checksum_add(result, value);
            }
            break;
        case BENCH_UPDATE:
            for (index = 0; index < data->count; ++index) {
                const uint64_t value = bench_value_at(data, index, true);
                bool inserted = true;

                if (!bench_put_item(map, kind, data, index, value, &inserted) ||
                    inserted) {
                    ok = false;
                    break;
                }
                result = bench_checksum_add(result, value);
            }
            break;
        case BENCH_LOOKUP_HIT:
            for (index = 0; index < data->count; ++index) {
                uint64_t value = 0;

                if (!bench_get_item(map, kind, data, index, false, &value) ||
                    value != bench_value_at(data, index, false)) {
                    ok = false;
                    break;
                }
                result = bench_checksum_add(result, value);
            }
            break;
        case BENCH_LOOKUP_MISS:
            for (index = 0; index < data->count; ++index) {
                uint64_t value = 0;

                if (bench_get_item(map, kind, data, index, true, &value)) {
                    ok = false;
                    break;
                }
                result = bench_checksum_add(result, data->u64_misses[index]);
            }
            break;
        case BENCH_ERASE:
            for (index = 0; index < data->count; ++index) {
                if (!bench_remove_item(map, kind, data, index)) {
                    ok = false;
                    break;
                }
                result = bench_checksum_add(result, data->u64_keys[index]);
            }
            break;
        case BENCH_ITERATE: {
            size_t seen = 0;

            if (!bench_adapter.iterate(map, &result, &seen) ||
                seen != data->count) {
                ok = false;
            }
            break;
        }
        case BENCH_WORKLOAD_COUNT:
            ok = false;
            break;
    }
    finish = bench_seconds_now();
    if (!(finish > 0.0) || !isfinite(finish) || finish < start) return false;
    *checksum = result;
    *elapsed = finish - start;
    return ok;
}

static bool bench_sample_matches(const bench_sample_result *left,
                                 const bench_sample_result *right)
{
    return left->checksum == right->checksum &&
           left->final_size == right->final_size &&
           left->memory.current_bytes == right->memory.current_bytes &&
           left->memory.peak_bytes == right->memory.peak_bytes &&
           left->memory.allocations == right->memory.allocations &&
           left->capacity == right->capacity;
}

static bool bench_run_sample(const bench_dataset *data, bench_key_kind kind,
                             bench_workload workload, double *nanoseconds,
                             bench_sample_result *sample)
{
    const bool insert_workload = workload == BENCH_INSERT_RESERVED ||
                                 workload == BENCH_INSERT_GROWING;
    const size_t expected = workload == BENCH_INSERT_GROWING ? 0u
                                                              : data->count;
    const bool final_empty = workload == BENCH_ERASE;
    const bool final_updated = workload == BENCH_UPDATE;
    const char *failure = NULL;
    bench_alloc_stats ended;
    void *map = NULL;
    double elapsed = 0.0;
    uint64_t checksum = 0;

    bench_alloc_reset();
    bench_alloc_begin();
    map = bench_adapter.create(kind, expected);
    if (map == NULL) {
        failure = "map creation failed";
        goto cleanup;
    }
    if (!bench_wrong_kind_is_safe(map, kind)) {
        failure = "wrong-key operation changed the map";
        goto cleanup;
    }
    if (workload == BENCH_INSERT_RESERVED &&
        !bench_adapter.reserve(map, data->count)) {
        failure = "reserve failed";
        goto cleanup;
    }
    if (!insert_workload && !bench_populate(map, kind, data)) {
        failure = "map setup failed";
        goto cleanup;
    }
    if (!bench_execute_workload(map, kind, workload, data, &checksum,
                                &elapsed)) {
        failure = "timed operation or clock failed";
        goto cleanup;
    }
    if (checksum != bench_expected_checksum(data, workload)) {
        failure = "workload checksum mismatch";
        goto cleanup;
    }
    if (!bench_validate_contents(map, kind, data, final_empty,
                                 final_updated)) {
        failure = "final map validation failed";
        goto cleanup;
    }
    sample->checksum = checksum;
    sample->final_size = bench_adapter.size(map);
    sample->memory = bench_alloc_snapshot();
    sample->capacity = bench_adapter.capacity(map);
    if (sample->memory.current_bytes > sample->memory.peak_bytes ||
        (sample->capacity != 0u &&
         sample->capacity < sample->final_size)) {
        failure = "invalid map accounting";
        goto cleanup;
    }
    *nanoseconds = elapsed * 1e9 / (double)data->count;
    if (!isfinite(*nanoseconds) || *nanoseconds < 0.0) {
        failure = "invalid elapsed time";
        goto cleanup;
    }

cleanup:
    if (map != NULL) bench_adapter.destroy(map);
    ended = bench_alloc_end();
    if (ended.current_bytes != 0u && failure == NULL) {
        failure = "adapter leaked scoped memory";
    }
    if (failure != NULL) {
        fprintf(stderr, "%s: %s/%s: %s\n", bench_adapter.name,
                bench_key_kind_name(kind), bench_workload_names[workload],
                failure);
        return false;
    }
    return true;
}

static bool bench_run_scenario(const bench_options *options,
                               const bench_dataset *data,
                               bench_key_kind kind,
                               bench_workload workload)
{
    bench_sample_result reference = {0, 0, {0, 0, 0, 0}, 0};
    double *samples;
    double median;
    double mad;
    size_t bytes;
    size_t sample_index;

    if (options->samples > SIZE_MAX / sizeof(*samples)) {
        fputs("sample array is too large\n", stderr);
        return false;
    }
    bytes = options->samples * sizeof(*samples);
    samples = (double *)malloc(bytes);
    if (samples == NULL) {
        fputs("could not allocate sample array\n", stderr);
        return false;
    }
    for (sample_index = 0; sample_index < options->samples; ++sample_index) {
        bench_sample_result current;

        if (!bench_run_sample(data, kind, workload, &samples[sample_index],
                              &current)) {
            free(samples);
            return false;
        }
        if (sample_index == 0u) {
            reference = current;
        } else if (!bench_sample_matches(&reference, &current)) {
            fprintf(stderr, "%s: %s/%s: sample metadata changed\n",
                    bench_adapter.name, bench_key_kind_name(kind),
                    bench_workload_names[workload]);
            free(samples);
            return false;
        }
    }

    median = bench_median(samples, options->samples);
    mad = bench_mad(samples, options->samples, median);
    free(samples);
    if (!isfinite(median) || !isfinite(mad) || median < 0.0 || mad < 0.0) {
        fputs("invalid timing statistics\n", stderr);
        return false;
    }
    if (printf("result\t%s\t%s\t%s\t%zu\t%zu\t%.3f\t%.3f\tok\t%" PRIu64
               "\t%zu\t%zu\t%zu\t%zu\t%zu\n",
               bench_adapter.name, bench_key_kind_name(kind),
               bench_workload_names[workload], options->count,
               options->samples, median, mad, reference.checksum,
               reference.final_size, reference.memory.current_bytes,
               reference.memory.peak_bytes, reference.memory.allocations,
               reference.capacity) < 0) {
        fputs("could not write benchmark output\n", stderr);
        return false;
    }
    return true;
}

int main(int argc, char **argv)
{
    bench_options options;
    bench_dataset data = BENCH_DATASET_ZERO;
    unsigned kind_index;
    unsigned workload_index;

    if (!bench_parse_options(argc, argv, &options)) return 2;
    if (!bench_adapter_is_complete()) {
        fputs("adapter contract is incomplete\n", stderr);
        return 1;
    }
    if (!bench_dataset_init(&data, options.count, options.seed)) {
        fputs("could not allocate benchmark dataset\n", stderr);
        return 1;
    }
    for (kind_index = 0; kind_index < 2u; ++kind_index) {
        const bench_key_kind kind = kind_index == 0u ? BENCH_KEYS_U64
                                                     : BENCH_KEYS_CSTR;

        for (workload_index = 0;
             workload_index < (unsigned)BENCH_WORKLOAD_COUNT;
             ++workload_index) {
            if (!bench_run_scenario(&options, &data, kind,
                                    (bench_workload)workload_index)) {
                bench_dataset_destroy(&data);
                return 1;
            }
        }
    }
    bench_dataset_destroy(&data);
    return 0;
}
