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
    BENCH_CHURN,
    BENCH_WORKLOAD_COUNT
} bench_workload;

typedef enum bench_churn_operation {
    BENCH_CHURN_HIT,
    BENCH_CHURN_MISS,
    BENCH_CHURN_ERASE,
    BENCH_CHURN_INSERT
} bench_churn_operation;

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

typedef struct bench_prepared_values {
    uint64_t *original;
    uint64_t *updated;
    uint64_t original_value_sum;
    uint64_t original_checksum;
    uint64_t updated_checksum;
    uint64_t key_checksum;
    uint64_t miss_checksum;
    uint64_t churn_checksum;
} bench_prepared_values;

typedef struct bench_operation_result {
    size_t successful;
    size_t inserted;
    size_t seen;
    uint64_t value_sum;
    uint64_t checksum;
} bench_operation_result;

static const char *const bench_workload_names[BENCH_WORKLOAD_COUNT] = {
    "insert_reserved",
    "insert_growing",
    "update",
    "lookup_hit",
    "lookup_miss",
    "erase",
    "iterate",
    "churn"
};

static const double bench_min_sample_seconds = 0.010;
enum {
    BENCH_MAX_SAMPLE_REPETITIONS = 1000000,
    BENCH_MIN_TIMED_INTERVAL_OPERATIONS = 256
};

static size_t bench_churn_live_count(size_t count)
{
    size_t live = (count / 10u) * 7u + ((count % 10u) * 7u) / 10u;

    return live == 0u ? 1u : live;
}

static bool bench_workload_operation_count(size_t count,
                                           bench_workload workload,
                                           size_t *result)
{
    size_t blocks;

    if (result == NULL || count == 0u) return false;
    if (workload != BENCH_CHURN) {
        *result = count;
        return true;
    }
    blocks = count / 20u + (size_t)(count % 20u != 0u);
    if (blocks > SIZE_MAX / 20u) return false;
    *result = blocks * 20u;
    return true;
}

static const char *bench_key_kind_name(bench_key_kind kind)
{
    return kind == BENCH_KEYS_U64 ? "u64" : "cstr";
}

static bool bench_parse_u64(const char *text, uint64_t limit, bool allow_zero,
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
    if (!allow_zero && value == 0) return false;
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
                !bench_parse_u64(argv[++index], (uint64_t)SIZE_MAX, false,
                                 &value)) {
                goto invalid;
            }
            count_seen = true;
            parsed.count = (size_t)value;
            continue;
        }
        if (strcmp(option, "--samples") == 0) {
            if (samples_seen || index + 1 >= argc ||
                !bench_parse_u64(argv[++index], (uint64_t)SIZE_MAX, false,
                                 &value)) {
                goto invalid;
            }
            samples_seen = true;
            parsed.samples = (size_t)value;
            continue;
        }
        if (strcmp(option, "--seed") == 0) {
            if (seed_seen || index + 1 >= argc ||
                !bench_parse_u64(argv[++index], UINT64_MAX, true, &value)) {
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

static void bench_prepared_values_destroy(bench_prepared_values *values)
{
    if (values == NULL) return;
    free(values->updated);
    free(values->original);
    *values = (bench_prepared_values){0};
}

static bool bench_prepared_values_init(bench_prepared_values *values,
                                       const bench_dataset *data)
{
    const uint64_t original_version = UINT64_C(0x13198a2e03707344);
    const uint64_t updated_version = UINT64_C(0xa4093822299f31d0);
    bench_prepared_values prepared = {0};
    size_t bytes;
    size_t index;

    if (values == NULL || data == NULL || data->count == 0u ||
        data->count > SIZE_MAX / sizeof(*prepared.original)) {
        return false;
    }
    bytes = data->count * sizeof(*prepared.original);
    prepared.original = (uint64_t *)malloc(bytes);
    prepared.updated = (uint64_t *)malloc(bytes);
    if (prepared.original == NULL || prepared.updated == NULL) {
        bench_prepared_values_destroy(&prepared);
        return false;
    }
    for (index = 0; index < data->count; ++index) {
        prepared.original[index] =
            bench_mix64(data->u64_keys[index] ^ data->seed ^ original_version);
        prepared.updated[index] =
            bench_mix64(data->u64_keys[index] ^ data->seed ^ updated_version);
        prepared.original_checksum = bench_checksum_add(
            prepared.original_checksum, prepared.original[index]);
        prepared.original_value_sum += prepared.original[index];
        prepared.updated_checksum = bench_checksum_add(
            prepared.updated_checksum, prepared.updated[index]);
        prepared.key_checksum = bench_checksum_add(prepared.key_checksum,
                                                   data->u64_keys[index]);
        prepared.miss_checksum = bench_checksum_add(prepared.miss_checksum,
                                                    data->u64_misses[index]);
        if (index < bench_churn_live_count(data->count)) {
            prepared.churn_checksum = bench_checksum_add(
                prepared.churn_checksum, prepared.original[index]);
        }
    }
    *values = prepared;
    return true;
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

static bool bench_populate_count(void *map, bench_key_kind kind,
                                 const bench_dataset *data,
                                 const bench_prepared_values *values,
                                 size_t count)
{
    size_t index;

    for (index = 0; index < count; ++index) {
        bool inserted = false;

        if (!bench_put_item(map, kind, data, index, values->original[index],
                            &inserted) ||
            !inserted) {
            return false;
        }
    }
    return bench_adapter.size(map) == count;
}

static bool bench_populate(void *map, bench_key_kind kind,
                           const bench_dataset *data,
                           const bench_prepared_values *values)
{
    return bench_populate_count(map, kind, data, values, data->count);
}

static bool bench_validate_contents(void *map, bench_key_kind kind,
                                    const bench_dataset *data,
                                    const bench_prepared_values *values,
                                    bool empty, bool updated)
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
                   value != (updated ? values->updated[index]
                                     : values->original[index])) {
            return false;
        }
        if (bench_get_item(map, kind, data, index, true, &value)) return false;
    }
    if (!bench_adapter.iterate(map, &iteration_checksum, &seen)) return false;
    return seen == (empty ? 0u : data->count) &&
           iteration_checksum ==
               (empty ? UINT64_C(0)
                      : (updated ? values->updated_checksum
                                 : values->original_checksum));
}

