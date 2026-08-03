#include "../bench/compare/common/bench_alloc.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void assert_maximally_aligned(const void *pointer)
{
    assert((uintptr_t)pointer % (uintptr_t)_Alignof(max_align_t) == 0u);
}

static void assert_stats_equal(bench_alloc_stats left,
                               bench_alloc_stats right)
{
    assert(left.current_bytes == right.current_bytes);
    assert(left.peak_bytes == right.peak_bytes);
    assert(left.allocations == right.allocations);
    assert(left.frees == right.frees);
}

static void test_scope(void)
{
    bench_alloc_stats stats;
    void *a;
    void *b;
    size_t i;

    bench_alloc_reset();
    bench_alloc_begin();
    a = bench_malloc(24);
    b = bench_calloc(4, 16);
    assert(a != NULL && b != NULL);
    assert_maximally_aligned(a);
    assert_maximally_aligned(b);
    for (i = 0; i < 64; ++i) {
        assert(((const unsigned char *)b)[i] == 0);
    }
    a = bench_realloc(a, 48);
    assert(a != NULL);
    assert_maximally_aligned(a);
    bench_free(b);
    stats = bench_alloc_end();
    assert(stats.allocations == 3);
    assert(stats.frees == 2);
    assert(stats.current_bytes == 48);
    assert(stats.peak_bytes >= 88);
    bench_free(a);
    stats = bench_alloc_snapshot();
    assert(stats.current_bytes == 0);
    assert(stats.frees == 2);
}

static void test_zero_size_requests(void)
{
    bench_alloc_stats stats;
    void *a;
    void *b;
    void *c;
    void *d;

    bench_alloc_reset();
    bench_alloc_begin();
    a = bench_malloc(0);
    b = bench_calloc(0, 17);
    c = bench_calloc(17, 0);
    d = bench_realloc(NULL, 0);
    assert(a != NULL && b != NULL && c != NULL && d != NULL);
    assert_maximally_aligned(a);
    assert_maximally_aligned(b);
    assert_maximally_aligned(c);
    assert_maximally_aligned(d);

    a = bench_realloc(a, 0);
    assert(a != NULL);
    assert_maximally_aligned(a);
    stats = bench_alloc_snapshot();
    assert(stats.allocations == 5);
    assert(stats.frees == 1);
    assert(stats.current_bytes == 0);
    assert(stats.peak_bytes == 0);

    bench_free(a);
    bench_free(b);
    bench_free(c);
    bench_free(d);
    stats = bench_alloc_end();
    assert(stats.allocations == 5);
    assert(stats.frees == 5);
    assert(stats.current_bytes == 0);
    assert(stats.peak_bytes == 0);
}

static void test_overflow_and_failed_realloc(void)
{
    static const unsigned char payload[] = {
        0x03, 0x14, 0x15, 0x92, 0x65, 0x35, 0x89, 0x79
    };
    bench_alloc_stats before;
    bench_alloc_stats after;
    unsigned char *allocation;

    bench_alloc_reset();
    bench_alloc_begin();
    assert(bench_calloc(SIZE_MAX, 2) == NULL);
    assert(bench_malloc(SIZE_MAX) == NULL);
    assert_stats_equal(bench_alloc_snapshot(),
                       (bench_alloc_stats){0, 0, 0, 0});

    allocation = (unsigned char *)bench_malloc(sizeof(payload));
    assert(allocation != NULL);
    memcpy(allocation, payload, sizeof(payload));
    before = bench_alloc_snapshot();
    assert(bench_realloc(allocation, SIZE_MAX) == NULL);
    after = bench_alloc_snapshot();
    assert_stats_equal(after, before);
    assert(memcmp(allocation, payload, sizeof(payload)) == 0);
    bench_free(allocation);
    after = bench_alloc_end();
    assert(after.current_bytes == 0);
    assert(after.allocations == 1);
    assert(after.frees == 1);
}

static void test_unscoped_allocations_stay_untracked(void)
{
    bench_alloc_stats stats;
    void *allocation;

    bench_alloc_reset();
    allocation = bench_malloc(16);
    assert(allocation != NULL);
    assert_stats_equal(bench_alloc_snapshot(),
                       (bench_alloc_stats){0, 0, 0, 0});
    bench_alloc_begin();
    allocation = bench_realloc(allocation, 32);
    assert(allocation != NULL);
    bench_free(allocation);
    stats = bench_alloc_end();
    assert_stats_equal(stats, (bench_alloc_stats){0, 0, 0, 0});
}

static void test_tracked_block_outlives_scope(void)
{
    bench_alloc_stats ended;
    bench_alloc_stats after_free;
    void *allocation;

    bench_alloc_reset();
    bench_alloc_begin();
    allocation = bench_malloc(37);
    assert(allocation != NULL);
    ended = bench_alloc_end();
    assert(ended.current_bytes == 37);
    assert(ended.peak_bytes == 37);
    assert(ended.allocations == 1);
    assert(ended.frees == 0);

    bench_free(allocation);
    after_free = bench_alloc_snapshot();
    assert(after_free.current_bytes == 0);
    assert(after_free.peak_bytes == 37);
    assert(after_free.allocations == 1);
    assert(after_free.frees == 0);
}

static void test_scope_controls_and_null_free(void)
{
    bench_alloc_stats stats;
    void *allocation;
    void *old_allocation;

    bench_alloc_reset();
    bench_free(NULL);
    assert_stats_equal(bench_alloc_end(),
                       (bench_alloc_stats){0, 0, 0, 0});
    bench_alloc_begin();
    allocation = bench_malloc(11);
    assert(allocation != NULL);
    bench_alloc_begin();
    stats = bench_alloc_snapshot();
    assert(stats.current_bytes == 11);
    assert(stats.allocations == 1);
    stats = bench_alloc_end();
    assert(stats.current_bytes == 11);
    assert_stats_equal(bench_alloc_end(), stats);
    bench_free(allocation);

    bench_alloc_reset();
    bench_alloc_begin();
    old_allocation = bench_malloc(5);
    assert(old_allocation != NULL);
    bench_alloc_reset();
    bench_alloc_begin();
    allocation = bench_malloc(7);
    assert(allocation != NULL);
    bench_free(old_allocation);
    stats = bench_alloc_snapshot();
    assert(stats.current_bytes == 7);
    assert(stats.allocations == 1);
    assert(stats.frees == 0);
    bench_free(allocation);
    stats = bench_alloc_end();
    assert(stats.current_bytes == 0);
    assert(stats.allocations == 1);
    assert(stats.frees == 1);
    bench_alloc_reset();
    assert_stats_equal(bench_alloc_snapshot(),
                       (bench_alloc_stats){0, 0, 0, 0});
}

int main(void)
{
    test_scope();
    test_zero_size_requests();
    test_overflow_and_failed_realloc();
    test_unscoped_allocations_stay_untracked();
    test_tracked_block_outlives_scope();
    test_scope_controls_and_null_free();
    puts("bench allocation tests passed");
    return 0;
}
