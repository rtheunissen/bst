# Go to C Translation Plan

## Goal

Translate the binary search tree implementations and benchmarks from Go to C to produce
reference-quality benchmark results free from garbage collection, runtime scheduling, and
other Go-specific overhead that may reduce trust in the results.

## Style Preservation

It is very important to preserve the stylistic choices of the original Go code during both
the preparation phase and the final C translation. Specifically:

- **Naming**: Keep the same variable names (`p`, `l`, `r`, `s`, `i`, `x`, `sl`, `sr`, etc.),
  function names (e.g. `rotateL`, `persist`, `dissolve`), and struct/type names (adapted for
  C conventions but keeping the same words — `AVLBottomUp` becomes `avl_bottomup`).

- **Spacing and alignment**: Preserve the intentional alignment of assignment operators, the
  vertical spacing between logical sections, and the compact single-line patterns used for
  nil checks (`if l == nil { return r }`). These are deliberate readability choices.

- **Code organization**: Keep the same file-per-tree-type structure. Each tree implementation
  should remain in its own file. Shared operations stay together.

- **Comments**: Preserve all comments, diagrams, and documentation. The ASCII art tree
  diagrams in files like `avl_bottomup.go` are particularly valuable and should survive
  translation verbatim.

- **Algorithm structure**: Do not refactor, optimize, or "improve" algorithms during
  translation. The goal is a faithful line-for-line port, not a rewrite. If the Go code
  uses a sentinel node pattern, the C code should use the same pattern. If the Go code
  uses an iterative loop with pointer-to-pointer, the C code should too.

- **Formatting patterns**: Preserve patterns like the indented assignment alignment:
  ```
  r.s = r.s + p.s + 1
  p.s = p.s - l.s - 1
  ```
  and the compact multi-statement lines where used.

The only naming changes should be mechanical: adapting to C conventions (lowercase with
underscores for types, `->` for pointer member access, etc.).

## Strategy

The translation is split into two phases:

1. **Phase 1 — Prepare Go**: Refactor the Go code to be as C-like as possible while keeping
   it compiling and passing tests. This makes the Phase 2 translation mechanical.

2. **Phase 2 — Translate to C**: Translate the prepared Go code to C.

After each step in Phase 1, run `make test` to verify correctness.

---

## Codebase Overview

### Repository structure

```
trees/                  # Core tree implementations (~7600 lines across 43 Go files)
  node.go               # Node struct, rotations, basic operations (361 lines)
  bst.go                # Tree struct, insert/delete/split/join/lookup (428 lines)
  allocate.go           # ReferenceCounter, copy-on-write persistence (103 lines)
  allocator_arena.go    # Arena allocator (build tag: goexperiment.arenas)
  allocator_heap.go     # Heap allocator (default)
  rank_balanced.go      # RankBalanced helpers: rank, promote, demote, etc. (58 lines)
  balancers.go          # Median, Height, Weight, Log, Cost, DSW balancers (368 lines)
  measurements.go       # Counters for rotations, allocations, path lengths (85 lines)

  # 26 tree implementations:
  avl_bottomup.go       avl_topdown.go         avl.go
  avl_weak_bottomup.go  avl_weak_topdown.go    avl_weak.go
  avl_relaxed_bottomup.go avl_relaxed_topdown.go avl_relaxed.go
  redblack_bottomup.go  redblack_topdown.go    redblack.go
  redblack_relaxed_bottomup.go redblack_relaxed_topdown.go redblack_relaxed.go
  wbst_bottomup.go      wbst_topdown.go        wbst_relaxed.go  wbst.go
  lbst_bottomup.go      lbst_topdown.go        lbst_relaxed.go  lbst.go
  treap_bottomup.go     treap_topdown.go       treap_fingertree.go
  randomized.go         splay.go               zip.go           conc.go
  llrb.go               aa.go
  joinbased.go          # Shared insert/delete/split for join-based trees

  draw.go               # Debug tree drawing
  sandbox.go            # Experiment sandbox
  assert.go             # assert() and invariant()

types/list/             # Abstract list interface and reference implementation
  interface.go          # List interface: Select, Update, Insert, Delete, Split, Join
  reference.go          # Reference implementation (slice-based, for correctness testing)
  tests.go              # Comprehensive test suite
  operation.go          # Operation interface for benchmarks
  operations/
    insert.go           # Insert and InsertPersistent operations
    insert_delete.go    # InsertDelete and InsertDeletePersistent operations

utility/                # Utility functions
  math.go               # Distance, Log2, SmallerMSB, GreaterThanOrEqualToMSB (generics)
  log.go                # Log (generics)
  parity.go             # Even (generics)
  strings.go            # PadLeft, PadRight, Centered, CommaSep (uses x/text)
  choose.go             # Resolve (generics, uses reflect)
  must.go               # Must (generics)
  reflect.go            # NameOf, FuncName (uses reflect)
  random/
    xoshiro256.go       # xoshiro256** PRNG (hand-written, C-ready)
    splitmix64.go       # SplitMix64 seeding (hand-written, C-ready)
    source.go           # Global PRNG, type alias to x/exp/rand.Source
    bounds.go           # LessThan: unbiased bounded random (Lemire's method)
  number/
    distribution.go     # Distribution interface
    distribution/       # Uniform, Normal, Zipf, Skewed, etc.

docs/benchmarks/main/  # Benchmark harnesses
  operations/
    benchmarks.go       # Time-based benchmarks per tree per distribution
    measurements.go     # Structural measurements per tree per distribution
  balancers/
    benchmarks.go       # Balancer benchmarks
    measurements.go     # Balancer measurements
```

