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
BENCH_COMPARE_COMPETITORS := $(BUILD)/competitors
BENCH_COMPARE_FETCH_STAMP := $(BUILD)/.bench-compare-fetch.stamp
BENCH_COMPARE_FHMAP := $(BENCH_COMPARE_BUILD)/fhmap
BENCH_COMPARE_CC := $(BENCH_COMPARE_BUILD)/cc
BENCH_COMPARE_VERSTABLE := $(BENCH_COMPARE_BUILD)/verstable
BENCH_COMPARE_KHASHL := $(BENCH_COMPARE_BUILD)/khashl
BENCH_COMPARE_KHASH := $(BENCH_COMPARE_BUILD)/khash
BENCH_COMPARE_MLIB := $(BENCH_COMPARE_BUILD)/mlib
BENCH_COMPARE_STC := $(BENCH_COMPARE_BUILD)/stc
BENCH_COMPARE_STB_DS := $(BENCH_COMPARE_BUILD)/stb_ds
BENCH_COMPARE_UTHASH := $(BENCH_COMPARE_BUILD)/uthash
BENCH_COMPARE_TIDWALL := $(BENCH_COMPARE_BUILD)/tidwall
BENCH_COMPARE_POTTERY := $(BENCH_COMPARE_BUILD)/pottery
BENCH_COMPARE_BINS := $(BENCH_COMPARE_FHMAP) $(BENCH_COMPARE_CC) \
	$(BENCH_COMPARE_VERSTABLE) $(BENCH_COMPARE_KHASHL) \
	$(BENCH_COMPARE_KHASH) $(BENCH_COMPARE_MLIB) $(BENCH_COMPARE_STC) \
	$(BENCH_COMPARE_STB_DS) $(BENCH_COMPARE_UTHASH) \
	$(BENCH_COMPARE_TIDWALL) $(BENCH_COMPARE_POTTERY)
BENCH_COMPARE_TIDWALL_OBJ := $(BENCH_COMPARE_BUILD)/tidwall_hashmap.o
BENCH_COMPARE_TYPED_TEST_SRC := tests/test_typed_adapter.c
BENCH_COMPARE_MLIB_TEST := $(BENCH_COMPARE_BUILD)/test_mlib_adapter
BENCH_COMPARE_STC_TEST := $(BENCH_COMPARE_BUILD)/test_stc_adapter
BENCH_COMPARE_STB_DS_TEST := $(BENCH_COMPARE_BUILD)/test_stb_ds_adapter
BENCH_COMPARE_STB_DS_SEED_TEST_SRC := tests/test_stb_ds_seed.c
BENCH_COMPARE_STB_DS_SEED_TEST := $(BENCH_COMPARE_BUILD)/test_stb_ds_seed
BENCH_COMPARE_UTHASH_TEST := $(BENCH_COMPARE_BUILD)/test_uthash_adapter
BENCH_COMPARE_TIDWALL_TEST := $(BENCH_COMPARE_BUILD)/test_tidwall_adapter
BENCH_COMPARE_POTTERY_TEST := $(BENCH_COMPARE_BUILD)/test_pottery_adapter
BENCH_COMPARE_MLIB_HEADERS := \
	$(BENCH_COMPARE_COMPETITORS)/mlib/m-array.h \
	$(BENCH_COMPARE_COMPETITORS)/mlib/m-core.h \
	$(BENCH_COMPARE_COMPETITORS)/mlib/m-dict.h
BENCH_COMPARE_STB_DS_HEADERS := \
	$(BENCH_COMPARE_COMPETITORS)/stb_ds/stb_ds.h
BENCH_COMPARE_STC_HEADERS := \
	$(BENCH_COMPARE_COMPETITORS)/stc/include/stc/common.h \
	$(BENCH_COMPARE_COMPETITORS)/stc/include/stc/hashmap.h \
	$(BENCH_COMPARE_COMPETITORS)/stc/include/stc/hmap.h \
	$(BENCH_COMPARE_COMPETITORS)/stc/include/stc/priv/linkage.h \
	$(BENCH_COMPARE_COMPETITORS)/stc/include/stc/priv/linkage2.h \
	$(BENCH_COMPARE_COMPETITORS)/stc/include/stc/priv/template.h \
	$(BENCH_COMPARE_COMPETITORS)/stc/include/stc/priv/template2.h \
	$(BENCH_COMPARE_COMPETITORS)/stc/include/stc/sys/finalize.h \
	$(BENCH_COMPARE_COMPETITORS)/stc/include/stc/types.h
BENCH_COMPARE_UTHASH_HEADERS := \
	$(BENCH_COMPARE_COMPETITORS)/uthash/src/uthash.h
BENCH_COMPARE_TIDWALL_SOURCES := \
	$(BENCH_COMPARE_COMPETITORS)/tidwall/hashmap.h \
	$(BENCH_COMPARE_COMPETITORS)/tidwall/hashmap.c
BENCH_COMPARE_POTTERY_INCLUDE := \
	$(BENCH_COMPARE_COMPETITORS)/pottery/include/pottery
