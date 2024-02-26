GO = GOEXPERIMENT=arenas go

sh:
	docker compose run --interactive --tty --rm sh

# Disables assertions to avoid slowdown, then runs the animation builder.
animation: assertions-off measurements-off animate

# Run the test suite.
test: assertions-on persistence-on measurements-off
	${GO} test -count 1 ./trees

# Runs the test suite and collects code coverage.
coverage: assertions-off measurements-off
	${GO} test ./trees -coverprofile "coverage.out"
	go tool cover -html="coverage.out" -o "coverage.html"
	open coverage.html

animate: assertions-off measurements-off
	${GO} run main/animate/animate.go

animate-wasm:
	docker run \
		--rm \
		--env "GOPATH=/go" \
		--volume "GOPATH:/go" \
		--volume .:/bst \
		--workdir /bst \
		--network host \
		--user "":"" \
			tinygo/tinygo:0.29.0 tinygo build \
				-no-debug \
				-gc=leaking \
				-target wasm \
				-o /bst/main/animate/wasm/animate.wasm \
				   /bst/main/animate/wasm/

assertions-on:
	${GO} run main/replace/replace.go -dir "./trees" -find "// assert(" -replace "assert("

assertions-off:
	${GO} run main/replace/replace.go -dir "./trees" -find "  assert(" -replace "  // assert("

measurements-on:
	${GO} run main/replace/replace.go -dir "./trees" -find "// measurement(" -replace "measurement("

measurements-off:
	${GO} run main/replace/replace.go -dir "./trees" -find "  measurement(" -replace "  // measurement("

persistence-on:
	${GO} run main/replace/replace.go -dir "./trees" -find "// tree.persist("       -replace "tree.persist("
	${GO} run main/replace/replace.go -dir "./trees" -find "// tree.share("         -replace "tree.share("
	${GO} run main/replace/replace.go -dir "./trees" -find "// defer tree.release(" -replace "defer tree.release("

persistence-off:
	${GO} run main/replace/replace.go -dir "./trees" -find "  tree.persist("       -replace "  // tree.persist("
	${GO} run main/replace/replace.go -dir "./trees" -find "  tree.share("         -replace "  // tree.share("
	${GO} run main/replace/replace.go -dir "./trees" -find "  defer tree.release(" -replace "  // defer tree.release("

sandbox:
	@${GO} run main/sandbox/sandbox.go

##
# BALANCERS
#

BALANCERS := \
	Median \
	Height \
	Weight \
	Log \
	Cost \
	DSW \

balancer-measurements-%:
	${GO} run docs/benchmarks/main/balancers/measurements.go -strategy $*

balancer-measurements: assertions-off measurements-on
	$(MAKE) -j $(foreach balancer,$(BALANCERS),balancer-measurements-$(balancer))

balancer-benchmarks: assertions-off measurements-off
	${GO} run docs/benchmarks/main/balancers/benchmarks.go


##
# OPERATIONS
#

NON_PERSISTENT_OPERATIONS := \
	Insert \
	InsertDelete \

PERSISTENT_OPERATIONS := \
	InsertPersistent \
	InsertDeletePersistent \

operation-benchmarks-%:
	${GO} run docs/benchmarks/main/operations/benchmarks.go -operation $*

operation-measurements-%:
	${GO} run docs/benchmarks/main/operations/measurements.go -operation $*

operation-non-persistent-benchmarks: assertions-off persistence-off measurements-off
	$(MAKE) $(foreach operation,$(NON_PERSISTENT_OPERATIONS),operation-benchmarks-$(operation))

operation-non-persistent-measurements: assertions-off persistence-off measurements-on
	$(MAKE) $(foreach operation,$(NON_PERSISTENT_OPERATIONS),operation-measurements-$(operation))

operation-persistent-benchmarks: assertions-off persistence-on measurements-off
	$(MAKE) $(foreach operation,$(PERSISTENT_OPERATIONS),operation-benchmarks-$(operation))

operation-persistent-measurements: assertions-off persistence-on measurements-on
	$(MAKE) $(foreach operation,$(PERSISTENT_OPERATIONS),operation-measurements-$(operation))

operation-benchmarks: \
	operation-non-persistent-benchmarks \
	operation-persistent-benchmarks

operation-measurements: \
	operation-non-persistent-measurements \
	operation-persistent-measurements


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

benchmark-index:
	${GO} run docs/benchmarks/index.go

article-index:
	${GO} run index.go

math:
	sh docs/katex/render.sh

index: benchmark-index article-index

docs: figures math optimize index

prepare: docs assertions-on measurements-on persistence-on

publish: prepare
	git add .
	git commit -m "Publishing commit"
	git push