### Struct hierarchy (embedding)

```
Node
  └── has ReferenceCounter (embedded)

Tree
  └── has root *Node, size uint64, pool Allocator

RankBalanced (empty struct, just methods)
  ├── AVL (embeds RankBalanced)
  │   ├── AVLBottomUp (embeds AVL + Tree)
  │   ├── AVLTopDown (embeds AVL + Tree)
  │   └── ...
  ├── AVLWeak (embeds RankBalanced)
  │   ├── AVLWeakBottomUp (embeds AVLWeak + Tree)
  │   └── AVLWeakTopDown (embeds AVLWeak + Tree)
  ├── RedBlack (embeds RankBalanced)
  │   ├── RedBlackBottomUp (embeds RedBlack + Tree)
  │   └── ...
  └── RedBlackRelaxed (embeds RankBalanced)
      └── ...

WeightBalance (interface)
  ├── ThreeTwo (implements WeightBalance)
  ├── LogSize (implements WeightBalance)
  ├── LogWeight (implements WeightBalance)
  └── Rational (implements WeightBalance)

WBSTBottomUp (embeds Tree + ThreeTwo)
WBSTTopDown (embeds Tree + ThreeTwo)
LBSTBottomUp (embeds Tree + LogWeight)
...

JoinBased (embeds Tree + Joiner interface)

Splay (embeds Tree)
Zip (embeds Tree + random.Source)
TreapTopDown (embeds Tree + random.Source)
Randomized (embeds Tree + random.Source)
Conc (embeds Tree)
```

### Method count by receiver type

```
Tree:               65 methods   (the base, shared by all trees)
TreapFingerTree:    51 methods   (the most complex single tree)
Conc:               24 methods
WBSTTopDown:        23 methods
AVLBottomUp:        22 methods
WBSTBottomUp:       19 methods
Splay:              17 methods
RedBlackBottomUp:   17 methods
AVLWeakTopDown:     17 methods
Zip:                15 methods
TreapTopDown:       14 methods
Node:               13 methods
RankBalanced:       12 methods
... (497 total methods across all files in trees/)
```

### External dependencies

```
go.mod:
  github.com/eiannone/keyboard    → only used in main/animate/animate.go (not core)
  golang.org/x/exp                → constraints, rand, slices
  golang.org/x/text               → language, message (number formatting)

Usage of x/exp:
  x/exp/constraints  → utility/math.go, utility/log.go, utility/parity.go,
                        utility/strings.go, trees/measurements.go
  x/exp/rand         → utility/random/source.go (type alias),
                        distribution/zipf.go, distribution/normal.go,
                        distribution/gamma.go, distribution/beta.go
  x/exp/slices       → index.go (not core)

Usage of x/text:
  x/text/language    → utility/strings.go (CommaSep), trees/animations/animation.go
  x/text/message     → utility/strings.go (CommaSep), trees/animations/animation.go
```

### Go-specific patterns to eliminate

```
defer               → DONE: all 17 sites replaced with explicit calls
generics            → DONE: monomorphized in core code (Must/Resolve remain in non-core)
interfaces          → list.List (~100 uses), WeightBalance, Joiner, BinaryTree, Measurement
struct embedding    → Tree is embedded in all 26 tree types; some types embed other types
struct methods      → Node and RankBalanced methods converted to free functions;
                      Tree methods (~65) remain as methods (convert during C translation)
type aliases        → DONE: list.Size/Position/Data replaced with local aliases
reflect             → utility/reflect.go: NameOf(), FuncName() (non-core, benchmark only)
build tags          → allocator_arena.go vs allocator_heap.go (use #ifdef in C)
random.Source       → DONE: replaced with concrete *xoshiro256
```

