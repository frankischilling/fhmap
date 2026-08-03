#define main bench_khashl_unused_driver_main
#include "../bench/compare/common/bench_driver.c"
#undef main

#ifndef KHASHL_FAULT_KIND
#error KHASHL_FAULT_KIND must select the tested key kind
#endif

void *__wrap_bench_calloc(size_t count, size_t size)
{
    (void)count;
    (void)size;
    return NULL;
}

int main(void)
{
    bench_dataset data = BENCH_DATASET_ZERO;
    bench_prepared_values values = {0};
    bench_alloc_stats memory;
    bool succeeded;

    if (!bench_dataset_init(&data, 1u, UINT64_C(0)) ||
        !bench_prepared_values_init(&values, &data)) {
        fputs("could not prepare khashl fault test\n", stderr);
        bench_prepared_values_destroy(&values);
        bench_dataset_destroy(&data);
        return 3;
    }
    succeeded = bench_run_untimed_map(&data, KHASHL_FAULT_KIND, &values,
                                      BENCH_INSERT_RESERVED, NULL);
    memory = bench_alloc_snapshot();
    bench_prepared_values_destroy(&values);
    bench_dataset_destroy(&data);
    if (succeeded) {
        fputs("khashl state allocation fault was not observed\n", stderr);
        return 3;
    }
    if (memory.current_bytes != 0u || memory.allocations != 1u ||
        memory.frees != 1u) {
        fputs("khashl state allocation fault accounting mismatch\n", stderr);
        return 3;
    }
    return 1;
}