BENCH_COMPARE_POTTERY_HEADERS := \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/pottery_dependencies.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/platform/pottery_platform_headers.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/platform/pottery_platform_features.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/platform/pottery_platform_attributes.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/platform/pottery_platform_functions.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/platform/pottery_platform_pragma_push.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/platform/pottery_platform_pragma_pop.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/open_hash_map/pottery_open_hash_map_static.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/open_hash_map/impl/pottery_open_hash_map_config_types.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/container_types/pottery_container_types_static.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/container_types/impl/pottery_container_types_macros.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/container_types/impl/pottery_container_types_declarations.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/container_types/impl/pottery_container_types_unmacros.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/open_hash_map/impl/pottery_open_hash_map_config_lifecycle.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/lifecycle/pottery_lifecycle_static.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/lifecycle/impl/pottery_lifecycle_macros.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/lifecycle/impl/pottery_lifecycle_declarations.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/lifecycle/impl/pottery_lifecycle_unmacros.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/open_hash_map/impl/pottery_open_hash_map_macros.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/open_hash_map/impl/pottery_open_hash_map_forward.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/lifecycle/pottery_lifecycle_cleanup.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/container_types/pottery_container_types_cleanup.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/open_hash_map/impl/pottery_open_hash_map_config_table.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/open_hash_table/pottery_open_hash_table_static.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/open_hash_table/impl/pottery_open_hash_table_config_types.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/open_hash_table/impl/pottery_open_hash_table_config_lifecycle.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/open_hash_table/impl/pottery_open_hash_table_config_array_access.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/array_access/pottery_array_access_static.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/array_access/impl/pottery_array_access_macros.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/array_access/impl/pottery_array_access_declarations.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/array_access/impl/pottery_array_access_unmacros.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/open_hash_table/impl/pottery_open_hash_table_macros.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/open_hash_table/impl/pottery_open_hash_table_declarations.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/open_hash_table/impl/pottery_open_hash_table_definitions.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/array_access/pottery_array_access_cleanup.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/open_hash_table/impl/pottery_open_hash_table_unmacros.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/open_hash_map/impl/pottery_open_hash_map_config_alloc.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/alloc/pottery_alloc_static.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/alloc/impl/pottery_alloc_macros.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/alloc/impl/pottery_alloc_declarations.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/alloc/impl/pottery_alloc_definitions.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/alloc/impl/pottery_alloc_unmacros.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/open_hash_map/impl/pottery_open_hash_map_declarations.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/open_hash_map/impl/pottery_open_hash_map_definitions.t.h \
	$(BENCH_COMPARE_POTTERY_INCLUDE)/open_hash_map/impl/pottery_open_hash_map_unmacros.t.h
BENCH_COMPARE_TYPED_COMMON_DEPS := $(BENCH_COMPARE_DRIVER_SRC) \
	$(BENCH_COMMON_SRC) $(BENCH_ALLOC_SRC) \
	bench/compare/common/bench_adapter.h \
	bench/compare/common/bench_common.h \
	bench/compare/common/bench_alloc.h
BENCH_COMPARE_TYPED_HEADERS := $(BENCH_COMPARE_MLIB_HEADERS) \
	$(BENCH_COMPARE_STB_DS_HEADERS) $(BENCH_COMPARE_STC_HEADERS) \
	$(BENCH_COMPARE_UTHASH_HEADERS) $(BENCH_COMPARE_TIDWALL_SOURCES) \
	$(BENCH_COMPARE_POTTERY_HEADERS)
BENCH_COMPARE_COMPETITOR_FILES := \
	$(BENCH_COMPARE_COMPETITORS)/cc/cc.h \
	$(BENCH_COMPARE_COMPETITORS)/verstable/verstable.h \
	$(BENCH_COMPARE_COMPETITORS)/khashl/khashl.h \
	$(BENCH_COMPARE_COMPETITORS)/khash/khash.h \
	$(BENCH_COMPARE_TYPED_HEADERS)
BENCH_COMPARE_MISSING_FILES := $(filter-out \
	$(wildcard $(BENCH_COMPARE_COMPETITOR_FILES)), \
	$(BENCH_COMPARE_COMPETITOR_FILES))
ifeq ($(sort $(BENCH_COMPARE_TYPED_HEADERS)),\
	$(sort $(wildcard $(BENCH_COMPARE_TYPED_HEADERS))))
BENCH_COMPARE_TYPED_AVAILABLE := 1
else
BENCH_COMPARE_TYPED_AVAILABLE := 0
endif
BENCH_COMPARE_MLIB_CLANG := $(BENCH_COMPARE_BUILD)/mlib_clang
BENCH_COMPARE_STC_CLANG := $(BENCH_COMPARE_BUILD)/stc_clang
BENCH_COMPARE_STB_DS_CLANG := $(BENCH_COMPARE_BUILD)/stb_ds_clang
BENCH_COMPARE_MLIB_TEST_CLANG := \
	$(BENCH_COMPARE_BUILD)/test_mlib_adapter_clang
BENCH_COMPARE_STC_TEST_CLANG := \
	$(BENCH_COMPARE_BUILD)/test_stc_adapter_clang
BENCH_COMPARE_STB_DS_TEST_CLANG := \
	$(BENCH_COMPARE_BUILD)/test_stb_ds_adapter_clang
BENCH_COMPARE_STB_DS_SEED_TEST_CLANG := \
	$(BENCH_COMPARE_BUILD)/test_stb_ds_seed_clang
BENCH_COMPARE_UTHASH_CLANG := $(BENCH_COMPARE_BUILD)/uthash_clang
BENCH_COMPARE_TIDWALL_CLANG := $(BENCH_COMPARE_BUILD)/tidwall_clang
BENCH_COMPARE_POTTERY_CLANG := $(BENCH_COMPARE_BUILD)/pottery_clang
BENCH_COMPARE_TIDWALL_OBJ_CLANG := \
	$(BENCH_COMPARE_BUILD)/tidwall_hashmap_clang.o
BENCH_COMPARE_UTHASH_TEST_CLANG := \
	$(BENCH_COMPARE_BUILD)/test_uthash_adapter_clang
BENCH_COMPARE_TIDWALL_TEST_CLANG := \
	$(BENCH_COMPARE_BUILD)/test_tidwall_adapter_clang
BENCH_COMPARE_POTTERY_TEST_CLANG := \
	$(BENCH_COMPARE_BUILD)/test_pottery_adapter_clang