### Remaining Go patterns the C translator must handle

```
list.List interface   → 100 uses across 26 tree files (New, Clone, Join, Split signatures)
                        In C: use a vtable struct of function pointers, or direct calls
                        with a switch on tree type.

Type assertions       → ~50 uses like `other.(*AVLBottomUp)` in Join/Split methods.
                        In C: the tree type is known at compile time, cast directly.

WeightBalance iface   → Embedded in WBST/LBST types. 3 methods (isBalanced, singleRotation,
                        maxHeight). In C: inline as direct function calls per concrete type
                        (ThreeTwo, LogSize, LogWeight). Skip Rational (uses math/big + map).

Joiner interface      → Used by JoinBased. 2 methods. In C: inline into each concrete type.

Named return values   → ~20 functions use `(p *Node)` or `(l, r *Node)` named returns.
                        In C: use local variables.

max/min builtins      → ~8 uses. In C: use a macro or inline function.

Closures              → `inorder(p, visit func(Data))` passes a closure. In C: use a
                        function pointer with context: `void (*visit)(uint64, void*)`.

append/slices         → Used only in verification (InteriorHeights, ExteriorHeights,
                        NodesPerLevel) and balancers (DSW). In C: use dynamic arrays.

Persistence toggle    → The Makefile uses find-replace to comment/uncomment persist/share/
                        release calls. In C: use `#ifdef PERSISTENT` guards.

Build tags            → allocator_arena.go vs allocator_heap.go. In C: use `#ifdef ARENA`.

math package          → Used in avl.go, redblack.go, balancers.go, wbst.go for Log2, Log.
                        In C: use <math.h> equivalents.

math/big + map        → Only in Rational (wbst.go). Skip entirely for C translation.
                        Benchmarks use ThreeTwo, LogSize, LogWeight only.