static bool bench_validate_churn_contents(
    void *map, bench_key_kind kind, const bench_dataset *data,
    const bench_prepared_values *values)
{
    const size_t live_count = bench_churn_live_count(data->count);
    uint64_t iteration_checksum = 0;
    size_t seen = 0;
    size_t index;

    if (bench_adapter.size(map) != live_count) return false;
    for (index = 0; index < data->count; ++index) {
        uint64_t value = 0;
        const bool found = bench_get_item(map, kind, data, index, false,
                                          &value);

        if (index < live_count) {
            if (!found || value != values->original[index]) return false;
        } else if (found) {
            return false;
        }
        if (bench_get_item(map, kind, data, index, true, &value)) return false;
    }
    if (!bench_adapter.iterate(map, &iteration_checksum, &seen)) return false;
    return seen == live_count &&
           iteration_checksum == values->churn_checksum;
}

static uint64_t bench_expected_checksum(const bench_prepared_values *values,
                                        bench_workload workload)
{
    switch (workload) {
        case BENCH_UPDATE:
            return values->updated_checksum;
        case BENCH_LOOKUP_MISS:
            return values->miss_checksum;
        case BENCH_ERASE:
            return values->key_checksum;
        case BENCH_INSERT_RESERVED:
        case BENCH_INSERT_GROWING:
        case BENCH_LOOKUP_HIT:
        case BENCH_ITERATE:
            return values->original_checksum;
        case BENCH_CHURN:
            return values->churn_checksum;
        case BENCH_WORKLOAD_COUNT:
            break;
    }
    return 0;
}

