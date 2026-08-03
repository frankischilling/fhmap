CC ?= cc
CFLAGS_COMMON := -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
BUILD := build
TEST_SRC := tests/test_fhmap.c
BENCH_COMMON_TEST_SRC := tests/test_bench_common.c
BENCH_COMMON_SRC := bench/compare/common/bench_common.c
BENCH_SRC := bench/bench_fhmap.c

.PHONY: all test clang-test sanitize bench clean

all: test

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_fhmap: $(TEST_SRC) fhmap.h | $(BUILD)
	$(CC) $(CFLAGS_COMMON) -O2 -g $(TEST_SRC) -o $@

$(BUILD)/test_bench_common: $(BENCH_COMMON_TEST_SRC) $(BENCH_COMMON_SRC) \
		bench/compare/common/bench_common.h | $(BUILD)
	$(CC) $(CFLAGS_COMMON) -O2 -g $(BENCH_COMMON_TEST_SRC) \
		$(BENCH_COMMON_SRC) -o $@

test: $(BUILD)/test_fhmap $(BUILD)/test_bench_common
	./$(BUILD)/test_fhmap
	./$(BUILD)/test_bench_common

clang-test: | $(BUILD)
	clang $(CFLAGS_COMMON) -O2 -g $(TEST_SRC) -o $(BUILD)/test_fhmap_clang
	./$(BUILD)/test_fhmap_clang
	clang $(CFLAGS_COMMON) -O2 -g $(BENCH_COMMON_TEST_SRC) \
		$(BENCH_COMMON_SRC) -o $(BUILD)/test_bench_common_clang
	./$(BUILD)/test_bench_common_clang

sanitize: | $(BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -fno-omit-frame-pointer \
		-fsanitize=address,undefined $(TEST_SRC) -o $(BUILD)/test_fhmap_sanitize
	ASAN_OPTIONS=detect_leaks=0 ./$(BUILD)/test_fhmap_sanitize
	$(CC) $(CFLAGS_COMMON) -O1 -g -fno-omit-frame-pointer \
		-fsanitize=address,undefined $(BENCH_COMMON_TEST_SRC) \
		$(BENCH_COMMON_SRC) -o $(BUILD)/test_bench_common_sanitize
	ASAN_OPTIONS=detect_leaks=0 ./$(BUILD)/test_bench_common_sanitize

$(BUILD)/bench_fhmap: $(BENCH_SRC) fhmap.h | $(BUILD)
	$(CC) $(CFLAGS_COMMON) -O3 -DNDEBUG -march=native $(BENCH_SRC) -o $@

bench: $(BUILD)/bench_fhmap
	./$(BUILD)/bench_fhmap

clean:
	rm -rf $(BUILD)
