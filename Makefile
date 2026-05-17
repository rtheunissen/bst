CC ?= cc
CFLAGS_COMMON = -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
CFLAGS_DEBUG = -O0 -g -DDEBUG -DPERSISTENT $(CFLAGS_COMMON)
CFLAGS_RELEASE = -O2 -march=native $(CFLAGS_COMMON)
CORE = c/tree.c c/random.c c/distributions.c \
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

BALANCERS := Median Height Weight Log Cost DSW
SMOKE_OUTPUT ?= /private/tmp/bst-benchmark-smoke
SMOKE_SCALE ?= 128
SMOKE_STEPS ?= 256
SMOKE_SAMPLES ?= 4
SMOKE_ITERATIONS ?= 1
SMOKE_DURATION_MS ?= 1
SMOKE_ARENA ?= 100000

.PHONY: \
	sh test test-debug bench bench-persistent compare measurements measurements-persistent quickbench quickbench-bin \
	operation-benchmarks operation-non-persistent-benchmarks operation-persistent-benchmarks \
	operation-measurements operation-non-persistent-measurements operation-persistent-measurements \
	balancers-bench balancer-benchmarks balancers-measure balancer-measurements \
	benchmark-smoke animation animate benchmark-index-tool \
	figures balancer-plots operation-plots optimize polytope delete \
	benchmark-index article-index math index docs prepare publish

test: $(CORE) c/balancers.c c/test.c
	$(CC) $(CFLAGS_RELEASE) -DPERSISTENT -o c/test_bin $(CORE) c/balancers.c c/test.c -lm
	./c/test_bin

test-debug: $(CORE) c/balancers.c c/test.c
	$(CC) $(CFLAGS_DEBUG) -o c/test_bin $(CORE) c/balancers.c c/test.c -lm
	./c/test_bin

bench: $(CORE) c/bench.c
	$(CC) $(CFLAGS_RELEASE) -o c/bench_bin $(CORE) c/bench.c -lm

bench-persistent: $(CORE) c/bench.c
	$(CC) $(CFLAGS_RELEASE) -DPERSISTENT -o c/bench_bin $(CORE) c/bench.c -lm

operation-benchmarks-Insert: bench
	./c/bench_bin -operation Insert

operation-benchmarks-InsertDelete: bench
	./c/bench_bin -operation InsertDelete

operation-benchmarks-InsertPersistent: bench-persistent
	./c/bench_bin -operation InsertPersistent

operation-benchmarks-InsertDeletePersistent: bench-persistent
	./c/bench_bin -operation InsertDeletePersistent

operation-non-persistent-benchmarks: \
	operation-benchmarks-Insert \
	operation-benchmarks-InsertDelete

operation-persistent-benchmarks: \
	operation-benchmarks-InsertPersistent \
	operation-benchmarks-InsertDeletePersistent

operation-benchmarks: \
	operation-non-persistent-benchmarks \
	operation-persistent-benchmarks

compare: $(CORE) c/compare.c
	$(CC) $(CFLAGS_RELEASE) -DPERSISTENT -o c/compare_bin $(CORE) c/compare.c -lm
	./c/compare_bin > c/compare.txt
	@echo "Comparison output written to c/compare.txt"

measurements: $(CORE) c/measurements.c
	$(CC) $(CFLAGS_RELEASE) -DMEASUREMENTS -o c/measurements_bin $(CORE) c/measurements.c -lm

measurements-persistent: $(CORE) c/measurements.c
	$(CC) $(CFLAGS_RELEASE) -DPERSISTENT -DMEASUREMENTS -o c/measurements_persistent_bin $(CORE) c/measurements.c -lm

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

quickbench-bin: $(CORE) c/quickbench.c
	$(CC) $(CFLAGS_RELEASE) -o c/quickbench_bin $(CORE) c/quickbench.c -lm

quickbench: quickbench-bin
	./c/quickbench_bin

balancers-bench: $(CORE) c/balancers.c c/balancers_bench.c
	$(CC) $(CFLAGS_RELEASE) -o c/balancers_bench_bin $(CORE) c/balancers.c c/balancers_bench.c -lm

balancer-benchmarks: balancers-bench
	./c/balancers_bench_bin