static void bench_apply_churn(void *map, bench_key_kind kind,
                              const bench_dataset *data,
                              const bench_prepared_values *values,
                              bench_operation_result *result)
{
    static const bench_churn_operation operations[20] = {
        BENCH_CHURN_HIT, BENCH_CHURN_HIT, BENCH_CHURN_MISS,
        BENCH_CHURN_HIT, BENCH_CHURN_ERASE, BENCH_CHURN_INSERT,
        BENCH_CHURN_HIT, BENCH_CHURN_MISS, BENCH_CHURN_HIT,
        BENCH_CHURN_HIT, BENCH_CHURN_HIT, BENCH_CHURN_MISS,
        BENCH_CHURN_HIT, BENCH_CHURN_ERASE, BENCH_CHURN_INSERT,
        BENCH_CHURN_HIT, BENCH_CHURN_MISS, BENCH_CHURN_HIT,
        BENCH_CHURN_ERASE, BENCH_CHURN_INSERT
    };
    const size_t live_count = bench_churn_live_count(data->count);
    size_t operation_count;
    size_t hit_index = 0;
    size_t miss_index = 0;
    size_t erase_index = 0;
    size_t insert_index = 0;
    size_t operation_index;

    result->checksum = values->churn_checksum;
    if (!bench_workload_operation_count(data->count, BENCH_CHURN,
                                        &operation_count)) {
        return;
    }
    for (operation_index = 0; operation_index < operation_count;
         ++operation_index) {
        const bench_churn_operation operation =
            operations[operation_index % 20u];
        size_t index;

        if (operation == BENCH_CHURN_HIT) {
            uint64_t value = 0;

            index = hit_index++ % live_count;
            if (bench_get_item(map, kind, data, index, false, &value) &&
                value == values->original[index]) {
                ++result->successful;
                result->value_sum += value;
            }
        } else if (operation == BENCH_CHURN_MISS) {
            uint64_t value = 0;

            index = miss_index++ % data->count;
            if (!bench_get_item(map, kind, data, index, true, &value))
                ++result->successful;
        } else if (operation == BENCH_CHURN_ERASE) {
            index = erase_index++ % live_count;
            if (bench_remove_item(map, kind, data, index)) {
                ++result->successful;
                result->checksum -=
                    bench_checksum_add(0u, values->original[index]);
            }
        } else {
            bool inserted = false;

            index = insert_index++ % live_count;
            if (bench_put_item(map, kind, data, index,
                               values->original[index], &inserted) &&
                inserted) {
                ++result->successful;
                ++result->inserted;
                result->checksum = bench_checksum_add(
                    result->checksum, values->original[index]);
            }
        }
        ++result->seen;
    }
}

static void bench_apply_workload(void *map, bench_key_kind kind,
                                 bench_workload workload,
                                 const bench_dataset *data,
                                 const bench_prepared_values *values,
                                 bench_operation_result *result)
{
    size_t index;

    switch (workload) {
        case BENCH_INSERT_RESERVED:
        case BENCH_INSERT_GROWING:
            for (index = 0; index < data->count; ++index) {
                bool inserted = false;
                const bool successful = bench_put_item(
                    map, kind, data, index, values->original[index], &inserted);

                result->successful += (size_t)successful;
                result->inserted += (size_t)inserted;
            }
            break;
        case BENCH_UPDATE:
            for (index = 0; index < data->count; ++index) {
                bool inserted = false;
                const bool successful = bench_put_item(
                    map, kind, data, index, values->updated[index], &inserted);

                result->successful += (size_t)successful;
                result->inserted += (size_t)inserted;
            }
            break;
        case BENCH_LOOKUP_HIT:
            for (index = 0; index < data->count; ++index) {
                uint64_t value = 0;
                const bool found = bench_get_item(map, kind, data, index,
                                                  false, &value);

                result->successful += (size_t)found;
                result->value_sum += value;
            }
            break;
        case BENCH_LOOKUP_MISS:
            for (index = 0; index < data->count; ++index) {
                uint64_t value = 0;
                const bool found = bench_get_item(map, kind, data, index,
                                                  true, &value);

                result->successful += (size_t)found;
            }
            break;
        case BENCH_ERASE:
            for (index = 0; index < data->count; ++index) {
                result->successful +=
                    (size_t)bench_remove_item(map, kind, data, index);
            }
            break;
        case BENCH_ITERATE:
            result->successful += (size_t)bench_adapter.iterate(
                map, &result->checksum, &result->seen);
            break;
        case BENCH_CHURN:
            bench_apply_churn(map, kind, data, values, result);
            break;
        case BENCH_WORKLOAD_COUNT:
            break;
    }
}

static void bench_prefault_lookup_values(uint64_t *values, size_t count)
{
    volatile uint64_t *volatile_values = values;
    size_t index;

    for (index = 0; index < count; ++index) volatile_values[index] = 0u;
}

static void bench_capture_lookup_hits(void *map, bench_key_kind kind,
                                      const bench_dataset *data,
                                      bench_operation_result *result,
                                      uint64_t *values)
{
    size_t index;

    for (index = 0; index < data->count; ++index) {
        result->successful += (size_t)bench_get_item(
            map, kind, data, index, false, &values[index]);
    }
}