```

### What does NOT need to change

```
trees/animations/   → visualization code, not part of benchmarks, skip entirely
trees/draw.go       → debug utility, can translate later or skip
main/animate/       → animation builder, skip
main/replace/       → text replacement tool, skip
docs/               → documentation, plots, skip
index.go            → HTML index builder, skip
```

---

## Phase 1: Prepare Go for C Translation

Each step should be committed separately. Run `make test` after each step to verify
correctness. The test suite covers all 26 tree implementations across 9 distributions
and 16 test cases including persistent variants.

### Step 1.1: Convert Node methods to free functions

**Files:** `trees/node.go`
**Count:** ~13 methods on Node

Convert all methods on `Node` to free functions with the receiver as the first argument.

Before:
```go
func (p *Node) rotateL() (r *Node) {
    r = p.r
    p.r = r.l
    r.l = p
    r.s = r.s + p.s + 1
    return r
}
```

After:
```go
func rotateL(p *Node) (r *Node) {
    r = p.r
    p.r = r.l
    r.l = p
    r.s = r.s + p.s + 1
    return r
}
```

This affects call sites throughout the codebase. For example:
- `p.rotateL()` → `rotateL(p)`
- `p.rotateR()` → `rotateR(p)`
- `p.height()` → `height(p)`
- `p.size()` → `size(p)`
- `p.sizeL()` → `sizeL(p)`
- `p.sizeR(s)` → `sizeR(p, s)`
- `p.isLeaf()` → `isLeaf(p)`
- `p.inorder(visit)` → `inorder(p, visit)`
- `p.AveragePathLength()` → `averagePathLength(p)` (also lowercase)
- `p.MaximumPathLength()` → `maxPathLength(p)` (also lowercase)
- `p.Draw(writer)` → can skip (debug only)

Also handle: `ReferenceCounter.AddReference()` and `ReferenceCounter.RemoveReference()`.
Since `ReferenceCounter` is embedded in `Node`, these become:
- `p.AddReference()` → `p.refs++` (inline at call sites in `allocate.go`)
- `p.RemoveReference()` → `if p.refs > 0 { p.refs-- }` (inline)

This step also provides an opportunity to remove the `ReferenceCounter` struct entirely and
just use the `refs` field directly on `Node`.

### Step 1.2: Convert RankBalanced methods to free functions

**Files:** `trees/rank_balanced.go`
**Count:** 12 methods

All `RankBalanced` methods only operate on `*Node` (using the `y` field as rank).
The receiver is an empty struct carrying no state. Convert to free functions:

Before:
```go
func (tree RankBalanced) rank(p *Node) int {
    if p == nil { return -1 }
    return int(p.y)
}
```

After:
```go
func rank(p *Node) int {
    if p == nil { return -1 }
    return int(p.y)
}
```

Functions to convert: `rank`, `rankDifference`, `hasZeroChild`, `isZeroChild`,
`isOneChild`, `isTwoChild`, `isThreeChild`, `promote`, `demote`, `isZeroZero`,
`isOneOne`, `isTwoTwo`.

Update all call sites: `tree.rank(p)` → `rank(p)`, `tree.promote(p)` → `promote(p)`, etc.
These are called from AVL, AVLWeak, RedBlack, and their variants.

Note: `trees/redblack.go` and `trees/avl.go` also have methods like `isRed`, `isBlack`,
`verifyRanks` that depend on `RankBalanced`. Convert these too.

### Step 1.3: Convert Tree methods to free functions

**Files:** `trees/bst.go`, `trees/allocate.go`
**Count:** ~65 methods on Tree

This is the largest single conversion. The `Tree` struct has state (`root`, `size`, `pool`),
so every method gets `tree *Tree` as the first parameter.

Before:
```go
func (tree *Tree) persist(p **Node) { ... }
func (tree *Tree) insert(p **Node, s list.Size, i list.Position, x list.Data) { ... }
func (tree Tree) lookup(p *Node, i list.Size) list.Data { ... }
```

After:
```go
func persist(tree *Tree, p **Node) { ... }
func insert(tree *Tree, p **Node, s uint64, i uint64, x uint64) { ... }
func lookup(tree *Tree, p *Node, i uint64) uint64 { ... }
```

Note the use of concrete `uint64` instead of type aliases (see Step 1.7).

**Important:** Some methods are on `Tree` (value receiver) vs `*Tree` (pointer receiver).
In C, everything takes a pointer. Unify to always pass `*Tree`.

Functions in `bst.go` to convert: `Delete`, `DeleteRoot`, `Select`, `Update`, `insert`,
`Insert`, `Size`, `Root`, `lookup`, `Each`, `join`, `partition`, `dissolveHibbard`,
`dissolveRandom`, `dissolveSymmetric`, `dissolveKnuth`, `dissolvePreferSmallerSubtree`,
`dissolvePreferLargerSubtree`, `dissolve`, `delete`, `split`, `Split`, `Join`,
`joinWith`, `verifySizes`, `InteriorHeightsAlongTheSpines`,
`ExteriorHeightsAlongTheSpines`, `countNodesPerLevel`, `NodesPerLevel`.

Functions in `allocate.go` to convert: `release`, `share`, `shared`, `persist`.

Functions in `node.go` on Tree: `verifySize`, `replacedByRightSubtree`,
`replacedByLeftSubtree`, `deleteMin2`, `deleteMin`, `deleteMax2`, `deleteMax`,
`update`, `pathLeft`, `pathRight`, `attach`, `attachL`, `attachLL`, `attachRR`,
`attachLR`, `attachRL`, `attachR`, `rotateL`, `rotateR`, `rotateRL`, `rotateLR`,
`appendR`, `appendL`.

For functions like `Tree.rotateL(p **Node)` which wrap `Node.rotateL()`, these become:
```go
func treeRotateL(tree *Tree, p **Node) {
    persist(tree, &(*p).r)
    *p = rotateL(*p)
}
```
Choose a naming convention to avoid collision with the node-level `rotateL`.

### Step 1.4: Flatten struct embedding and convert tree-specific methods

**Files:** All 23 tree implementation files + `joinbased.go`

After Steps 1.1–1.3, the shared methods are free functions. Now flatten each tree struct
so it directly contains a `Tree` field (not embedded) and any extra fields.

Before:
```go
type AVLBottomUp struct {
    AVL          // embeds AVL which embeds RankBalanced
    Tree         // embeds Tree
}
```

After:
```go
type AVLBottomUp struct {
    tree Tree
}
```

`RankBalanced` and `AVL` disappear — their methods are already free functions from Steps
1.2 and the AVL-specific verify functions become free functions too.

For trees that embed `random.Source` (Zip, TreapTopDown, Randomized, TreapFingerTree):
```go
type Zip struct {
    tree   Tree
    source random.Source  // concrete xoshiro256, not interface
}
```

For `JoinBased` which embeds `Tree` + `Joiner` interface: flatten and convert `Joiner`
dispatch to explicit function pointer or direct calls (see Step 1.8).

Convert all tree-specific methods to free functions with a prefix or separate naming:
- `avl_bottomup_insert(tree *AVLBottomUp, ...)` or group in a file-scoped way
- Each tree's `Insert`, `Delete`, `Select`, `Update`, `Split`, `Join`, `Verify`, `New`,
  `Clone`, `Free` become free functions

### Step 1.5: Replace `defer` with explicit calls

**Files:** 14 call sites across 13 files
**Pattern:** `defer tree.release(p)` or `defer tree.release(*p)` in delete paths

In every case, `defer tree.release(p)` is used in delete paths to decrement the reference
count of a deleted node at function return. Replace with an explicit call.

`defer` executes after the return value is evaluated, so the release must happen after
the join/zip/etc. result is computed but before the return. Save the result in a local:

Before:
```go
func (tree *AVLBottomUp) delete(p *Node, s list.Size, i list.Position, x *list.Data) *Node {
    if i == sl {
        *x = p.x
        defer tree.release(p)
        tree.share(p.l)
        tree.share(p.r)
        return tree.join(p.l, p.r, sl, sr)
    }
    ...
}
```

After:
```go
func (tree *AVLBottomUp) delete(p *Node, s list.Size, i list.Position, x *list.Data) *Node {
    if i == sl {
        *x = p.x
        tree.share(p.l)
        tree.share(p.r)
        result := tree.join(p.l, p.r, sl, sr)
        tree.release(p)
        return result
    }
    ...
}
```

Call sites that need this transformation (14 total):

- `trees/avl_bottomup.go`         — `delete`: `defer tree.release(p)`
- `trees/avl_weak_bottomup.go`    — `delete`: `defer tree.release(p)`
- `trees/avl_weak_topdown.go`     — `dissolve`: `defer tree.release(*p)`
- `trees/redblack_bottomup.go`    — `delete`: `defer tree.release(p)`
- `trees/wbst_bottomup.go`        — `delete`: `defer tree.release(p)`
- `trees/wbst_topdown.go`         — `delete`: `defer tree.release(*p)`
- `trees/joinbased.go`            — `delete`: `defer tree.release(p)`
- `trees/treap_bottomup.go`       — `delete`: `defer tree.release(p)`
- `trees/treap_topdown.go`        — `delete`: `defer tree.release(*p)`
- `trees/treap_fingertree.go`     — `delete`: `defer tree.release(*p)`
- `trees/treap_fingertree.go`     — `deleteFirst`: `defer tree.release(tree.root.l)`
- `trees/treap_fingertree.go`     — `deleteLast`: `defer tree.release(tree.root.r)`
- `trees/treap_fingertree.go`     — `deleteFromHead`: `defer tree.release(g)`
- `trees/treap_fingertree.go`     — `deleteFromTail`: `defer tree.release(g)`
- `trees/zip.go`                  — `delete`: `defer tree.release(*p)`
- `trees/randomized.go`           — `delete`: `defer tree.release(*p)`
- `trees/splay.go`                — `Delete`: `defer tree.release(tree.root)`

### Step 1.6: Replace `random.Source` interface with concrete `*xoshiro256`

**Files:** `utility/random/source.go`, `trees/treap_bottomup.go`, `trees/treap_topdown.go`,
          `trees/treap_fingertree.go`, `trees/zip.go`, `trees/randomized.go`

Currently `random.Source` is a type alias to `x/exp/rand.Source` (an interface). The Treap,
Zip, Randomized, and TreapFingerTree trees store this interface in their struct. Since all
actual usage is `*xoshiro256`, replace the interface with the concrete type:

Before:
```go
type TreapTopDown struct {
    Tree
    random.Source   // interface
}
```

After:
```go
type TreapTopDown struct {
    Tree
    source *xoshiro256   // concrete
}
```

This also requires updating `random.New()` to return `*xoshiro256` instead of `Source`,
and updating the distributions that use `rand.Source` (Zipf, Normal, Beta, Gamma) to use
`*xoshiro256` directly.

This change removes the main `x/exp/rand` dependency from the core tree implementations.

### Step 1.7: Convert `split` return values to output parameters

**Files:** All tree files that define `split` functions

Go functions that return `(*Node, *Node)` should be converted to use output parameters,
matching C conventions where functions return through pointer arguments:

Before:
```go
func (tree *Tree) split(p *Node, i uint64) (*Node, *Node) {
    ...
    return n.r, n.l
}
```

After:
```go
func (tree *Tree) split(p *Node, i uint64, l **Node, r **Node) {
    ...
    *l = n.r
    *r = n.l
}
```

This affects all `split` functions and their callers (Split, join-based delete, etc.).
Multiple return values have no direct equivalent in C; output parameters are idiomatic.

### Step 1.8: Skip or simplify `Rational` weight balance

**File:** `trees/wbst.go`

The `Rational` type uses `math/big.Rat` for arbitrary-precision arithmetic and a
`map[[2]list.Size]bool` cache. Neither has a simple C equivalent. Since the benchmarks
only use `ThreeTwo`, `LogSize`, and `LogWeight`:

- Exclude `Rational` from the C translation entirely, OR
- Replace with fixed-precision integer arithmetic (multiply-then-compare)

The `WeightBalance` interface (`isBalanced`, `singleRotation`, `maxHeight`) should be
inlined into each concrete WBST/LBST type as direct function calls, eliminating the
interface dispatch.

### Step 1.9: Monomorphize generics

**Files:** `utility/math.go`, `utility/log.go`, `utility/parity.go`, `utility/strings.go`,
          `utility/choose.go`, `utility/must.go`, `trees/measurements.go`
**Count:** 9 generic functions

All generic functions are used with either `uint64` or `int`. Replace with concrete versions.

Before:
```go
func Distance[T constraints.Integer](a, b T) T {
    if a > b { return a - b }
    return b - a
}
```

After:
```go
func distanceUint64(a, b uint64) uint64 {
    if a > b { return a - b }
    return b - a
}