balancers-measure: $(CORE) c/balancers.c c/balancers_measure.c
	$(CC) $(CFLAGS_RELEASE) -DMEASUREMENTS -o c/balancers_measure_bin $(CORE) c/balancers.c c/balancers_measure.c -lm

balancer-measurements-%: balancers-measure
	./c/balancers_measure_bin -strategy $*

balancer-measurements: balancers-measure
	$(MAKE) -j $(foreach balancer,$(BALANCERS),balancer-measurements-$(balancer))

benchmark-smoke:
	$(MAKE) bench
	./c/bench_bin -operation Insert -scale $(SMOKE_SCALE) -steps $(SMOKE_STEPS) -samples $(SMOKE_SAMPLES) -iterations $(SMOKE_ITERATIONS) -arena $(SMOKE_ARENA) -output-dir $(SMOKE_OUTPUT)
	./c/bench_bin -operation InsertDelete -scale $(SMOKE_SCALE) -steps $(SMOKE_STEPS) -samples $(SMOKE_SAMPLES) -iterations $(SMOKE_ITERATIONS) -arena $(SMOKE_ARENA) -output-dir $(SMOKE_OUTPUT)
	$(MAKE) bench-persistent
	./c/bench_bin -operation InsertPersistent -scale $(SMOKE_SCALE) -steps $(SMOKE_STEPS) -samples $(SMOKE_SAMPLES) -iterations $(SMOKE_ITERATIONS) -arena $(SMOKE_ARENA) -output-dir $(SMOKE_OUTPUT)
	./c/bench_bin -operation InsertDeletePersistent -scale $(SMOKE_SCALE) -steps $(SMOKE_STEPS) -samples $(SMOKE_SAMPLES) -iterations $(SMOKE_ITERATIONS) -arena $(SMOKE_ARENA) -output-dir $(SMOKE_OUTPUT)
	$(MAKE) measurements
	./c/measurements_bin -operation Insert -scale $(SMOKE_SCALE) -steps $(SMOKE_STEPS) -samples $(SMOKE_SAMPLES) -arena $(SMOKE_ARENA) -output-dir $(SMOKE_OUTPUT)
	./c/measurements_bin -operation InsertDelete -scale $(SMOKE_SCALE) -steps $(SMOKE_STEPS) -samples $(SMOKE_SAMPLES) -arena $(SMOKE_ARENA) -output-dir $(SMOKE_OUTPUT)
	$(MAKE) measurements-persistent
	./c/measurements_persistent_bin -operation InsertPersistent -scale $(SMOKE_SCALE) -steps $(SMOKE_STEPS) -samples $(SMOKE_SAMPLES) -arena $(SMOKE_ARENA) -output-dir $(SMOKE_OUTPUT)
	./c/measurements_persistent_bin -operation InsertDeletePersistent -scale $(SMOKE_SCALE) -steps $(SMOKE_STEPS) -samples $(SMOKE_SAMPLES) -arena $(SMOKE_ARENA) -output-dir $(SMOKE_OUTPUT)
	$(MAKE) balancers-bench
	./c/balancers_bench_bin -scale $(SMOKE_SCALE) -samples $(SMOKE_SAMPLES) -duration-ms $(SMOKE_DURATION_MS) -output-dir $(SMOKE_OUTPUT)
	$(MAKE) balancers-measure
	./c/balancers_measure_bin -scale $(SMOKE_SCALE) -samples $(SMOKE_SAMPLES) -iterations $(SMOKE_ITERATIONS) -output-dir $(SMOKE_OUTPUT)
	$(MAKE) quickbench-bin
	./c/quickbench_bin -scale $(SMOKE_SCALE) -steps $(SMOKE_STEPS) -arena $(SMOKE_ARENA)

animate: $(CORE) c/animate.c
	$(CC) $(CFLAGS_RELEASE) -DPERSISTENT -o c/animate_bin $(CORE) c/animate.c -lm
	./c/animate_bin

animation: animate

benchmark-index-tool: c/benchmark_index.c
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

article-index:
	@test -f index.html
	@echo "index.html is checked in as the article output."

math:
	sh docs/katex/render.sh

index: benchmark-index article-index

docs: figures math optimize index

prepare: docs

publish: prepare
	git add .
	git commit -m "Publishing commit"
	git push