static bool bench_lookup_values_are_valid(
    const uint64_t *lookup_values, const bench_dataset *data,
    const bench_prepared_values *values)
{
    bool valid = true;
    size_t index;

    if (lookup_values == NULL) return false;
    for (index = 0; index < data->count; ++index) {
        valid &= lookup_values[index] == values->original[index];
    }
    return valid;
}

static bool bench_operation_result_is_valid(
    const bench_operation_result *result, const bench_dataset *data,
    const bench_prepared_values *values, bench_workload workload,
    const uint64_t *lookup_values)
{
    switch (workload) {
        case BENCH_INSERT_RESERVED:
        case BENCH_INSERT_GROWING:
            return result->successful == data->count &&
                   result->inserted == data->count;
        case BENCH_UPDATE:
            return result->successful == data->count &&
                   result->inserted == 0u;
        case BENCH_LOOKUP_HIT: {
            bool valid;

            if (lookup_values == NULL) {
                return result->successful == data->count &&
                       result->value_sum == values->original_value_sum;
            }
            valid = result->successful == data->count;
            valid &= bench_lookup_values_are_valid(lookup_values, data,
                                                   values);
            return valid;
        }
        case BENCH_LOOKUP_MISS:
            return result->successful == 0u;
        case BENCH_ERASE:
            return result->successful == data->count;
        case BENCH_ITERATE:
            return result->successful == 1u && result->seen == data->count &&
                   result->checksum == values->original_checksum;
        case BENCH_CHURN: {
            size_t operation_count;

            return bench_workload_operation_count(data->count, workload,
                                                  &operation_count) &&
                   result->successful == operation_count &&
                   result->inserted == (operation_count / 20u) * 3u &&
                   result->seen == operation_count &&
                   result->checksum == values->churn_checksum;
        }
        case BENCH_WORKLOAD_COUNT:
            return false;
    }
    return false;
}

static bool bench_prepare_map(void **map_out, bench_key_kind kind,
                              bench_workload workload,
                              const bench_dataset *data,
                              const bench_prepared_values *values,
                              bool check_wrong_kind)
{
    const bool insert_workload = workload == BENCH_INSERT_RESERVED ||
                                 workload == BENCH_INSERT_GROWING;
    const size_t expected = workload == BENCH_INSERT_GROWING ? 0u
                                                              : data->count;
    void *map;

    *map_out = NULL;
    map = bench_adapter.create(kind, expected);
    if (map == NULL) return false;
    *map_out = map;
    if (check_wrong_kind && !bench_wrong_kind_is_safe(map, kind)) return false;
    if ((workload == BENCH_INSERT_RESERVED || workload == BENCH_CHURN) &&
        !bench_adapter.reserve(map, data->count)) {
        return false;
    }
    if (insert_workload) return true;
    if (workload == BENCH_CHURN) {
        return bench_populate_count(map, kind, data, values,
                                    bench_churn_live_count(data->count));
    }
    return bench_populate(map, kind, data, values);
}

