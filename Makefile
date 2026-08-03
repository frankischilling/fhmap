CC ?= cc
CFLAGS_COMMON := -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
BUILD := build
TEST_SRC := tests/test_fhmap.c
BENCH_COMMON_TEST_SRC := tests/test_bench_common.c
BENCH_COMMON_SRC := bench/compare/common/bench_common.c
BENCH_ALLOC_TEST_SRC := tests/test_bench_alloc.c
BENCH_ALLOC_SRC := bench/compare/common/bench_alloc.c
BENCH_COMPARE_DRIVER_SRC := bench/compare/common/bench_driver.c
BENCH_COMPARE_ADAPTER_SRC := bench/compare/adapters/fhmap_adapter.c
BENCH_COMPARE_SRC := $(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
	$(BENCH_ALLOC_SRC) $(BENCH_COMPARE_ADAPTER_SRC)
BENCH_COMPARE_BUILD := $(BUILD)/bench-compare
BENCH_COMPARE_FHMAP := $(BENCH_COMPARE_BUILD)/fhmap
BENCH_COMPARE_CC := $(BENCH_COMPARE_BUILD)/cc
BENCH_COMPARE_VERSTABLE := $(BENCH_COMPARE_BUILD)/verstable
BENCH_COMPARE_KHASHL := $(BENCH_COMPARE_BUILD)/khashl
BENCH_COMPARE_KHASH := $(BENCH_COMPARE_BUILD)/khash
BENCH_COMPARE_KHASHL_FAULT_U64 := \
	$(BENCH_COMPARE_BUILD)/khashl_init_fail_u64
BENCH_COMPARE_KHASHL_FAULT_CSTR := \
	$(BENCH_COMPARE_BUILD)/khashl_init_fail_cstr
BENCH_COMPARE_PERMUTED := $(BENCH_COMPARE_BUILD)/fhmap_permuted_hits
BENCH_COMPARE_FETCHER := bench/compare/fetch-competitors.sh
BENCH_COMPARE_LOCK := bench/compare/competitors.lock
BENCH_COMPARE_COMPETITORS := $(BUILD)/competitors
BENCH_SRC := bench/bench_fhmap.c

.PHONY: all test clang-test sanitize bench bench-compare-lock \
	bench-compare-fetch bench-compare-smoke clean

all: test

$(BUILD):
	mkdir -p $(BUILD)

$(BENCH_COMPARE_BUILD): | $(BUILD)
	mkdir -p $(BENCH_COMPARE_BUILD)

$(BUILD)/test_fhmap: $(TEST_SRC) fhmap.h | $(BUILD)
	$(CC) $(CFLAGS_COMMON) -O2 -g $(TEST_SRC) -o $@

$(BUILD)/test_bench_common: $(BENCH_COMMON_TEST_SRC) $(BENCH_COMMON_SRC) \
		bench/compare/common/bench_common.h | $(BUILD)
	$(CC) $(CFLAGS_COMMON) -O2 -g $(BENCH_COMMON_TEST_SRC) \
		$(BENCH_COMMON_SRC) -o $@

$(BUILD)/test_bench_alloc: $(BENCH_ALLOC_TEST_SRC) $(BENCH_ALLOC_SRC) \
		bench/compare/common/bench_alloc.h | $(BUILD)
	$(CC) $(CFLAGS_COMMON) -O2 -g $(BENCH_ALLOC_TEST_SRC) \
		$(BENCH_ALLOC_SRC) -o $@

test: bench-compare-lock $(BUILD)/test_fhmap $(BUILD)/test_bench_common \
		$(BUILD)/test_bench_alloc
	./$(BUILD)/test_fhmap
	./$(BUILD)/test_bench_common
	./$(BUILD)/test_bench_alloc

clang-test: | $(BUILD) $(BENCH_COMPARE_BUILD)
	clang $(CFLAGS_COMMON) -O2 -g $(TEST_SRC) -o $(BUILD)/test_fhmap_clang
	./$(BUILD)/test_fhmap_clang
	clang $(CFLAGS_COMMON) -O2 -g $(BENCH_COMMON_TEST_SRC) \
		$(BENCH_COMMON_SRC) -o $(BUILD)/test_bench_common_clang
	./$(BUILD)/test_bench_common_clang
	clang $(CFLAGS_COMMON) -O2 -g $(BENCH_ALLOC_TEST_SRC) \
		$(BENCH_ALLOC_SRC) -o $(BUILD)/test_bench_alloc_clang
	./$(BUILD)/test_bench_alloc_clang
	clang $(CFLAGS_COMMON) -O3 -DNDEBUG $(BENCH_COMPARE_SRC) \
		-o $(BENCH_COMPARE_BUILD)/fhmap_clang
	$(BENCH_COMPARE_BUILD)/fhmap_clang --smoke --count 32 --samples 3 \
		>/dev/null

sanitize: | $(BUILD) $(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -fno-omit-frame-pointer \
		-fsanitize=address,undefined $(TEST_SRC) -o $(BUILD)/test_fhmap_sanitize
	ASAN_OPTIONS=detect_leaks=0 ./$(BUILD)/test_fhmap_sanitize
	$(CC) $(CFLAGS_COMMON) -O1 -g -fno-omit-frame-pointer \
		-fsanitize=address,undefined $(BENCH_COMMON_TEST_SRC) \
		$(BENCH_COMMON_SRC) -o $(BUILD)/test_bench_common_sanitize
	ASAN_OPTIONS=detect_leaks=0 ./$(BUILD)/test_bench_common_sanitize
	$(CC) $(CFLAGS_COMMON) -O1 -g -fno-omit-frame-pointer \
		-fsanitize=address,undefined $(BENCH_ALLOC_TEST_SRC) \
		$(BENCH_ALLOC_SRC) -o $(BUILD)/test_bench_alloc_sanitize
	ASAN_OPTIONS=detect_leaks=0 ./$(BUILD)/test_bench_alloc_sanitize
	$(CC) $(CFLAGS_COMMON) -O1 -g -fno-omit-frame-pointer \
		-fsanitize=address,undefined $(BENCH_COMPARE_SRC) \
		-o $(BENCH_COMPARE_BUILD)/fhmap_sanitize
	ASAN_OPTIONS=detect_leaks=0 $(BENCH_COMPARE_BUILD)/fhmap_sanitize \
		--smoke --count 32 --samples 3 >/dev/null

$(BENCH_COMPARE_FHMAP): $(BENCH_COMPARE_SRC) \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_common.h \
		bench/compare/common/bench_alloc.h fhmap.h | $(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O3 -DNDEBUG -march=native \
		$(BENCH_COMPARE_SRC) -o $@

$(BENCH_COMPARE_CC): $(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/cc_adapter.c \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_common.h \
		bench/compare/common/bench_alloc.h \
		$(BENCH_COMPARE_COMPETITORS)/cc/cc.h | $(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O3 -DNDEBUG -march=native \
		-isystem $(BENCH_COMPARE_COMPETITORS)/cc \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/cc_adapter.c -o $@

$(BENCH_COMPARE_VERSTABLE): $(BENCH_COMPARE_DRIVER_SRC) \
		$(BENCH_COMMON_SRC) $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/verstable_adapter.c \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_common.h \
		bench/compare/common/bench_alloc.h \
		$(BENCH_COMPARE_COMPETITORS)/verstable/verstable.h | \
		$(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O3 -DNDEBUG -march=native \
		-isystem $(BENCH_COMPARE_COMPETITORS)/verstable \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/verstable_adapter.c -o $@

$(BENCH_COMPARE_KHASHL): $(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/khashl_adapter.c \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_common.h \
		bench/compare/common/bench_alloc.h \
		$(BENCH_COMPARE_COMPETITORS)/khashl/khashl.h | \
		$(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O3 -DNDEBUG -march=native \
		-isystem $(BENCH_COMPARE_COMPETITORS)/khashl \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/khashl_adapter.c -o $@

$(BENCH_COMPARE_KHASH): $(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/khash_adapter.c \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_common.h \
		bench/compare/common/bench_alloc.h \
		$(BENCH_COMPARE_COMPETITORS)/khash/khash.h | \
		$(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O3 -DNDEBUG -march=native \
		-isystem $(BENCH_COMPARE_COMPETITORS)/khash \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/khash_adapter.c -o $@

$(BENCH_COMPARE_KHASHL_FAULT_U64): tests/test_khashl_init_fault.c \
		$(BENCH_COMMON_SRC) $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/khashl_adapter.c \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_common.h \
		bench/compare/common/bench_alloc.h \
		$(BENCH_COMPARE_COMPETITORS)/khashl/khashl.h | \
		$(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror \
		-DKHASHL_FAULT_KIND=BENCH_KEYS_U64 \
		-isystem $(BENCH_COMPARE_COMPETITORS)/khashl \
		tests/test_khashl_init_fault.c $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/khashl_adapter.c \
		-Wl,--wrap=bench_calloc -o $@

$(BENCH_COMPARE_KHASHL_FAULT_CSTR): tests/test_khashl_init_fault.c \
		$(BENCH_COMMON_SRC) $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/khashl_adapter.c \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_common.h \
		bench/compare/common/bench_alloc.h \
		$(BENCH_COMPARE_COMPETITORS)/khashl/khashl.h | \
		$(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror \
		-DKHASHL_FAULT_KIND=BENCH_KEYS_CSTR \
		-isystem $(BENCH_COMPARE_COMPETITORS)/khashl \
		tests/test_khashl_init_fault.c $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/khashl_adapter.c \
		-Wl,--wrap=bench_calloc -o $@

$(BENCH_COMPARE_PERMUTED): $(BENCH_COMPARE_SRC) \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_common.h \
		bench/compare/common/bench_alloc.h fhmap.h | $(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O3 -DNDEBUG \
		-DFHMAP_BENCH_TEST_PERMUTE_LOOKUP_HITS \
		$(BENCH_COMPARE_SRC) -o $@

bench-compare-lock: $(BENCH_COMPARE_FETCHER) $(BENCH_COMPARE_LOCK)
	sh $(BENCH_COMPARE_FETCHER) --check-lock

bench-compare-fetch: bench-compare-lock | $(BUILD)
	sh $(BENCH_COMPARE_FETCHER) --dest $(BENCH_COMPARE_COMPETITORS)

bench-compare-smoke: bench-compare-lock $(BENCH_COMPARE_FHMAP) \
		$(BENCH_COMPARE_CC) \
		$(BENCH_COMPARE_VERSTABLE) \
		$(BENCH_COMPARE_KHASHL) \
		$(BENCH_COMPARE_KHASH) \
		$(BENCH_COMPARE_KHASHL_FAULT_U64) \
		$(BENCH_COMPARE_KHASHL_FAULT_CSTR) \
		$(BENCH_COMPARE_PERMUTED)
	sh tests/test_bench_compare.sh

$(BUILD)/bench_fhmap: $(BENCH_SRC) fhmap.h | $(BUILD)
	$(CC) $(CFLAGS_COMMON) -O3 -DNDEBUG -march=native $(BENCH_SRC) -o $@

bench: $(BUILD)/bench_fhmap
	./$(BUILD)/bench_fhmap

clean:
	rm -rf $(BUILD)
