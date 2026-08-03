#include "../bench/compare/common/bench_common.h"

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_mixer(void)
{
    assert(bench_mix64(UINT64_C(0)) == UINT64_C(0));
    assert(bench_mix64(UINT64_C(1)) == UINT64_C(0x5692161d100b05e5));
    assert(bench_mix64(UINT64_C(0x0123456789abcdef)) ==
           UINT64_C(0xb2c058e4ebb5112c));
    assert(bench_mix64(UINT64_MAX) == UINT64_C(0xb4d055fcf2cbbd7b));
}

static void test_statistics(void)
{
    double odd[] = {9.0, 1.0, 4.0};
    double odd_original[] = {9.0, 1.0, 4.0};
    double even[] = {8.0, 2.0, 6.0, 4.0};
    double even_original[] = {8.0, 2.0, 6.0, 4.0};

    assert(bench_median(odd, 3) == 4.0);
    assert(memcmp(odd, odd_original, sizeof(odd)) == 0);
    assert(bench_median(even, 4) == 5.0);
    assert(memcmp(even, even_original, sizeof(even)) == 0);
    assert(bench_mad(odd, 3, 4.0) == 3.0);
    assert(memcmp(odd, odd_original, sizeof(odd)) == 0);
    assert(bench_median(NULL, 0) == 0.0);
    assert(bench_mad(NULL, 0, 0.0) == 0.0);
}

static void test_checked_statistics(void)
{
    double odd[] = {9.0, 1.0, 4.0};
    double even[] = {8.0, 2.0, 6.0, 4.0};
    double single[] = {7.0};
    double not_finite[] = {1.0, INFINITY, 3.0};
    double not_a_number[] = {1.0, NAN, 3.0};
    double median = -1.0;
    double mad = -1.0;

    assert(bench_statistics(odd, 3, &median, &mad));
    assert(median == 4.0);
    assert(mad == 3.0);
    assert(odd[0] == 0.0 && odd[1] == 3.0 && odd[2] == 5.0);

    assert(bench_statistics(even, 4, &median, &mad));
    assert(median == 5.0);
    assert(mad == 2.0);
    assert(even[0] == 1.0 && even[1] == 1.0 &&
           even[2] == 3.0 && even[3] == 3.0);

    assert(bench_statistics(single, 1, &median, &mad));
    assert(median == 7.0);
    assert(mad == 0.0);

    median = 11.0;
    mad = 12.0;
    assert(!bench_statistics(NULL, 1, &median, &mad));
    assert(!bench_statistics(odd, 0, &median, &mad));
    assert(!bench_statistics(odd, 3, NULL, &mad));
    assert(!bench_statistics(odd, 3, &median, NULL));
    assert(!bench_statistics(not_finite, 3, &median, &mad));
    assert(!bench_statistics(not_a_number, 3, &median, &mad));
    assert(median == 11.0 && mad == 12.0);
}

static void assert_printable_string(const char *string)
{
    const unsigned char *cursor = (const unsigned char *)string;

    assert(*cursor != '\0');
    while (*cursor != '\0') {
        assert(isprint(*cursor));
        ++cursor;
    }
}

static void test_dataset(void)
{
    bench_dataset a = BENCH_DATASET_ZERO;
    bench_dataset b = BENCH_DATASET_ZERO;
    char borrowed_copy[32];
    const char *borrowed;
    bool saw_shuffled_key = false;
    size_t i;

    assert(bench_dataset_init(&a, 4096, UINT64_C(1234)));
    assert(bench_dataset_init(&b, 4096, UINT64_C(1234)));
    assert(a.count == 4096);
    assert(a.seed == UINT64_C(1234));

    borrowed = a.str_keys[0];
    assert(strlen(borrowed) < sizeof(borrowed_copy));
    memcpy(borrowed_copy, borrowed, strlen(borrowed) + 1u);

    for (i = 0; i < a.count; ++i) {
        assert(a.u64_keys[i] == b.u64_keys[i]);
        assert(a.u64_misses[i] == b.u64_misses[i]);
        assert((a.u64_keys[i] & UINT64_C(1)) == UINT64_C(0));
        assert((a.u64_misses[i] & UINT64_C(1)) == UINT64_C(1));
        assert(a.u64_misses[i] == a.u64_keys[i] + UINT64_C(1));
        assert(a.u64_keys[i] != a.u64_misses[i]);
        assert(strcmp(a.str_keys[i], b.str_keys[i]) == 0);
        assert(strcmp(a.str_keys[i], a.str_misses[i]) != 0);
        assert_printable_string(a.str_keys[i]);
        assert_printable_string(a.str_misses[i]);
        if (a.u64_keys[i] != (uint64_t)i * UINT64_C(2)) {
            saw_shuffled_key = true;
        }
    }
    assert(saw_shuffled_key);

    bench_dataset_destroy(&b);
    assert(strcmp(borrowed, borrowed_copy) == 0);
    assert(a.str_keys[0] == borrowed);
    bench_dataset_destroy(&a);
}

static void test_dataset_rejects_invalid_input(void)
{
    bench_dataset data = BENCH_DATASET_ZERO;

    assert(!bench_dataset_init(NULL, 1, UINT64_C(1)));
    assert(!bench_dataset_init(&data, 0, UINT64_C(1)));
    assert(!bench_dataset_init(&data, SIZE_MAX, UINT64_C(1)));
    bench_dataset_destroy(&data);
    bench_dataset_destroy(&data);
}

int main(void)
{
    test_mixer();
    test_statistics();
    test_checked_statistics();
    test_dataset();
    test_dataset_rejects_invalid_input();
    puts("bench common tests passed");
    return 0;
}