func distanceInt(a, b int) int {
    if a > b { return a - b }
    return b - a
}
```

Check which concrete types each function is actually called with and only generate those.

Also remove `import "golang.org/x/exp/constraints"` from all files.

### Step 1.10: Simplify type aliases

**Files:** `types/list/interface.go`, `trees/node.go`, throughout

Currently there are redundant type aliases:
```go
// types/list/interface.go
type Size = uint64
type Position = uint64
type Data = uint64

// trees/node.go
type Rank = uint64
type Size = uint64
type Data = uint64
```

Replace all uses of `list.Size`, `list.Position`, `list.Data`, `trees.Size`, `trees.Data`,
`trees.Rank` with plain `uint64` throughout. This eliminates the cross-package dependency
between `trees/` and `types/list/` for type definitions.

### Step 1.11: Eliminate interfaces

**Interfaces to eliminate:**

1. **`list.List`** — the main interface (Select, Update, Insert, Delete, Split, Join, etc.).
   Each tree type implements this. In the test suite and benchmarks, trees are used via this
   interface. Replace with either:
   - A struct of function pointers (C vtable style), or
   - Direct calls with a switch on tree type (simpler)

   For benchmarks, function pointers are cleaner:
   ```go
   type TreeOps struct {
       Insert func(i, x uint64)
       Delete func(i uint64) uint64
       Select func(i uint64) uint64
       // ...
   }
   ```

2. **`list.Operation`** — used in benchmarks to define Insert, InsertDelete, etc.
   Small interface (4 methods). Replace with a struct of function pointers or direct calls.

3. **`number.Distribution`** — used for random position generation.
   Has 2 methods: `New(uint64) Distribution` and `LessThan(uint64) uint64`.
   Replace with a struct containing a function pointer and state.

4. **`trees.BinaryTree`** — used only in measurements. 3 methods. Inline or remove.

5. **`trees.Measurement`** — 2 methods (Reset, Measure). Replace with function pointers.

6. **`trees.Balancer`** — 2 methods (Restore, Verify). Replace with function pointers.

7. **`trees.WeightBalance`** — 3 methods used by WBST/LBST. Since the concrete types
   (ThreeTwo, LogSize, LogWeight, Rational) are known, use direct calls.

8. **`trees.Joiner`** — 2 methods used by JoinBased. Inline into each concrete tree type.

9. **`random.Source`** (type alias to `x/exp/rand.Source`) — replace with concrete
   `*xoshiro256` everywhere.

### Step 1.12: Remove external dependencies

After the above steps, eliminate remaining external imports:

1. **`golang.org/x/exp/constraints`** — gone after Step 1.9 (monomorphize generics)
2. **`golang.org/x/exp/rand`** — gone after Step 1.6 (replace `random.Source` with
   concrete `*xoshiro256`, rewrite Zipf/Normal/Beta/Gamma distributions)
3. **`golang.org/x/text`** — replace `CommaSep` with a manual implementation
4. **`golang.org/x/exp/slices`** — only in `index.go`, not core

After this, `go.mod` should only have `github.com/eiannone/keyboard` (animation only).

### Step 1.13: Remove reflect usage

**File:** `utility/reflect.go`

`NameOf(v any) string` uses `reflect.TypeOf` to get a type's name. This is used in:
- Benchmark harnesses (for file/directory naming)
- Test suite (for test naming)
- `utility/choose.go` Resolve (for matching operation names)

Replace with explicit name methods or a registration map:
```go
func treeName(tree *SomeTree) string { return "AVLBottomUp" }
```

Or add a `Name() string` method/function to each tree type.

### Step 1.14: Lowercase struct names

Rename all exported types to lowercase with underscores to match C conventions:

```
Node             → node
Tree             → tree
AVLBottomUp      → avl_bottomup
RedBlackTopDown  → redblack_topdown
TreapFingerTree  → treap_fingertree
...
```

Do this last since it touches every file and every reference.

---

## Phase 2: Translate to C

### Step 2.1: Core data structures

Create `bst.h` and `bst.c` with the fundamental types:

```c
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