BENCH_COMPARE_MLIB_SANITIZE := $(BENCH_COMPARE_BUILD)/mlib_sanitize
BENCH_COMPARE_STC_SANITIZE := $(BENCH_COMPARE_BUILD)/stc_sanitize
BENCH_COMPARE_STB_DS_SANITIZE := $(BENCH_COMPARE_BUILD)/stb_ds_sanitize
BENCH_COMPARE_MLIB_TEST_SANITIZE := \
	$(BENCH_COMPARE_BUILD)/test_mlib_adapter_sanitize
BENCH_COMPARE_STC_TEST_SANITIZE := \
	$(BENCH_COMPARE_BUILD)/test_stc_adapter_sanitize
BENCH_COMPARE_STB_DS_TEST_SANITIZE := \
	$(BENCH_COMPARE_BUILD)/test_stb_ds_adapter_sanitize
BENCH_COMPARE_STB_DS_SEED_TEST_SANITIZE := \
	$(BENCH_COMPARE_BUILD)/test_stb_ds_seed_sanitize
BENCH_COMPARE_UTHASH_SANITIZE := $(BENCH_COMPARE_BUILD)/uthash_sanitize
BENCH_COMPARE_TIDWALL_SANITIZE := $(BENCH_COMPARE_BUILD)/tidwall_sanitize
BENCH_COMPARE_POTTERY_SANITIZE := $(BENCH_COMPARE_BUILD)/pottery_sanitize
BENCH_COMPARE_TIDWALL_OBJ_SANITIZE := \
	$(BENCH_COMPARE_BUILD)/tidwall_hashmap_sanitize.o
BENCH_COMPARE_UTHASH_TEST_SANITIZE := \
	$(BENCH_COMPARE_BUILD)/test_uthash_adapter_sanitize
BENCH_COMPARE_TIDWALL_TEST_SANITIZE := \
	$(BENCH_COMPARE_BUILD)/test_tidwall_adapter_sanitize
BENCH_COMPARE_POTTERY_TEST_SANITIZE := \
	$(BENCH_COMPARE_BUILD)/test_pottery_adapter_sanitize
ifeq ($(BENCH_COMPARE_TYPED_AVAILABLE),1)
BENCH_COMPARE_TYPED_CLANG := $(BENCH_COMPARE_MLIB_CLANG) \
	$(BENCH_COMPARE_STC_CLANG) $(BENCH_COMPARE_STB_DS_CLANG) \
	$(BENCH_COMPARE_MLIB_TEST_CLANG) $(BENCH_COMPARE_STC_TEST_CLANG) \
	$(BENCH_COMPARE_STB_DS_TEST_CLANG) \
	$(BENCH_COMPARE_STB_DS_SEED_TEST_CLANG) \
	$(BENCH_COMPARE_UTHASH_CLANG) $(BENCH_COMPARE_TIDWALL_CLANG) \
	$(BENCH_COMPARE_POTTERY_CLANG) \
	$(BENCH_COMPARE_UTHASH_TEST_CLANG) \
	$(BENCH_COMPARE_TIDWALL_TEST_CLANG) \
	$(BENCH_COMPARE_POTTERY_TEST_CLANG)
BENCH_COMPARE_TYPED_SANITIZE := $(BENCH_COMPARE_MLIB_SANITIZE) \
	$(BENCH_COMPARE_STC_SANITIZE) $(BENCH_COMPARE_STB_DS_SANITIZE) \
	$(BENCH_COMPARE_MLIB_TEST_SANITIZE) \
	$(BENCH_COMPARE_STC_TEST_SANITIZE) \
	$(BENCH_COMPARE_STB_DS_TEST_SANITIZE) \
	$(BENCH_COMPARE_STB_DS_SEED_TEST_SANITIZE) \
	$(BENCH_COMPARE_UTHASH_SANITIZE) $(BENCH_COMPARE_TIDWALL_SANITIZE) \
	$(BENCH_COMPARE_POTTERY_SANITIZE) \
	$(BENCH_COMPARE_UTHASH_TEST_SANITIZE) \
	$(BENCH_COMPARE_TIDWALL_TEST_SANITIZE) \
	$(BENCH_COMPARE_POTTERY_TEST_SANITIZE)
endif
BENCH_COMPARE_KHASHL_FAULT_U64 := \
	$(BENCH_COMPARE_BUILD)/khashl_init_fail_u64
BENCH_COMPARE_KHASHL_FAULT_CSTR := \
	$(BENCH_COMPARE_BUILD)/khashl_init_fail_cstr
BENCH_COMPARE_KHASH_RESERVE_TEST := \
	$(BENCH_COMPARE_BUILD)/test_khash_reserve
BENCH_COMPARE_KHASHL_RESERVE_TEST := \
	$(BENCH_COMPARE_BUILD)/test_khashl_reserve
BENCH_COMPARE_PERMUTED := $(BENCH_COMPARE_BUILD)/fhmap_permuted_hits
BENCH_COMPARE_TEST_BINS := $(BENCH_COMPARE_MLIB_TEST) \
	$(BENCH_COMPARE_STC_TEST) $(BENCH_COMPARE_STB_DS_TEST) \
	$(BENCH_COMPARE_STB_DS_SEED_TEST) $(BENCH_COMPARE_UTHASH_TEST) \
	$(BENCH_COMPARE_TIDWALL_TEST) $(BENCH_COMPARE_POTTERY_TEST) \
	$(BENCH_COMPARE_KHASHL_FAULT_U64) \
	$(BENCH_COMPARE_KHASHL_FAULT_CSTR) \
	$(BENCH_COMPARE_KHASH_RESERVE_TEST) \
	$(BENCH_COMPARE_KHASHL_RESERVE_TEST) $(BENCH_COMPARE_PERMUTED)
BENCH_COMPARE_FETCHER := ./bench/compare/fetch-competitors.sh
BENCH_COMPARE_LOCK := bench/compare/competitors.lock
BENCH_COMPARE_CFLAGS := $(CFLAGS_COMMON) -O3 -DNDEBUG -march=native
BENCH_SRC := bench/bench_fhmap.c

