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
BENCH_COMPARE_PERMUTED := $(BENCH_COMPARE_BUILD)/fhmap_permuted_hits
BENCH_SRC := bench/bench_fhmap.c

.PHONY: all test clang-test sanitize bench bench-compare-smoke clean

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

test: $(BUILD)/test_fhmap $(BUILD)/test_bench_common \
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

$(BENCH_COMPARE_PERMUTED): $(BENCH_COMPARE_SRC) \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_common.h \
		bench/compare/common/bench_alloc.h fhmap.h | $(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O3 -DNDEBUG \
		-DFHMAP_BENCH_TEST_PERMUTE_LOOKUP_HITS \
		$(BENCH_COMPARE_SRC) -o $@

bench-compare-smoke: $(BENCH_COMPARE_FHMAP) $(BENCH_COMPARE_PERMUTED)
	sh tests/test_bench_compare.sh

$(BUILD)/bench_fhmap: $(BENCH_SRC) fhmap.h | $(BUILD)
	$(CC) $(CFLAGS_COMMON) -O3 -DNDEBUG -march=native $(BENCH_SRC) -o $@

bench: $(BUILD)/bench_fhmap
	./$(BUILD)/bench_fhmap

clean:
	rm -rf $(BUILD)