typedef uint64_t u64;

typedef struct node {
    struct node *l;
    struct node *r;
    u64 s;     // size of left subtree (also position)
    u64 x;     // data
    u64 y;     // rank / priority
    u64 refs;  // reference count for persistence
} node_t;

typedef struct tree {
    node_t *root;
    u64 size;
    // allocator fields TBD
} tree_t;
```

### Step 2.2: Memory allocator

Implement two allocator strategies:

1. **Simple heap allocator** (malloc/free with optional free list)
2. **Arena allocator** (bump allocation, bulk free)

```c
// Arena allocator
typedef struct arena {
    char *buffer;
    size_t capacity;
    size_t offset;
} arena_t;

node_t *arena_alloc(arena_t *a);
void arena_free(arena_t *a);
```

### Step 2.3: PRNG and utility

Translate directly from Go (these are already C-like):

- `xoshiro256.go` → `xoshiro256.h/c`
- `splitmix64.go` → `splitmix64.h/c`
- `bounds.go` → `random.h/c` (LessThan using `__uint128_t` for Lemire's method)
- `math.go` → `util.h/c` (Log2 via `__builtin_clzll`, Distance, MSB comparisons)

### Step 2.4: Distributions

Translate each distribution to C:

- Uniform: straightforward (LessThan wrapper)
- Normal: Box-Muller or Ziggurat
- Zipf: translate from x/exp/rand implementation
- Skewed, Beta, Gamma: translate from Go implementations

Use a function pointer struct:
```c
typedef struct distribution {
    void *state;
    u64 (*less_than)(void *state, u64 n);
} distribution_t;
```

### Step 2.5: Node operations

Translate all free functions from Phase 1 Step 1.1:

```c
static inline node_t *rotate_l(node_t *p) {
    node_t *r = p->r;
    p->r = r->l;
    r->l = p;
    r->s = r->s + p->s + 1;
    return r;
}
```

### Step 2.6: Tree operations (base)

Translate the shared Tree functions from `bst.go` and `allocate.go`:

```c
void persist(tree_t *tree, node_t **p);
void share(tree_t *tree, node_t *p);
void release(tree_t *tree, node_t *p);
node_t *tree_allocate(tree_t *tree, node_t value);
```

### Step 2.7: Individual tree implementations

Translate each tree, one at a time. Recommended order (simplest to most complex):

1. **Splay** (250 lines, no rank, no balancing invariants)
2. **Randomized** (139 lines, simple random insertion)
3. **Zip** (206 lines, uses random ranks)
4. **TreapBottomUp** (recursive insert with rotate-up)
5. **TreapTopDown** (197 lines, random priorities)
6. **Conc** (271 lines, different structure but self-contained)
7. **LLRB** (left-leaning red-black, Sedgewick's simplification)
8. **AA** (Arne Andersson's tree, right-leaning reds only)
9. **AVLBottomUp** (686 lines, well-documented, reference AVL)
10. **AVLTopDown** (113 lines, iterative variant)
11. **AVLWeakBottomUp** / **AVLWeakTopDown** (263 / 493 lines)
12. **AVLRelaxedBottomUp** / **AVLRelaxedTopDown** (120 / 112 lines)
13. **RedBlackBottomUp** / **RedBlackTopDown** (317 / 212 lines)
14. **RedBlackRelaxedBottomUp** / **RedBlackRelaxedTopDown**
15. **WBSTBottomUp** / **WBSTTopDown** / **WBSTRelaxed** (325 / 393 / 162 lines)
16. **LBSTBottomUp** / **LBSTTopDown** / **LBSTRelaxed**
17. **TreapFingerTree** (837 lines, most complex)

Each tree gets its own `.c` file and a common header.

### Step 2.8: Assertions and invariants

```c
#ifdef DEBUG
  #define assert(cond) do { if (!(cond)) { fprintf(stderr, "assertion failed: %s:%d\n", __FILE__, __LINE__); abort(); } } while(0)
