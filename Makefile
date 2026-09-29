CC ?= cc
GO ?= go
CFLAGS_COMMON = -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
CFLAGS_DEBUG = -O0 -g -DDEBUG -DPERSISTENT $(CFLAGS_COMMON)
CFLAGS_DEBUG_NONPERSISTENT = -O0 -g -DDEBUG $(CFLAGS_COMMON)
CFLAGS_RELEASE = -O2 -march=native $(CFLAGS_COMMON)
CFLAGS_BENCHMARK ?= -O3 -march=native -DNDEBUG -DBST_FAST_BENCHMARK -DBST_DISABLE_INVARIANTS $(CFLAGS_COMMON) -Wno-unused-variable -Wno-unused-but-set-variable
CFLAGS_MEASUREMENTS ?= -O3 -march=native $(CFLAGS_COMMON)
C_HEADERS = $(wildcard c/*.h)
CLANG_FORMAT ?= clang-format
FORMAT_FILES = $(sort $(wildcard c/*.c c/*.h))
GOFMT ?= gofmt
GO_FILES = $(sort $(wildcard *.go))
TREE_CORE = c/bst.c c/tree.c c/tree_arena.c c/tree_persistence.c c/tree_measurements.c c/tree_verify.c \
            c/random.c c/distributions.c \
            c/weight.c \
            c/splay.c c/randomized.c c/zip.c \
            c/treap_bottomup.c c/treap_topdown.c c/treap_fingertree.c \
            c/conc.c c/llrb.c c/aa.c \
            c/avl_bottomup.c c/avl_topdown.c \
            c/avl_weak_bottomup.c c/avl_weak_topdown.c \
            c/avl_relaxed_bottomup.c c/avl_relaxed_topdown.c \
            c/redblack_bottomup.c c/redblack_topdown.c \
            c/redblack_relaxed_bottomup.c c/redblack_relaxed_topdown.c \
            c/wbst_bottomup.c c/wbst_topdown.c c/wbst_relaxed.c \
            c/lbst_bottomup.c c/lbst_topdown.c c/lbst_relaxed.c
TREE_REGISTRY = c/tree_registry.c
BALANCER_CORE = c/balancers.c
BALANCER_REGISTRY = c/balancer_registry.c
BALANCER_RUNNER = c/balancer_runs.c
CLI_SUPPORT = c/cli.c
OPERATION_CORE = c/operations.c
OPERATION_RUNNER = $(OPERATION_CORE) c/operation_runs.c
REFERENCE_SUPPORT = c/reference_list.c
TEST_SUPPORT = $(REFERENCE_SUPPORT) $(OPERATION_RUNNER)
SAMPLING_HEADER = c/sampling.h
TIME_UTILS_HEADER = c/time_utils.h
BENCHMARK_INDEX_SUPPORT = c/benchmark_index.c
TEST_ARGS ?=
COMPARE_ARGS ?=
COMPARE_OUTPUT ?= c/compare.txt

BALANCERS := Median Height Weight Log Cost DSW
SMOKE_OUTPUT ?= /private/tmp/bst-benchmark-smoke
SMOKE_SCALE ?= 128
SMOKE_STEPS ?= 256
SMOKE_SAMPLES ?= 4
SMOKE_ITERATIONS ?= 1
SMOKE_DURATION_MS ?= 1
SMOKE_ARENA ?= 100000
SMOKE_TREE ?= Splay
SMOKE_BALANCER ?= Median
SMOKE_DISTRIBUTION ?= Uniform
BENCHMARK_JOBS ?= $(shell getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 1)

.PHONY: \
	sh format format-check test test-debug test-nonpersistent test-debug-nonpersistent \
	bench bench-persistent compare measurements measurements-persistent quickbench quickbench-bin \
	operation-benchmarks operation-non-persistent-benchmarks operation-persistent-benchmarks \
	operation-measurements operation-non-persistent-measurements operation-persistent-measurements \
	balancers-bench balancer-benchmarks balancers-measure balancer-measurements \
	benchmark-smoke animation animate benchmark-index-tool \
	figures balancer-plots operation-plots optimize polytope delete \
	benchmark-index article-index article-index-check article-index-test \
	math index docs prepare publish

format: .clang-format
	$(CLANG_FORMAT) -i $(FORMAT_FILES)
	$(GOFMT) -w $(GO_FILES)

format-check: .clang-format
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_FILES)
	@test -z "$$($(GOFMT) -l $(GO_FILES))"

test: $(C_HEADERS) $(TREE_CORE) $(TREE_REGISTRY) $(BALANCER_CORE) $(BALANCER_REGISTRY) $(BALANCER_RUNNER) $(CLI_SUPPORT) $(TEST_SUPPORT) $(BENCHMARK_INDEX_SUPPORT) c/test.c
	$(CC) $(CFLAGS_RELEASE) -DPERSISTENT -DBENCHMARK_INDEX_NO_MAIN -o c/test_bin $(TREE_CORE) $(TREE_REGISTRY) $(BALANCER_CORE) $(BALANCER_REGISTRY) $(BALANCER_RUNNER) $(CLI_SUPPORT) $(TEST_SUPPORT) $(BENCHMARK_INDEX_SUPPORT) c/test.c -lm
	./c/test_bin $(TEST_ARGS)

test-debug: $(C_HEADERS) $(TREE_CORE) $(TREE_REGISTRY) $(BALANCER_CORE) $(BALANCER_REGISTRY) $(BALANCER_RUNNER) $(CLI_SUPPORT) $(TEST_SUPPORT) $(BENCHMARK_INDEX_SUPPORT) c/test.c
	$(CC) $(CFLAGS_DEBUG) -DBENCHMARK_INDEX_NO_MAIN -o c/test_bin $(TREE_CORE) $(TREE_REGISTRY) $(BALANCER_CORE) $(BALANCER_REGISTRY) $(BALANCER_RUNNER) $(CLI_SUPPORT) $(TEST_SUPPORT) $(BENCHMARK_INDEX_SUPPORT) c/test.c -lm
	./c/test_bin $(TEST_ARGS)

test-nonpersistent: $(C_HEADERS) $(TREE_CORE) $(TREE_REGISTRY) $(BALANCER_CORE) $(BALANCER_REGISTRY) $(BALANCER_RUNNER) $(CLI_SUPPORT) $(TEST_SUPPORT) $(BENCHMARK_INDEX_SUPPORT) c/test.c
	$(CC) $(CFLAGS_RELEASE) -DBENCHMARK_INDEX_NO_MAIN -o c/test_nonpersistent_bin $(TREE_CORE) $(TREE_REGISTRY) $(BALANCER_CORE) $(BALANCER_REGISTRY) $(BALANCER_RUNNER) $(CLI_SUPPORT) $(TEST_SUPPORT) $(BENCHMARK_INDEX_SUPPORT) c/test.c -lm
	./c/test_nonpersistent_bin $(TEST_ARGS)

test-debug-nonpersistent: $(C_HEADERS) $(TREE_CORE) $(TREE_REGISTRY) $(BALANCER_CORE) $(BALANCER_REGISTRY) $(BALANCER_RUNNER) $(CLI_SUPPORT) $(TEST_SUPPORT) $(BENCHMARK_INDEX_SUPPORT) c/test.c
	$(CC) $(CFLAGS_DEBUG_NONPERSISTENT) -DBENCHMARK_INDEX_NO_MAIN -o c/test_debug_nonpersistent_bin $(TREE_CORE) $(TREE_REGISTRY) $(BALANCER_CORE) $(BALANCER_REGISTRY) $(BALANCER_RUNNER) $(CLI_SUPPORT) $(TEST_SUPPORT) $(BENCHMARK_INDEX_SUPPORT) c/test.c -lm
	./c/test_debug_nonpersistent_bin $(TEST_ARGS)

bench: $(C_HEADERS) $(TREE_CORE) $(TREE_REGISTRY) $(OPERATION_RUNNER) $(CLI_SUPPORT) $(SAMPLING_HEADER) $(TIME_UTILS_HEADER) c/bench.c
	$(CC) $(CFLAGS_BENCHMARK) -o c/bench_bin $(TREE_CORE) $(TREE_REGISTRY) $(OPERATION_RUNNER) $(CLI_SUPPORT) c/bench.c -lm

bench-persistent: $(C_HEADERS) $(TREE_CORE) $(TREE_REGISTRY) $(OPERATION_RUNNER) $(CLI_SUPPORT) $(SAMPLING_HEADER) $(TIME_UTILS_HEADER) c/bench.c
	$(CC) $(CFLAGS_BENCHMARK) -DPERSISTENT -o c/bench_persistent_bin $(TREE_CORE) $(TREE_REGISTRY) $(OPERATION_RUNNER) $(CLI_SUPPORT) c/bench.c -lm

operation-benchmarks-Insert: bench
	./c/bench_bin -operation Insert

operation-benchmarks-InsertDelete: bench
	./c/bench_bin -operation InsertDelete

operation-benchmarks-InsertPersistent: bench-persistent
	./c/bench_persistent_bin -operation InsertPersistent

operation-benchmarks-InsertDeletePersistent: bench-persistent
	./c/bench_persistent_bin -operation InsertDeletePersistent

operation-non-persistent-benchmarks: \
	operation-benchmarks-Insert \
	operation-benchmarks-InsertDelete

operation-persistent-benchmarks: \
	operation-benchmarks-InsertPersistent \
	operation-benchmarks-InsertDeletePersistent

operation-benchmarks: \
	operation-non-persistent-benchmarks \
	operation-persistent-benchmarks

compare: $(C_HEADERS) $(TREE_CORE) $(TREE_REGISTRY) $(CLI_SUPPORT) $(REFERENCE_SUPPORT) c/compare.c
	$(CC) $(CFLAGS_RELEASE) -DPERSISTENT -o c/compare_bin $(TREE_CORE) $(TREE_REGISTRY) $(CLI_SUPPORT) $(REFERENCE_SUPPORT) c/compare.c -lm
	./c/compare_bin $(COMPARE_ARGS) > $(COMPARE_OUTPUT)
	@echo "Comparison output written to $(COMPARE_OUTPUT)"

measurements: $(C_HEADERS) $(TREE_CORE) $(TREE_REGISTRY) $(OPERATION_RUNNER) $(CLI_SUPPORT) $(SAMPLING_HEADER) $(TIME_UTILS_HEADER) c/measurements.c
	$(CC) $(CFLAGS_MEASUREMENTS) -DMEASUREMENTS -o c/measurements_bin $(TREE_CORE) $(TREE_REGISTRY) $(OPERATION_RUNNER) $(CLI_SUPPORT) c/measurements.c -lm

measurements-persistent: $(C_HEADERS) $(TREE_CORE) $(TREE_REGISTRY) $(OPERATION_RUNNER) $(CLI_SUPPORT) $(SAMPLING_HEADER) $(TIME_UTILS_HEADER) c/measurements.c
	$(CC) $(CFLAGS_MEASUREMENTS) -DPERSISTENT -DMEASUREMENTS -o c/measurements_persistent_bin $(TREE_CORE) $(TREE_REGISTRY) $(OPERATION_RUNNER) $(CLI_SUPPORT) c/measurements.c -lm

operation-measurements-InsertPersistent: measurements-persistent
	./c/measurements_persistent_bin -operation InsertPersistent

operation-measurements-InsertDeletePersistent: measurements-persistent
	./c/measurements_persistent_bin -operation InsertDeletePersistent

operation-measurements-Insert: measurements
	./c/measurements_bin -operation Insert

operation-measurements-InsertDelete: measurements
	./c/measurements_bin -operation InsertDelete

operation-non-persistent-measurements: \
	operation-measurements-Insert \
	operation-measurements-InsertDelete

operation-persistent-measurements: \
	operation-measurements-InsertPersistent \
	operation-measurements-InsertDeletePersistent

operation-measurements: \
	operation-non-persistent-measurements \
	operation-persistent-measurements

quickbench-bin: $(C_HEADERS) $(TREE_CORE) $(TREE_REGISTRY) $(OPERATION_RUNNER) $(CLI_SUPPORT) $(TIME_UTILS_HEADER) c/quickbench.c
	$(CC) $(CFLAGS_BENCHMARK) -o c/quickbench_bin $(TREE_CORE) $(TREE_REGISTRY) $(OPERATION_RUNNER) $(CLI_SUPPORT) c/quickbench.c -lm

quickbench: quickbench-bin
	./c/quickbench_bin

balancers-bench: $(C_HEADERS) $(TREE_CORE) $(BALANCER_CORE) $(BALANCER_REGISTRY) $(BALANCER_RUNNER) $(CLI_SUPPORT) $(SAMPLING_HEADER) $(TIME_UTILS_HEADER) c/balancers_bench.c
	$(CC) $(CFLAGS_BENCHMARK) -o c/balancers_bench_bin $(TREE_CORE) $(BALANCER_CORE) $(BALANCER_REGISTRY) $(BALANCER_RUNNER) $(CLI_SUPPORT) c/balancers_bench.c -lm

balancer-benchmarks: balancers-bench
	./c/balancers_bench_bin

balancers-measure: $(C_HEADERS) $(TREE_CORE) $(BALANCER_CORE) $(BALANCER_REGISTRY) $(BALANCER_RUNNER) $(CLI_SUPPORT) $(SAMPLING_HEADER) $(TIME_UTILS_HEADER) c/balancers_measure.c
	$(CC) $(CFLAGS_MEASUREMENTS) -DMEASUREMENTS -o c/balancers_measure_bin $(TREE_CORE) $(BALANCER_CORE) $(BALANCER_REGISTRY) $(BALANCER_RUNNER) $(CLI_SUPPORT) c/balancers_measure.c -lm

balancer-measurements-%: balancers-measure
	./c/balancers_measure_bin -strategy $*

balancer-measurements: balancers-measure
	$(MAKE) -j $(foreach balancer,$(BALANCERS),balancer-measurements-$(balancer))

benchmark-smoke:
	$(MAKE) -j$(BENCHMARK_JOBS) bench bench-persistent measurements measurements-persistent balancers-bench balancers-measure quickbench-bin
	./c/bench_bin -operation Insert -tree $(SMOKE_TREE) -dist $(SMOKE_DISTRIBUTION) -scale $(SMOKE_SCALE) -steps $(SMOKE_STEPS) -samples $(SMOKE_SAMPLES) -iterations $(SMOKE_ITERATIONS) -arena $(SMOKE_ARENA) -output-dir $(SMOKE_OUTPUT)
	./c/bench_bin -operation InsertDelete -tree $(SMOKE_TREE) -dist $(SMOKE_DISTRIBUTION) -scale $(SMOKE_SCALE) -steps $(SMOKE_STEPS) -samples $(SMOKE_SAMPLES) -iterations $(SMOKE_ITERATIONS) -arena $(SMOKE_ARENA) -output-dir $(SMOKE_OUTPUT)
	./c/bench_persistent_bin -operation InsertPersistent -tree $(SMOKE_TREE) -dist $(SMOKE_DISTRIBUTION) -scale $(SMOKE_SCALE) -steps $(SMOKE_STEPS) -samples $(SMOKE_SAMPLES) -iterations $(SMOKE_ITERATIONS) -arena $(SMOKE_ARENA) -output-dir $(SMOKE_OUTPUT)
	./c/bench_persistent_bin -operation InsertDeletePersistent -tree $(SMOKE_TREE) -dist $(SMOKE_DISTRIBUTION) -scale $(SMOKE_SCALE) -steps $(SMOKE_STEPS) -samples $(SMOKE_SAMPLES) -iterations $(SMOKE_ITERATIONS) -arena $(SMOKE_ARENA) -output-dir $(SMOKE_OUTPUT)
	./c/measurements_bin -operation Insert -tree $(SMOKE_TREE) -dist $(SMOKE_DISTRIBUTION) -scale $(SMOKE_SCALE) -steps $(SMOKE_STEPS) -samples $(SMOKE_SAMPLES) -arena $(SMOKE_ARENA) -output-dir $(SMOKE_OUTPUT)
	./c/measurements_bin -operation InsertDelete -tree $(SMOKE_TREE) -dist $(SMOKE_DISTRIBUTION) -scale $(SMOKE_SCALE) -steps $(SMOKE_STEPS) -samples $(SMOKE_SAMPLES) -arena $(SMOKE_ARENA) -output-dir $(SMOKE_OUTPUT)
	./c/measurements_persistent_bin -operation InsertPersistent -tree $(SMOKE_TREE) -dist $(SMOKE_DISTRIBUTION) -scale $(SMOKE_SCALE) -steps $(SMOKE_STEPS) -samples $(SMOKE_SAMPLES) -arena $(SMOKE_ARENA) -output-dir $(SMOKE_OUTPUT)
	./c/measurements_persistent_bin -operation InsertDeletePersistent -tree $(SMOKE_TREE) -dist $(SMOKE_DISTRIBUTION) -scale $(SMOKE_SCALE) -steps $(SMOKE_STEPS) -samples $(SMOKE_SAMPLES) -arena $(SMOKE_ARENA) -output-dir $(SMOKE_OUTPUT)
	./c/balancers_bench_bin -balancer $(SMOKE_BALANCER) -dist $(SMOKE_DISTRIBUTION) -scale $(SMOKE_SCALE) -samples $(SMOKE_SAMPLES) -duration-ms $(SMOKE_DURATION_MS) -output-dir $(SMOKE_OUTPUT)
	./c/balancers_measure_bin -balancer $(SMOKE_BALANCER) -dist $(SMOKE_DISTRIBUTION) -scale $(SMOKE_SCALE) -samples $(SMOKE_SAMPLES) -iterations $(SMOKE_ITERATIONS) -output-dir $(SMOKE_OUTPUT)
	./c/quickbench_bin -tree $(SMOKE_TREE) -dist $(SMOKE_DISTRIBUTION) -scale $(SMOKE_SCALE) -steps $(SMOKE_STEPS) -arena $(SMOKE_ARENA)

animate: $(C_HEADERS) $(TREE_CORE) $(TREE_REGISTRY) $(OPERATION_CORE) $(CLI_SUPPORT) c/animate.c
	$(CC) $(CFLAGS_RELEASE) -DPERSISTENT -o c/animate_bin $(TREE_CORE) $(TREE_REGISTRY) $(OPERATION_CORE) $(CLI_SUPPORT) c/animate.c -lm
	./c/animate_bin

animation: animate

benchmark-index-tool: $(C_HEADERS) c/benchmark_index.c
	$(CC) $(CFLAGS_RELEASE) -o c/benchmark_index_bin c/benchmark_index.c

sh:
	docker compose run --interactive --tty --rm sh

##
# FIGURES
#

define figure
    gnuplot "docs/plots/figures/$(1).gnuplot" > "docs/plots/figures/$(1).svg"
endef

figures: delete polytope
	$(call figure,a)
	$(call figure,b)
	$(call figure,c)
	$(call figure,d)
	$(call figure,e)
	$(call figure,f)
	$(call figure,x)
	$(call figure,leaf)
	$(call figure,tree)
	$(call figure,split_join)
	$(call figure,finger_tree)
	$(call figure,rotations)
	$(call figure,linked_list)
	$(call figure,linked_list_median)
	$(call figure,partition)
	$(call figure,binary_search_tree)
	$(call figure,binary_search_tree_large)
	$(call figure,hibbard)
	$(call figure,balance)
	$(call figure,scapegoat)
	$(call figure,persistence)
	$(call figure,parent_pointers)
	$(call figure,concurrency)
	$(call figure,median_balance)
	$(call figure,perfect_trees)
	$(call figure,redblack)
	$(call figure,array)
	$(call figure,insert_leaf)
	$(call figure,log_balanced)
	$(call figure,mlogm)

balancer-plots:
	gnuplot docs/plots/benchmarks/balancers/balancers.gnuplot

operation-plots:
	gnuplot docs/plots/benchmarks/operations/operations.gnuplot

optimize:
	npx --yes svgo --config docs/svgo.config.js --recursive --folder docs

polytope:
	gnuplot docs/plots/figures/polytope/polytope.gnuplot > "docs/plots/figures/polytope/polytope.svg"

delete:
	gnuplot docs/plots/figures/delete/delete.gnuplot > "docs/plots/figures/delete/delete.svg"

benchmark-index: benchmark-index-tool
	./c/benchmark_index_bin

article-index: index.go index.template.html
	$(GO) run index.go

article-index-check: index.go index.template.html index.html
	$(GO) run index.go -check

article-index-test: index.go index_test.go
	$(GO) test index.go index_test.go

math:
	sh docs/katex/render.sh

index: benchmark-index article-index

docs: figures math balancer-plots operation-plots
	$(MAKE) optimize
	$(MAKE) index

prepare: docs

publish: prepare
	git add .
	git commit -m "Publishing commit"
	git push
