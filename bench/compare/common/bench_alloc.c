#include "bench_alloc.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef union bench_alloc_header {
    struct {
        size_t requested_size;
        uint64_t generation;
        bool scoped;
    } fields;
    max_align_t alignment;
} bench_alloc_header;

static bench_alloc_stats bench_stats;
static uint64_t bench_generation = UINT64_C(1);
static bool bench_scope_active;

static bool bench_size_add(size_t left, size_t right, size_t *result)
{
    if (right > SIZE_MAX - left) return false;
    *result = left + right;
    return true;
}

static bool bench_size_multiply(size_t left, size_t right, size_t *result)
{
    if (left != 0 && right > SIZE_MAX / left) return false;
    *result = left * right;
    return true;
}

static bool bench_raw_size(size_t requested_size, size_t *raw_size)
{
    /*
     * Zero-byte requests use one raw payload byte. This keeps the returned
     * pointer aligned and freeable without relying on malloc(0), while the
     * counters still record zero requested bytes.
     */
    const size_t payload_size = requested_size == 0 ? 1u : requested_size;

    return bench_size_add(sizeof(bench_alloc_header), payload_size, raw_size);
}

static bool bench_header_is_current(const bench_alloc_header *header)
{
    return header->fields.scoped &&
           header->fields.generation == bench_generation;
}

static bool bench_can_record_allocation(size_t requested_size,
                                        size_t replaced_size,
                                        bool replacement,
                                        size_t *new_current)
{
    size_t retained_bytes;

    if (bench_stats.allocations == SIZE_MAX) return false;
    if (replacement && bench_stats.frees == SIZE_MAX) return false;
    if (replaced_size > bench_stats.current_bytes) return false;
    retained_bytes = bench_stats.current_bytes - replaced_size;
    return bench_size_add(retained_bytes, requested_size, new_current);
}

static void *bench_allocate(size_t requested_size, bool clear)
{
    bench_alloc_header *header;
    size_t raw_size;
    size_t new_current = 0;

    if (!bench_raw_size(requested_size, &raw_size)) return NULL;
    if (bench_scope_active &&
        !bench_can_record_allocation(requested_size, 0, false,
                                     &new_current)) {
        return NULL;
    }

    header = clear ? (bench_alloc_header *)calloc(1, raw_size)
                   : (bench_alloc_header *)malloc(raw_size);
    if (header == NULL) return NULL;
    header->fields.requested_size = requested_size;
    header->fields.generation = bench_generation;
    header->fields.scoped = bench_scope_active;

    if (bench_scope_active) {
        ++bench_stats.allocations;
        bench_stats.current_bytes = new_current;
        if (new_current > bench_stats.peak_bytes) {
            bench_stats.peak_bytes = new_current;
        }
    }
    return (void *)(header + 1);
}

void bench_alloc_reset(void)
{
    bench_stats = (bench_alloc_stats){0, 0, 0, 0};
    bench_scope_active = false;
    ++bench_generation;
    if (bench_generation == 0) bench_generation = UINT64_C(1);
}

void bench_alloc_begin(void)
{
    bench_scope_active = true;
}

bench_alloc_stats bench_alloc_end(void)
{
    bench_scope_active = false;
    return bench_stats;
}

bench_alloc_stats bench_alloc_snapshot(void)
{
    return bench_stats;
}

void *bench_malloc(size_t size)
{
    return bench_allocate(size, false);
}

void *bench_calloc(size_t count, size_t size)
{
    size_t requested_size;

    if (!bench_size_multiply(count, size, &requested_size)) return NULL;
    return bench_allocate(requested_size, true);
}

void *bench_realloc(void *pointer, size_t size)
{
    bench_alloc_header *header;
    bench_alloc_header *resized;
    const bool scope_was_active = bench_scope_active;
    bool tracked;
    size_t old_size;
    size_t raw_size;
    size_t new_current = 0;

    if (pointer == NULL) return bench_malloc(size);
    header = (bench_alloc_header *)pointer - 1;
    old_size = header->fields.requested_size;
    tracked = bench_header_is_current(header);
    if (!bench_raw_size(size, &raw_size)) return NULL;

    if (tracked) {
        if (scope_was_active) {
            if (!bench_can_record_allocation(size, old_size, true,
                                             &new_current)) {
                return NULL;
            }
        } else {
            size_t retained_bytes;

            if (old_size > bench_stats.current_bytes) return NULL;
            retained_bytes = bench_stats.current_bytes - old_size;
            if (!bench_size_add(retained_bytes, size, &new_current)) {
                return NULL;
            }
        }
    }

    resized = (bench_alloc_header *)realloc(header, raw_size);
    if (resized == NULL) return NULL;
    resized->fields.requested_size = size;

    if (tracked) {
        bench_stats.current_bytes = new_current;
        if (scope_was_active) {
            ++bench_stats.allocations;
            ++bench_stats.frees;
            if (new_current > bench_stats.peak_bytes) {
                bench_stats.peak_bytes = new_current;
            }
        }
    }
    return (void *)(resized + 1);
}

void bench_free(void *pointer)
{
    bench_alloc_header *header;

    if (pointer == NULL) return;
    header = (bench_alloc_header *)pointer - 1;
    if (bench_header_is_current(header)) {
        if (header->fields.requested_size <= bench_stats.current_bytes) {
            bench_stats.current_bytes -= header->fields.requested_size;
        } else {
            bench_stats.current_bytes = 0;
        }
        if (bench_scope_active && bench_stats.frees != SIZE_MAX) {
            ++bench_stats.frees;
        }
    }
    free(header);
}