#else
  #define assert(cond) ((void)0)
#endif

#define invariant(cond) do { if (!(cond)) { fprintf(stderr, "invariant failed: %s:%d\n", __FILE__, __LINE__); abort(); } } while(0)
```

This mirrors the Go pattern where `assert` can be toggled but `invariant` is always on.

### Step 2.9: Benchmark harness

Create a C benchmark program that:

1. Iterates over tree implementations via function pointer tables
2. Iterates over distributions
3. Times operations using `clock_gettime(CLOCK_MONOTONIC_RAW)`
4. Outputs the same CSV format as the Go benchmarks
5. Disables GC concerns (N/A in C, but disable ASLR, set CPU affinity, etc.)

```c
typedef struct tree_vtable {
    const char *name;
    void (*init)(tree_t *t);
    void (*insert)(tree_t *t, u64 i, u64 x);
    u64  (*delete)(tree_t *t, u64 i);
    u64  (*select_)(tree_t *t, u64 i);
    void (*verify)(tree_t *t);
    void (*free_)(tree_t *t);
} tree_vtable_t;
```

### Step 2.10: Build system

Extend the existing `Makefile`:

```makefile
CC = gcc
CFLAGS = -O2 -march=native -Wall -Wextra
CFLAGS_DEBUG = -O0 -g -DDEBUG