export FHMAP_BENCH_CC = $(CC)
export FHMAP_BENCH_CFLAGS = $(BENCH_COMPARE_CFLAGS)

.PHONY: all test clang-test sanitize bench bench-compare-lock \
	bench-compare-fetch bench-compare-build bench-compare-smoke \
	bench-compare-fetch-force bench-compare clean

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

clang-test: $(BENCH_COMPARE_TYPED_CLANG) | $(BUILD) $(BENCH_COMPARE_BUILD)
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
ifeq ($(BENCH_COMPARE_TYPED_AVAILABLE),1)
	$(BENCH_COMPARE_MLIB_CLANG) --smoke --count 32 --samples 3 >/dev/null
	$(BENCH_COMPARE_STC_CLANG) --smoke --count 32 --samples 3 >/dev/null
	$(BENCH_COMPARE_STB_DS_CLANG) --smoke --count 32 --samples 3 >/dev/null
	$(BENCH_COMPARE_MLIB_TEST_CLANG)
	$(BENCH_COMPARE_STC_TEST_CLANG)
	$(BENCH_COMPARE_STB_DS_TEST_CLANG)
	$(BENCH_COMPARE_STB_DS_SEED_TEST_CLANG)
	$(BENCH_COMPARE_UTHASH_CLANG) --smoke --count 32 --samples 3 >/dev/null
	$(BENCH_COMPARE_TIDWALL_CLANG) --smoke --count 32 --samples 3 >/dev/null
	$(BENCH_COMPARE_POTTERY_CLANG) --smoke --count 32 --samples 3 >/dev/null
	$(BENCH_COMPARE_UTHASH_TEST_CLANG)
	$(BENCH_COMPARE_TIDWALL_TEST_CLANG)
	$(BENCH_COMPARE_POTTERY_TEST_CLANG)
else
	@printf 'typed competitor sources are absent; skipping Clang matrix\n'
endif

sanitize: $(BENCH_COMPARE_TYPED_SANITIZE) | $(BUILD) $(BENCH_COMPARE_BUILD)
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
ifeq ($(BENCH_COMPARE_TYPED_AVAILABLE),1)
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
		$(BENCH_COMPARE_MLIB_SANITIZE) --smoke --count 32 --samples 3 \
		>/dev/null
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
		$(BENCH_COMPARE_STC_SANITIZE) --smoke --count 32 --samples 3 \
		>/dev/null
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
		$(BENCH_COMPARE_STB_DS_SANITIZE) --smoke --count 32 --samples 3 \
		>/dev/null
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
		$(BENCH_COMPARE_MLIB_TEST_SANITIZE)
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
		$(BENCH_COMPARE_STC_TEST_SANITIZE)
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
		$(BENCH_COMPARE_STB_DS_TEST_SANITIZE)
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
		$(BENCH_COMPARE_STB_DS_SEED_TEST_SANITIZE)
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
		$(BENCH_COMPARE_UTHASH_SANITIZE) --smoke --count 32 --samples 3 \
		>/dev/null
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
		$(BENCH_COMPARE_TIDWALL_SANITIZE) --smoke --count 32 --samples 3 \
		>/dev/null
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
		$(BENCH_COMPARE_POTTERY_SANITIZE) --smoke --count 32 --samples 3 \
		>/dev/null
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
		$(BENCH_COMPARE_UTHASH_TEST_SANITIZE)
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
		$(BENCH_COMPARE_TIDWALL_TEST_SANITIZE)
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
		$(BENCH_COMPARE_POTTERY_TEST_SANITIZE)
else
	@printf 'typed competitor sources are absent; skipping sanitizer matrix\n'
endif

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