static bool bench_run_untimed_map(const bench_dataset *data,
                                  bench_key_kind kind,
                                  const bench_prepared_values *values,
                                  bench_workload workload,
                                  bench_sample_result *sample)
{
    const bool final_empty = workload == BENCH_ERASE;
    const bool final_updated = workload == BENCH_UPDATE;
    const char *failure = NULL;
    bench_operation_result operation = {0, 0, 0, 0, 0};
    bench_sample_result observed;
    bench_alloc_stats ended;
    void *map = NULL;

    bench_alloc_reset();
    bench_alloc_begin();
    if (!bench_prepare_map(&map, kind, workload, data, values, true)) {
        failure = "map setup failed";
        goto cleanup;
    }
    bench_apply_workload(map, kind, workload, data, values, &operation);
    if (!bench_operation_result_is_valid(&operation, data, values, workload,
                                         NULL)) {
        failure = "operation result mismatch";
        goto cleanup;
    }
    if (workload == BENCH_CHURN
            ? !bench_validate_churn_contents(map, kind, data, values)
            : !bench_validate_contents(map, kind, data, values, final_empty,
                                       final_updated)) {
        failure = "final map validation failed";
        goto cleanup;
    }
    observed.checksum = bench_expected_checksum(values, workload);
    observed.final_size = bench_adapter.size(map);
    observed.memory = bench_alloc_snapshot();
    observed.capacity = bench_adapter.capacity(map);
    if (observed.memory.current_bytes > observed.memory.peak_bytes ||
        (observed.capacity != 0u &&
         observed.capacity < observed.final_size)) {
        failure = "invalid map accounting";
        goto cleanup;
    }
    if (sample != NULL) *sample = observed;

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

static bool bench_run_timed_batch(const bench_dataset *data,
                                  bench_key_kind kind,
                                  const bench_prepared_values *values,
                                  bench_workload workload,
                                  double *elapsed_seconds,
                                  size_t *operation_count)
{
    const bool final_empty = workload == BENCH_ERASE;
    const bool final_updated = workload == BENCH_UPDATE;
    const char *failure = NULL;
    bench_operation_result *results = NULL;
    uint64_t *lookup_values = NULL;
    void **maps = NULL;
    double start = 0.0;
    double finish = 0.0;
    size_t map_count;
    size_t map_index;
    size_t operations;
    size_t lookup_value_bytes = 0u;
    size_t pointer_bytes;
    size_t result_bytes;

    map_count = data->count < BENCH_MIN_TIMED_INTERVAL_OPERATIONS
                    ? 1u + ((size_t)BENCH_MIN_TIMED_INTERVAL_OPERATIONS - 1u) /
                                 data->count
                    : 1u;
    if (data->count > SIZE_MAX / map_count ||
        map_count > SIZE_MAX / sizeof(*maps) ||
        map_count > SIZE_MAX / sizeof(*results)) {
        failure = "timed batch size overflow";
        goto cleanup;
    }
    if (!bench_workload_operation_count(data->count, workload, &operations) ||
        operations > SIZE_MAX / map_count) {
        failure = "timed operation count overflow";
        goto cleanup;
    }
    operations *= map_count;
    if (workload == BENCH_LOOKUP_HIT) {
        if (operations > SIZE_MAX / sizeof(*lookup_values)) {
            failure = "lookup value array size overflow";
            goto cleanup;
        }
        lookup_value_bytes = operations * sizeof(*lookup_values);
    }
    pointer_bytes = map_count * sizeof(*maps);
    result_bytes = map_count * sizeof(*results);
    maps = (void **)calloc(1u, pointer_bytes);
    results = (bench_operation_result *)calloc(1u, result_bytes);
    if (workload == BENCH_LOOKUP_HIT) {
        lookup_values = (uint64_t *)malloc(lookup_value_bytes);
    }
    if (maps == NULL || results == NULL ||
        (workload == BENCH_LOOKUP_HIT && lookup_values == NULL)) {
        failure = "could not allocate timed batch";
        goto cleanup;
    }
    if (workload == BENCH_LOOKUP_HIT) {
        bench_prefault_lookup_values(lookup_values, operations);
    }

    bench_alloc_reset();
    for (map_index = 0; map_index < map_count; ++map_index) {
        if (!bench_prepare_map(&maps[map_index], kind, workload, data,
                               values, false)) {
            failure = "timed map setup failed";
            goto cleanup;
        }
    }

    start = bench_seconds_now();
    if (!(start > 0.0) || !isfinite(start)) {
        failure = "clock start failed";
        goto cleanup;
    }
    for (map_index = 0; map_index < map_count; ++map_index) {
        if (workload == BENCH_LOOKUP_HIT) {
            bench_capture_lookup_hits(
                maps[map_index], kind, data, &results[map_index],
                lookup_values + map_index * data->count);
        } else {
            bench_apply_workload(maps[map_index], kind, workload, data,
                                 values, &results[map_index]);
        }
    }
    finish = bench_seconds_now();

    for (map_index = 0; map_index < map_count; ++map_index) {
        if (!bench_operation_result_is_valid(&results[map_index], data,
                                             values, workload,
                                             workload == BENCH_LOOKUP_HIT
                                                 ? lookup_values +
                                                       map_index * data->count
                                                 : NULL) &&
            failure == NULL) {
            failure = "timed operation result mismatch";
        }
        if ((workload == BENCH_CHURN
                 ? !bench_validate_churn_contents(maps[map_index], kind, data,
                                                  values)
                 : !bench_validate_contents(maps[map_index], kind, data,
                                            values, final_empty,
                                            final_updated)) &&
            failure == NULL) {
            failure = "timed map validation failed";
        }
    }
    if (!(finish > 0.0) || !isfinite(finish) || finish <= start) {
        if (failure == NULL) failure = "timer made no progress";
    } else {
        *elapsed_seconds = finish - start;
        *operation_count = operations;
    }

cleanup:
    if (maps != NULL) {
        for (map_index = 0; map_index < map_count; ++map_index) {
            if (maps[map_index] != NULL) bench_adapter.destroy(maps[map_index]);
        }
    }
    free(lookup_values);
    free(results);
    free(maps);
    if (failure != NULL) {
        fprintf(stderr, "%s: %s/%s: %s\n", bench_adapter.name,
                bench_key_kind_name(kind), bench_workload_names[workload],
                failure);
        return false;
    }
    return true;
}

static bool bench_run_batched_sample(
    const bench_dataset *data, bench_key_kind kind,
    const bench_prepared_values *values, bench_workload workload,
    double *nanoseconds)
{
    double total_seconds = 0.0;
    size_t total_operations = 0;
    size_t repetitions = 0;

    while (total_seconds < bench_min_sample_seconds) {
        double elapsed_seconds = 0.0;
        double next_total;
        size_t batch_operations = 0;

        if (repetitions == (size_t)BENCH_MAX_SAMPLE_REPETITIONS) {
            fprintf(stderr, "%s: %s/%s: sample repetition limit reached\n",
                    bench_adapter.name, bench_key_kind_name(kind),
                    bench_workload_names[workload]);
            return false;
        }
        if (!bench_run_timed_batch(data, kind, values, workload,
                                   &elapsed_seconds, &batch_operations)) {
            return false;
        }
        if (batch_operations == 0u ||
            batch_operations > SIZE_MAX - total_operations) {
            fprintf(stderr, "%s: %s/%s: operation count overflow\n",
                    bench_adapter.name, bench_key_kind_name(kind),
                    bench_workload_names[workload]);
            return false;
        }
        next_total = total_seconds + elapsed_seconds;
        if (!isfinite(next_total) || !(next_total > total_seconds)) {
            fprintf(stderr, "%s: %s/%s: timer made no progress\n",
                    bench_adapter.name, bench_key_kind_name(kind),
                    bench_workload_names[workload]);
            return false;
        }
        total_seconds = next_total;
        total_operations += batch_operations;
        ++repetitions;
    }

    *nanoseconds = (double)((long double)total_seconds * 1.0e9L /
                            (long double)total_operations);
    if (!isfinite(*nanoseconds) || !(*nanoseconds > 0.0)) {
        fprintf(stderr, "%s: %s/%s: invalid sample result\n",
                bench_adapter.name, bench_key_kind_name(kind),
                bench_workload_names[workload]);
        return false;
    }
    return true;
}

static bool bench_run_scenario(const bench_options *options,
                               const bench_dataset *data,
                               const bench_prepared_values *values,
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
    if (!bench_run_untimed_map(data, kind, values, workload, NULL)) {
        free(samples);
        return false;
    }
    if (!bench_run_untimed_map(data, kind, values, workload, &reference)) {
        free(samples);
        return false;
    }
    for (sample_index = 0; sample_index < options->samples; ++sample_index) {
        if (!bench_run_batched_sample(data, kind, values, workload,
                                      &samples[sample_index])) {
            free(samples);
            return false;
        }
    }

    if (!bench_statistics(samples, options->samples, &median, &mad)) {
        fputs("invalid timing statistics\n", stderr);
        free(samples);
        return false;
    }
    free(samples);
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
    bench_prepared_values values = {0};
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
    if (!bench_prepared_values_init(&values, &data)) {
        fputs("could not allocate prepared benchmark values\n", stderr);
        bench_dataset_destroy(&data);
        return 1;
    }
    for (kind_index = 0; kind_index < 2u; ++kind_index) {
        const bench_key_kind kind = kind_index == 0u ? BENCH_KEYS_U64
                                                     : BENCH_KEYS_CSTR;

        for (workload_index = 0;
             workload_index < (unsigned)BENCH_WORKLOAD_COUNT;
             ++workload_index) {
            if (!bench_run_scenario(&options, &data, &values, kind,
                                    (bench_workload)workload_index)) {
                bench_prepared_values_destroy(&values);
                bench_dataset_destroy(&data);
                return 1;
            }
        }
    }
    bench_prepared_values_destroy(&values);
    bench_dataset_destroy(&data);
    return 0;
}