c-test: c/*.c c/*.h
    $(CC) $(CFLAGS_DEBUG) -o bst_test c/*.c -lm
    ./bst_test

c-bench: c/*.c c/*.h
    $(CC) $(CFLAGS) -o bst_bench c/bench.c c/*.c -lm
    ./bst_bench
```

### Step 2.11: Verify C against Go

Run both the Go and C benchmarks on the same machine and compare:

1. **Correctness:** Run the same test sequences and verify identical results
2. **Structural:** Verify that tree shapes match (same path lengths, same sizes)
3. **Performance:** Compare to ensure C results are in the expected range

---

## Files to translate (priority order)

### Core (must translate)
```
trees/node.go                → c/node.c, c/node.h
trees/bst.go                 → c/tree.c, c/tree.h
trees/allocate.go            → c/persist.c (COW persistence)
trees/allocator_heap.go      → c/alloc.c, c/alloc.h
trees/rank_balanced.go       → c/rank.h (inline functions)
trees/assert.go              → c/assert.h (macros)
trees/balancers.go           → c/balancers.c, c/balancers.h

All 23 tree implementations  → c/<tree_name>.c

utility/random/xoshiro256.go → c/xoshiro256.c, c/xoshiro256.h
utility/random/splitmix64.go → c/splitmix64.c, c/splitmix64.h
utility/random/bounds.go     → c/random.c, c/random.h
utility/math.go              → c/util.h (inline functions)
```

### Benchmarking (must translate)
```
utility/number/distribution/*.go              → c/dist_*.c
types/list/operations/insert.go               → c/bench_ops.c
types/list/operations/insert_delete.go        → c/bench_ops.c
docs/benchmarks/main/operations/benchmarks.go → c/bench_main.c
docs/benchmarks/main/operations/measurements.go → c/measure_main.c
```

### Testing (should translate)
```
types/list/reference.go   → c/reference.c (array-based reference)
types/list/tests.go       → c/test.c
trees/bst_test.go         → c/test_main.c
```

### Skip (not needed for benchmarks)
```
trees/animations/         → visualization only
trees/draw.go             → debug only
trees/sandbox.go          → experiments only
main/animate/             → animation builder
main/replace/             → Go text tool
docs/                     → documentation, plots
index.go                  → HTML builder
```

---

## Key translation patterns

### Go pointer-to-pointer → C pointer-to-pointer

Go's `**Node` pattern for in-place mutation translates directly:
```go
func insert(tree *Tree, p **Node, ...) {
    tree.persist(p)
    p = &(*p).l
}
```
```c
void insert(tree_t *tree, node_t **p, ...) {
    persist(tree, p);
    p = &(*p)->l;
}
```

### Go multiple return values → C output parameters

```go
func split(p *Node, i uint64) (*Node, *Node) { ... }
```
```c
void split(node_t *p, u64 i, node_t **out_l, node_t **out_r) { ... }
// OR
typedef struct { node_t *l; node_t *r; } split_result_t;
split_result_t split(node_t *p, u64 i) { ... }
```

### Go named return values → C local variables

```go
func rotateL(p *Node) (r *Node) {
    r = p.r
    ...
    return r
}
```
```c
node_t *rotate_l(node_t *p) {
    node_t *r = p->r;
    ...
    return r;
}
```

### Go nil → C NULL

Direct mapping. Nil checks become NULL checks.

### Go slices → C arrays + length

For dynamic arrays (used in verification, benchmarks, not in core tree ops):
```c
typedef struct {
    u64 *data;
    u64 len;
    u64 cap;
} vec_u64_t;
```

### Go closures → C function pointers + context

```go
func (p *Node) inorder(visit func(uint64)) { ... }
```
```c
void inorder(node_t *p, void (*visit)(u64, void*), void *ctx) { ... }
```