$(BENCH_COMPARE_MLIB): $(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/mlib_adapter.c \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_common.h \
		bench/compare/common/bench_alloc.h \
		$(BENCH_COMPARE_MLIB_HEADERS) | \
		$(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O3 -DNDEBUG -march=native \
		-isystem $(BENCH_COMPARE_COMPETITORS)/mlib \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/mlib_adapter.c -o $@

$(BENCH_COMPARE_STC): $(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/stc_adapter.c \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_common.h \
		bench/compare/common/bench_alloc.h \
		$(BENCH_COMPARE_STC_HEADERS) | \
		$(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O3 -DNDEBUG -march=native \
		-isystem $(BENCH_COMPARE_COMPETITORS)/stc/include \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/stc_adapter.c -o $@

$(BENCH_COMPARE_STB_DS): $(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/stb_ds_adapter.c \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_common.h \
		bench/compare/common/bench_alloc.h \
		$(BENCH_COMPARE_STB_DS_HEADERS) | \
		$(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O3 -DNDEBUG -march=native \
		-isystem $(BENCH_COMPARE_COMPETITORS)/stb_ds \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/stb_ds_adapter.c -o $@

$(BENCH_COMPARE_UTHASH): $(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/uthash_adapter.c \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_common.h \
		bench/compare/common/bench_alloc.h \
		$(BENCH_COMPARE_COMPETITORS)/uthash/src/uthash.h | \
		$(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O3 -DNDEBUG -march=native -Werror \
		-isystem $(BENCH_COMPARE_COMPETITORS)/uthash/src \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/uthash_adapter.c -o $@

$(BENCH_COMPARE_UTHASH_TEST): tests/test_uthash_adapter.c \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/uthash_adapter.c \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_alloc.h \
		$(BENCH_COMPARE_COMPETITORS)/uthash/src/uthash.h | \
		$(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror \
		-isystem $(BENCH_COMPARE_COMPETITORS)/uthash/src \
		tests/test_uthash_adapter.c $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/uthash_adapter.c -o $@

$(BENCH_COMPARE_TIDWALL_OBJ): \
		$(BENCH_COMPARE_COMPETITORS)/tidwall/hashmap.c \
		$(BENCH_COMPARE_COMPETITORS)/tidwall/hashmap.h | \
		$(BENCH_COMPARE_BUILD)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -O3 -DNDEBUG -march=native \
		-isystem $(BENCH_COMPARE_COMPETITORS)/tidwall \
		-c $(BENCH_COMPARE_COMPETITORS)/tidwall/hashmap.c -o $@

$(BENCH_COMPARE_TIDWALL): $(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/tidwall_adapter.c \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_common.h \
		bench/compare/common/bench_alloc.h \
		$(BENCH_COMPARE_COMPETITORS)/tidwall/hashmap.h \
		$(BENCH_COMPARE_TIDWALL_OBJ) | \
		$(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O3 -DNDEBUG -march=native -Werror \
		-isystem $(BENCH_COMPARE_COMPETITORS)/tidwall \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/tidwall_adapter.c \
		$(BENCH_COMPARE_TIDWALL_OBJ) -o $@

$(BENCH_COMPARE_TIDWALL_TEST): tests/test_tidwall_adapter.c \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/tidwall_adapter.c \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_alloc.h \
		$(BENCH_COMPARE_COMPETITORS)/tidwall/hashmap.h \
		$(BENCH_COMPARE_TIDWALL_OBJ) | \
		$(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror \
		-DFHMAP_BENCH_ADAPTER_TESTING \
		-isystem $(BENCH_COMPARE_COMPETITORS)/tidwall \
		tests/test_tidwall_adapter.c $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/tidwall_adapter.c \
		$(BENCH_COMPARE_TIDWALL_OBJ) \
		-Wl,--wrap=bench_malloc -o $@

$(BENCH_COMPARE_POTTERY): $(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/pottery_adapter.c \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_common.h \
		bench/compare/common/bench_alloc.h \
		$(BENCH_COMPARE_POTTERY_HEADERS) | \
		$(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O3 -DNDEBUG -march=native -Werror \
		-isystem $(BENCH_COMPARE_COMPETITORS)/pottery/include \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/pottery_adapter.c -o $@

$(BENCH_COMPARE_POTTERY_TEST): tests/test_pottery_adapter.c \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/pottery_adapter.c \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_alloc.h \
		$(BENCH_COMPARE_POTTERY_HEADERS) | \
		$(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror \
		-DFHMAP_BENCH_ADAPTER_TESTING \
		-isystem $(BENCH_COMPARE_COMPETITORS)/pottery/include \
		tests/test_pottery_adapter.c $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/pottery_adapter.c \
		-Wl,--wrap=bench_malloc -Wl,--wrap=bench_calloc -o $@

$(BENCH_COMPARE_MLIB_TEST): $(BENCH_COMPARE_TYPED_TEST_SRC) \
		$(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		bench/compare/adapters/mlib_adapter.c \
		$(BENCH_COMPARE_MLIB_HEADERS) | \
		$(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror \
		-isystem $(BENCH_COMPARE_COMPETITORS)/mlib \
		$(BENCH_COMPARE_TYPED_TEST_SRC) $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/mlib_adapter.c -o $@

$(BENCH_COMPARE_STC_TEST): $(BENCH_COMPARE_TYPED_TEST_SRC) \
		$(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		bench/compare/adapters/stc_adapter.c \
		$(BENCH_COMPARE_STC_HEADERS) | \
		$(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror \
		-DSTC_TYPED_ADAPTER_TEST \
		-isystem $(BENCH_COMPARE_COMPETITORS)/stc/include \
		$(BENCH_COMPARE_TYPED_TEST_SRC) $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/stc_adapter.c -o $@

$(BENCH_COMPARE_STB_DS_TEST): $(BENCH_COMPARE_TYPED_TEST_SRC) \
		$(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		bench/compare/adapters/stb_ds_adapter.c \
		$(BENCH_COMPARE_STB_DS_HEADERS) | \
		$(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror \
		-isystem $(BENCH_COMPARE_COMPETITORS)/stb_ds \
		$(BENCH_COMPARE_TYPED_TEST_SRC) $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/stb_ds_adapter.c -o $@

$(BENCH_COMPARE_STB_DS_SEED_TEST): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		$(BENCH_COMPARE_STB_DS_SEED_TEST_SRC) \
		bench/compare/adapters/stb_ds_adapter.c \
		$(BENCH_COMPARE_STB_DS_HEADERS) | $(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror \
		-isystem $(BENCH_COMPARE_COMPETITORS)/stb_ds \
		$(BENCH_COMPARE_STB_DS_SEED_TEST_SRC) $(BENCH_ALLOC_SRC) -o $@

$(BENCH_COMPARE_MLIB_CLANG): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		bench/compare/adapters/mlib_adapter.c \
		$(BENCH_COMPARE_MLIB_HEADERS) | $(BENCH_COMPARE_BUILD)
	clang $(CFLAGS_COMMON) -O3 -DNDEBUG -Werror \
		-isystem $(BENCH_COMPARE_COMPETITORS)/mlib \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/mlib_adapter.c -o $@

$(BENCH_COMPARE_STC_CLANG): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		bench/compare/adapters/stc_adapter.c \
		$(BENCH_COMPARE_STC_HEADERS) | $(BENCH_COMPARE_BUILD)
	clang $(CFLAGS_COMMON) -O3 -DNDEBUG -Werror \
		-isystem $(BENCH_COMPARE_COMPETITORS)/stc/include \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/stc_adapter.c -o $@

$(BENCH_COMPARE_STB_DS_CLANG): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		bench/compare/adapters/stb_ds_adapter.c \
		$(BENCH_COMPARE_STB_DS_HEADERS) | $(BENCH_COMPARE_BUILD)
	clang $(CFLAGS_COMMON) -O3 -DNDEBUG -Werror \
		-isystem $(BENCH_COMPARE_COMPETITORS)/stb_ds \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/stb_ds_adapter.c -o $@

$(BENCH_COMPARE_MLIB_TEST_CLANG): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		$(BENCH_COMPARE_TYPED_TEST_SRC) \
		bench/compare/adapters/mlib_adapter.c \
		$(BENCH_COMPARE_MLIB_HEADERS) | $(BENCH_COMPARE_BUILD)
	clang $(CFLAGS_COMMON) -O1 -g -Werror \
		-isystem $(BENCH_COMPARE_COMPETITORS)/mlib \
		$(BENCH_COMPARE_TYPED_TEST_SRC) $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/mlib_adapter.c -o $@

$(BENCH_COMPARE_STC_TEST_CLANG): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		$(BENCH_COMPARE_TYPED_TEST_SRC) \
		bench/compare/adapters/stc_adapter.c \
		$(BENCH_COMPARE_STC_HEADERS) | $(BENCH_COMPARE_BUILD)
	clang $(CFLAGS_COMMON) -O1 -g -Werror -DSTC_TYPED_ADAPTER_TEST \
		-isystem $(BENCH_COMPARE_COMPETITORS)/stc/include \
		$(BENCH_COMPARE_TYPED_TEST_SRC) $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/stc_adapter.c -o $@

$(BENCH_COMPARE_STB_DS_TEST_CLANG): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		$(BENCH_COMPARE_TYPED_TEST_SRC) \
		bench/compare/adapters/stb_ds_adapter.c \
		$(BENCH_COMPARE_STB_DS_HEADERS) | $(BENCH_COMPARE_BUILD)
	clang $(CFLAGS_COMMON) -O1 -g -Werror \
		-isystem $(BENCH_COMPARE_COMPETITORS)/stb_ds \
		$(BENCH_COMPARE_TYPED_TEST_SRC) $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/stb_ds_adapter.c -o $@

$(BENCH_COMPARE_STB_DS_SEED_TEST_CLANG): \
		$(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		$(BENCH_COMPARE_STB_DS_SEED_TEST_SRC) \
		bench/compare/adapters/stb_ds_adapter.c \
		$(BENCH_COMPARE_STB_DS_HEADERS) | $(BENCH_COMPARE_BUILD)
	clang $(CFLAGS_COMMON) -O1 -g -Werror \
		-isystem $(BENCH_COMPARE_COMPETITORS)/stb_ds \
		$(BENCH_COMPARE_STB_DS_SEED_TEST_SRC) $(BENCH_ALLOC_SRC) -o $@

$(BENCH_COMPARE_UTHASH_CLANG): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		bench/compare/adapters/uthash_adapter.c \
		$(BENCH_COMPARE_UTHASH_HEADERS) | $(BENCH_COMPARE_BUILD)
	clang $(CFLAGS_COMMON) -O3 -DNDEBUG -Werror \
		-isystem $(BENCH_COMPARE_COMPETITORS)/uthash/src \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/uthash_adapter.c -o $@

$(BENCH_COMPARE_TIDWALL_OBJ_CLANG): $(BENCH_COMPARE_TIDWALL_SOURCES) | \
		$(BENCH_COMPARE_BUILD)
	clang -std=c11 -Wall -Wextra -Wpedantic -O3 -DNDEBUG -march=native \
		-isystem $(BENCH_COMPARE_COMPETITORS)/tidwall \
		-c $(BENCH_COMPARE_COMPETITORS)/tidwall/hashmap.c -o $@

$(BENCH_COMPARE_TIDWALL_CLANG): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		bench/compare/adapters/tidwall_adapter.c \
		$(BENCH_COMPARE_TIDWALL_SOURCES) \
		$(BENCH_COMPARE_TIDWALL_OBJ_CLANG) | $(BENCH_COMPARE_BUILD)
	clang $(CFLAGS_COMMON) -O3 -DNDEBUG -Werror \
		-isystem $(BENCH_COMPARE_COMPETITORS)/tidwall \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/tidwall_adapter.c \
		$(BENCH_COMPARE_TIDWALL_OBJ_CLANG) -o $@

$(BENCH_COMPARE_POTTERY_CLANG): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		bench/compare/adapters/pottery_adapter.c \
		$(BENCH_COMPARE_POTTERY_HEADERS) | $(BENCH_COMPARE_BUILD)
	clang $(CFLAGS_COMMON) -O3 -DNDEBUG -Werror \
		-isystem $(BENCH_COMPARE_COMPETITORS)/pottery/include \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/pottery_adapter.c -o $@

$(BENCH_COMPARE_UTHASH_TEST_CLANG): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		tests/test_uthash_adapter.c bench/compare/adapters/uthash_adapter.c \
		$(BENCH_COMPARE_UTHASH_HEADERS) | $(BENCH_COMPARE_BUILD)
	clang $(CFLAGS_COMMON) -O1 -g -Werror \
		-isystem $(BENCH_COMPARE_COMPETITORS)/uthash/src \
		tests/test_uthash_adapter.c $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/uthash_adapter.c -o $@

$(BENCH_COMPARE_TIDWALL_TEST_CLANG): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		tests/test_tidwall_adapter.c bench/compare/adapters/tidwall_adapter.c \
		$(BENCH_COMPARE_TIDWALL_SOURCES) \
		$(BENCH_COMPARE_TIDWALL_OBJ_CLANG) | $(BENCH_COMPARE_BUILD)
	clang $(CFLAGS_COMMON) -O1 -g -Werror \
		-DFHMAP_BENCH_ADAPTER_TESTING \
		-isystem $(BENCH_COMPARE_COMPETITORS)/tidwall \
		tests/test_tidwall_adapter.c $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/tidwall_adapter.c \
		$(BENCH_COMPARE_TIDWALL_OBJ_CLANG) \
		-Wl,--wrap=bench_malloc -o $@

$(BENCH_COMPARE_POTTERY_TEST_CLANG): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		tests/test_pottery_adapter.c bench/compare/adapters/pottery_adapter.c \
		$(BENCH_COMPARE_POTTERY_HEADERS) | $(BENCH_COMPARE_BUILD)
	clang $(CFLAGS_COMMON) -O1 -g -Werror \
		-DFHMAP_BENCH_ADAPTER_TESTING \
		-isystem $(BENCH_COMPARE_COMPETITORS)/pottery/include \
		tests/test_pottery_adapter.c $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/pottery_adapter.c \
		-Wl,--wrap=bench_malloc -Wl,--wrap=bench_calloc -o $@

$(BENCH_COMPARE_MLIB_SANITIZE): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		bench/compare/adapters/mlib_adapter.c \
		$(BENCH_COMPARE_MLIB_HEADERS) | $(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror -fno-omit-frame-pointer \
		-fsanitize=address,undefined \
		-isystem $(BENCH_COMPARE_COMPETITORS)/mlib \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/mlib_adapter.c -o $@

$(BENCH_COMPARE_STC_SANITIZE): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		bench/compare/adapters/stc_adapter.c \
		$(BENCH_COMPARE_STC_HEADERS) | $(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror -fno-omit-frame-pointer \
		-fsanitize=address,undefined \
		-isystem $(BENCH_COMPARE_COMPETITORS)/stc/include \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/stc_adapter.c -o $@

$(BENCH_COMPARE_STB_DS_SANITIZE): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		bench/compare/adapters/stb_ds_adapter.c \
		$(BENCH_COMPARE_STB_DS_HEADERS) | $(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror -fno-omit-frame-pointer \
		-fsanitize=address,undefined \
		-isystem $(BENCH_COMPARE_COMPETITORS)/stb_ds \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/stb_ds_adapter.c -o $@

$(BENCH_COMPARE_MLIB_TEST_SANITIZE): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		$(BENCH_COMPARE_TYPED_TEST_SRC) \
		bench/compare/adapters/mlib_adapter.c \
		$(BENCH_COMPARE_MLIB_HEADERS) | $(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror -fno-omit-frame-pointer \
		-fsanitize=address,undefined \
		-isystem $(BENCH_COMPARE_COMPETITORS)/mlib \
		$(BENCH_COMPARE_TYPED_TEST_SRC) $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/mlib_adapter.c -o $@

$(BENCH_COMPARE_STC_TEST_SANITIZE): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		$(BENCH_COMPARE_TYPED_TEST_SRC) \
		bench/compare/adapters/stc_adapter.c \
		$(BENCH_COMPARE_STC_HEADERS) | $(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror -fno-omit-frame-pointer \
		-fsanitize=address,undefined -DSTC_TYPED_ADAPTER_TEST \
		-isystem $(BENCH_COMPARE_COMPETITORS)/stc/include \
		$(BENCH_COMPARE_TYPED_TEST_SRC) $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/stc_adapter.c -o $@

$(BENCH_COMPARE_STB_DS_TEST_SANITIZE): \
		$(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		$(BENCH_COMPARE_TYPED_TEST_SRC) \
		bench/compare/adapters/stb_ds_adapter.c \
		$(BENCH_COMPARE_STB_DS_HEADERS) | $(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror -fno-omit-frame-pointer \
		-fsanitize=address,undefined \
		-isystem $(BENCH_COMPARE_COMPETITORS)/stb_ds \
		$(BENCH_COMPARE_TYPED_TEST_SRC) $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/stb_ds_adapter.c -o $@

$(BENCH_COMPARE_STB_DS_SEED_TEST_SANITIZE): \
		$(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		$(BENCH_COMPARE_STB_DS_SEED_TEST_SRC) \
		bench/compare/adapters/stb_ds_adapter.c \
		$(BENCH_COMPARE_STB_DS_HEADERS) | $(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror -fno-omit-frame-pointer \
		-fsanitize=address,undefined \
		-isystem $(BENCH_COMPARE_COMPETITORS)/stb_ds \
		$(BENCH_COMPARE_STB_DS_SEED_TEST_SRC) $(BENCH_ALLOC_SRC) -o $@

$(BENCH_COMPARE_UTHASH_SANITIZE): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		bench/compare/adapters/uthash_adapter.c \
		$(BENCH_COMPARE_UTHASH_HEADERS) | $(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror -fno-omit-frame-pointer \
		-fsanitize=address,undefined \
		-isystem $(BENCH_COMPARE_COMPETITORS)/uthash/src \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/uthash_adapter.c -o $@

$(BENCH_COMPARE_TIDWALL_OBJ_SANITIZE): $(BENCH_COMPARE_TIDWALL_SOURCES) | \
		$(BENCH_COMPARE_BUILD)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -O1 -g \
		-fno-omit-frame-pointer -fsanitize=address,undefined \
		-isystem $(BENCH_COMPARE_COMPETITORS)/tidwall \
		-c $(BENCH_COMPARE_COMPETITORS)/tidwall/hashmap.c -o $@

$(BENCH_COMPARE_TIDWALL_SANITIZE): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		bench/compare/adapters/tidwall_adapter.c \
		$(BENCH_COMPARE_TIDWALL_SOURCES) \
		$(BENCH_COMPARE_TIDWALL_OBJ_SANITIZE) | $(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror -fno-omit-frame-pointer \
		-fsanitize=address,undefined \
		-isystem $(BENCH_COMPARE_COMPETITORS)/tidwall \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/tidwall_adapter.c \
		$(BENCH_COMPARE_TIDWALL_OBJ_SANITIZE) -o $@

$(BENCH_COMPARE_POTTERY_SANITIZE): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		bench/compare/adapters/pottery_adapter.c \
		$(BENCH_COMPARE_POTTERY_HEADERS) | $(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror -fno-omit-frame-pointer \
		-fsanitize=address,undefined \
		-isystem $(BENCH_COMPARE_COMPETITORS)/pottery/include \
		$(BENCH_COMPARE_DRIVER_SRC) $(BENCH_COMMON_SRC) \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/pottery_adapter.c -o $@

$(BENCH_COMPARE_UTHASH_TEST_SANITIZE): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		tests/test_uthash_adapter.c bench/compare/adapters/uthash_adapter.c \
		$(BENCH_COMPARE_UTHASH_HEADERS) | $(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror -fno-omit-frame-pointer \
		-fsanitize=address,undefined \
		-isystem $(BENCH_COMPARE_COMPETITORS)/uthash/src \
		tests/test_uthash_adapter.c $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/uthash_adapter.c -o $@

$(BENCH_COMPARE_TIDWALL_TEST_SANITIZE): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		tests/test_tidwall_adapter.c bench/compare/adapters/tidwall_adapter.c \
		$(BENCH_COMPARE_TIDWALL_SOURCES) \
		$(BENCH_COMPARE_TIDWALL_OBJ_SANITIZE) | $(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror -fno-omit-frame-pointer \
		-fsanitize=address,undefined \
		-DFHMAP_BENCH_ADAPTER_TESTING \
		-isystem $(BENCH_COMPARE_COMPETITORS)/tidwall \
		tests/test_tidwall_adapter.c $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/tidwall_adapter.c \
		$(BENCH_COMPARE_TIDWALL_OBJ_SANITIZE) \
		-Wl,--wrap=bench_malloc -o $@

$(BENCH_COMPARE_POTTERY_TEST_SANITIZE): $(BENCH_COMPARE_TYPED_COMMON_DEPS) \
		tests/test_pottery_adapter.c bench/compare/adapters/pottery_adapter.c \
		$(BENCH_COMPARE_POTTERY_HEADERS) | $(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror -fno-omit-frame-pointer \
		-fsanitize=address,undefined \
		-DFHMAP_BENCH_ADAPTER_TESTING \
		-isystem $(BENCH_COMPARE_COMPETITORS)/pottery/include \
		tests/test_pottery_adapter.c $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/pottery_adapter.c \
		-Wl,--wrap=bench_malloc -Wl,--wrap=bench_calloc -o $@

$(BENCH_COMPARE_KHASHL_FAULT_U64): tests/test_khashl_init_fault.c \
		$(BENCH_COMPARE_DRIVER_SRC) \
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
		$(BENCH_COMPARE_DRIVER_SRC) \
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

$(BENCH_COMPARE_KHASH_RESERVE_TEST): tests/test_khash_reserve.c \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/khash_adapter.c \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_alloc.h \
		$(BENCH_COMPARE_COMPETITORS)/khash/khash.h | \
		$(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror \
		-isystem $(BENCH_COMPARE_COMPETITORS)/khash \
		tests/test_khash_reserve.c $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/khash_adapter.c \
		-Wl,--wrap=bench_malloc -o $@

$(BENCH_COMPARE_KHASHL_RESERVE_TEST): tests/test_khashl_reserve.c \
		$(BENCH_ALLOC_SRC) bench/compare/adapters/khashl_adapter.c \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_alloc.h \
		$(BENCH_COMPARE_COMPETITORS)/khashl/khashl.h | \
		$(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O1 -g -Werror \
		-isystem $(BENCH_COMPARE_COMPETITORS)/khashl \
		tests/test_khashl_reserve.c $(BENCH_ALLOC_SRC) \
		bench/compare/adapters/khashl_adapter.c \
		-Wl,--wrap=bench_malloc -Wl,--wrap=bench_realloc -o $@

$(BENCH_COMPARE_PERMUTED): $(BENCH_COMPARE_SRC) \
		bench/compare/common/bench_adapter.h \
		bench/compare/common/bench_common.h \
		bench/compare/common/bench_alloc.h fhmap.h | $(BENCH_COMPARE_BUILD)
	$(CC) $(CFLAGS_COMMON) -O3 -DNDEBUG \
		-DFHMAP_BENCH_TEST_PERMUTE_LOOKUP_HITS \
		$(BENCH_COMPARE_SRC) -o $@

bench-compare-lock: $(BENCH_COMPARE_FETCHER) $(BENCH_COMPARE_LOCK)
	sh $(BENCH_COMPARE_FETCHER) --check-lock

bench-compare-fetch-force:

$(BENCH_COMPARE_FETCH_STAMP): $(BENCH_COMPARE_FETCHER) \
		$(BENCH_COMPARE_LOCK) bench-compare-fetch-force | $(BUILD)
	sh $(BENCH_COMPARE_FETCHER) --check-lock
	$(BENCH_COMPARE_FETCHER) --dest $(BENCH_COMPARE_COMPETITORS)
	touch $@

$(BENCH_COMPARE_MISSING_FILES): | $(BENCH_COMPARE_FETCH_STAMP)
	@test -f "$@" || { \
		printf 'missing fetched competitor file: %s\n' "$@" >&2; \
		exit 1; \
	}

$(BENCH_COMPARE_BINS) $(BENCH_COMPARE_TEST_BINS): | \
		$(BENCH_COMPARE_FETCH_STAMP)

bench-compare-fetch: $(BENCH_COMPARE_FETCH_STAMP)

bench-compare-build: bench-compare-fetch $(BENCH_COMPARE_BINS)

bench-compare-smoke: bench-compare-build $(BENCH_COMPARE_TEST_BINS)
	./tests/test_bench_compare.sh

bench-compare: bench-compare-build
	./bench/compare/run-comparison.sh

$(BUILD)/bench_fhmap: $(BENCH_SRC) fhmap.h | $(BUILD)
	$(CC) $(CFLAGS_COMMON) -O3 -DNDEBUG -march=native $(BENCH_SRC) -o $@

bench: $(BUILD)/bench_fhmap
	./$(BUILD)/bench_fhmap

clean:
	rm -rf $(BUILD)
