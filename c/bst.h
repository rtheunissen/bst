#ifndef BST_H
#define BST_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef uint64_t u64;

//
// Assertions are checks within code to verify that certain conditions are met.
//
// `assert_bst` can be toggled with -DDEBUG. `invariant` is always on.
//
#ifdef DEBUG
#define assert_bst(cond) do { \
   if (!(cond)) { \
      fprintf(stderr, "assertion failed: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      abort(); \
   } \
} while(0)
#else
#define assert_bst(cond) ((void)0)
#endif

#define invariant(cond) do { \
   if (!(cond)) { \
      fprintf(stderr, "invariant failed: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      abort(); \
   } \
} while(0)

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))


// ─────────────────────────────────────────────────────────────────────────────
// Measurements
//
// Controlled by -DMEASUREMENTS. When enabled, global counters track rotations,
// allocations, and other structural metrics per operation.
// ─────────────────────────────────────────────────────────────────────────────

#ifdef MEASUREMENTS
extern u64 measurement_rotations;
extern u64 measurement_allocations;
extern u64 measurement_partition_count;
extern u64 measurement_partition_depth;

static inline void measurement(u64 *addr, u64 delta) {
   *addr = *addr + delta;
}
#else
#define measurement(addr, delta) ((void)0)
#endif


//
// Node
//
// A node is a simple structure that can be linked to other nodes. Each node has
// a left and right outgoing link to other nodes.
// The node at the top of the tree with no parent is called the _root_ node.
//
//
//                                   (P)arent
//                                 ↙     ↘
//                               (L)eft  (R)ight
//
//
// A node is a container for a unit of information.
// When multiple nodes are linked together they form a binary tree.
// This structure allows the ability to organize information.
// One type of organization is a sequence - linear, list, etc.
//
// A sequence is implied by binary search tree, where the parent appears in the
// sequence after the left node and before the right node.
//
// This can be viewed as a binary search tree ordered by sequential position.
//
// Every node tracks its 0-based position relative to the start of its sequence,
// equal to the number of nodes in its left subtree. Given the total size of a
// node, we can calculate the sizes of both subtrees without referencing them.
//
//
//          Position:     0   1   2   3   4   5   6
//
//          Sequence:    [e,  x,  a,  m,  p,  l,  e]
//
//                                    3
//                                   (m)
//                            1   ↙       ↘   1
//                           (x)             (l)
//                        0 ↙   ↘ 0       0 ↙   ↘ 0
//                       (e)     (a)     (p)     (e)
//
//
// Multiple trees may share the same node, allowing independent trees to be made
// up of common subtrees shared in memory. Making a change to one tree creates
// a new tree that shares most of the previous structure. The reference count
// of a node is the number of other trees that reference it, thus zero indicates
// that a node is only referenced by its own tree and is not shared by others.
//
typedef struct node {
   struct node *l;  // Pointers to the left and right subtrees.
   struct node *r;

   u64 s;     // Size of the left subtree (also position).
   u64 x;     // Data
   u64 y;     // Rank / priority
#ifdef PERSISTENT
   u64 refs;  // Reference count for copy-on-write persistence.
#endif
} node_t;


//
// Tree
//
typedef struct tree {
   node_t *root;
   u64     size;
} tree_t;


// ─────────────────────────────────────────────────────────────────────────────
// Inline node operations
// ─────────────────────────────────────────────────────────────────────────────

// Returns the number of nodes in the left subtree of p*.
static inline u64 sizeL(node_t *p) {
   return p->s;
}

// Returns the number of nodes in the right subtree of p*, given the size of p*.
static inline u64 sizeR(node_t *p, u64 s) {
   return s - p->s - 1;
}

static inline bool isLeaf(node_t *p) {
   return p->l == NULL && p->r == NULL;
}

static inline node_t *rotateL(node_t *p) {
   measurement(&measurement_rotations, 1);
   node_t *r = p->r;
   p->r = r->l;
   r->l = p;
   r->s = r->s + p->s + 1;
   return r;
}

static inline node_t *rotateR(node_t *p) {
   measurement(&measurement_rotations, 1);
   node_t *l = p->l;
   p->l = l->r;
   l->r = p;
   p->s = p->s - l->s - 1;
   return l;
}

// Rotates the LEFT subtree LEFT, then rotates the root RIGHT.
static inline node_t *rotateLR(node_t *p) {
   p->l = rotateL(p->l);
   return rotateR(p);
}

// Rotates the RIGHT subtree RIGHT, then rotates the root LEFT.
static inline node_t *rotateRL(node_t *p) {
   p->r = rotateR(p->r);
   return rotateL(p);
}

static inline node_t **insertL(node_t *p) {
   p->s++;
   return &p->l;
}

static inline node_t **insertR(node_t *p, u64 *i) {
   *i = *i - p->s - 1;
   return &p->r;
}

static inline node_t **deleteL(node_t *p) {
   p->s--;
   return &p->l;
}

static inline node_t **deleteR(node_t *p, u64 *i) {
   *i = *i - p->s - 1;
   return &p->r;
}


// ─────────────────────────────────────────────────────────────────────────────
// Allocator
//
// Two modes: heap (malloc) and arena (bump allocation with bulk free).
// The arena is used for benchmarks where millions of nodes are allocated and
// freed between runs. Set up with arena_init() before use.
// ─────────────────────────────────────────────────────────────────────────────

typedef struct arena_chunk {
   node_t *buffer;
   u64     capacity;
   u64     offset;
   struct arena_chunk *next;  // linked list for overflow chunks
} arena_chunk_t;

typedef struct {
   arena_chunk_t  first;     // initial chunk (inline)
   arena_chunk_t *current;   // current chunk being allocated from
} arena_t;

// Global arena (NULL = use malloc instead)
extern arena_t *global_arena;

void arena_init(arena_t *a, u64 capacity);
void arena_reset(arena_t *a);
void arena_destroy(arena_t *a);

void arena_grow(arena_t *a);

// Allocates memory for a new node and copies the given node into that memory.
static inline node_t *tree_allocate(node_t value) {
   measurement(&measurement_allocations, 1);
   node_t *p;
   if (global_arena) {
      arena_chunk_t *c = global_arena->current;
      if (c->offset >= c->capacity) {
         arena_grow(global_arena);
         c = global_arena->current;
      }
      p = &c->buffer[c->offset++];
   } else {
      p = (node_t *)malloc(sizeof(node_t));
   }
   *p = value;
   return p;
}


// ─────────────────────────────────────────────────────────────────────────────
// Persistence (copy-on-write)
//
// Controlled by -DPERSISTENT. When disabled, persist/share/release are no-ops,
// eliminating all COW overhead for non-persistent benchmarks.
//
// Tests must always be compiled with -DPERSISTENT since the test suite includes
// persistent correctness tests (Clone, InsertPersistent, DeletePersistent, etc).
// ─────────────────────────────────────────────────────────────────────────────

#ifdef PERSISTENT
// Removes one owning reference from the given node. If no other tree owns it,
// the node and its children are released recursively.
void tree_release(node_t *p);

// Frees nodes recursively, respecting reference counts.
void free_nodes(node_t *p);
#else
static inline void tree_release(node_t *p) {
   (void)p;
}

static inline void free_nodes(node_t *p) {
   (void)p;
}
#endif

// Adds a reference to the node to share it with another tree.
static inline void tree_share(node_t *p) {
#ifdef PERSISTENT
   if (p != NULL) {
      p->refs++;
   }
#else
   (void)p;
#endif
}

// Determines whether a node is shared with at least one other tree.
static inline bool tree_shared(node_t *p) {
#ifdef PERSISTENT
   return p->refs > 0;
#else
   (void)p;
   return false;
#endif
}

// Replaces the given node with a copy only if the node is shared with other
// trees. This is copy-on-write: only copy when about to modify a shared node.
static inline void tree_persist(node_t **p) {
#ifdef PERSISTENT
   assert_bst(*p != NULL);
   //
   // There is no need to copy the node if it is NOT shared with other trees.
   //
   if (!tree_shared(*p)) {
      return;
   }
   // A copy is required to modify the node stored at the given reference.
   //
   // Only the pointers to the left and right subtrees are copied, increasing
   // their reference counts by one and thereby sharing them with another tree.
   //
   tree_share((*p)->l);
   tree_share((*p)->r);

   node_t *old = *p;

   // Allocate memory for a new node and copy the original values into it.
   *p = tree_allocate((node_t){
      .l = old->l,
      .r = old->r,
      .s = old->s,
      .x = old->x,
      .y = old->y,
   });

   tree_release(old);
#else
   (void)p;
#endif
}


// ─────────────────────────────────────────────────────────────────────────────
// Tree operations — declarations (defined in tree.c)
// ─────────────────────────────────────────────────────────────────────────────

tree_t tree_clone(tree_t *tree);

// Base insert/delete/lookup/update (used by trees that don't override them)
void tree_insert_base(tree_t *tree, u64 i, u64 x);
u64  tree_delete_base(tree_t *tree, u64 i);
u64  tree_lookup(tree_t *tree, u64 i);
void tree_update_base(tree_t *tree, u64 i, u64 x);

// Internal insert/delete on subtree pointers
void tree_insert_internal(node_t **p, u64 s, u64 i, u64 x);
u64  tree_delete_internal(node_t **p, u64 s, u64 i);

// Split/join (base implementations)
void tree_split_internal(node_t *p, u64 i, node_t **out_l, node_t **out_r);
void tree_split_base(tree_t *tree, u64 i, tree_t *out_l, tree_t *out_r);
tree_t tree_join_base(tree_t *tree, tree_t *other);
node_t *tree_join_internal(node_t *l, node_t *r, u64 sl, u64 sr);

// Dissolve (replace a node with one of its children or a neighbor)
node_t *tree_dissolve(node_t *p, u64 s);

// Delete min/max from subtree
node_t *tree_deleteMin(node_t **p);
node_t *tree_deleteMax(node_t **p);

// Partition
node_t *tree_partition(node_t *p, u64 i);

// Inorder traversal
void inorder(node_t *p, void (*visit)(u64, void *), void *ctx);

// Verification
u64 tree_verify_size(node_t *p, u64 s);

// Recursive height (O(n))
int height(node_t *p);

// Maximum path length (same as height)
static inline int nodeMaximumPathLength(node_t *p) {
   return height(p);
}

// Average path length (O(n))
double nodeAveragePathLength(node_t *p);

// Recursive node count (O(n))
u64 node_size(node_t *p);

// Tree rotations that persist children first
void treeRotateL(node_t **p);
void treeRotateR(node_t **p);
void treeRotateRL(node_t **p);
void treeRotateLR(node_t **p);

// Append to rightmost/leftmost
void tree_appendR(node_t **p, node_t *n);
void tree_appendL(node_t **p, node_t *n);

// replacedBy helpers (for top-down delete)
node_t *tree_replacedByRightSubtree(node_t **p);
node_t *tree_replacedByLeftSubtree(node_t **p);

// pathLeft/pathRight (triple-pointer path traversal for top-down insert)
void tree_pathLeft(node_t ***p);
void tree_pathRight(node_t ***p, u64 *i);

// attach helpers (allocate + link)
void tree_attach(node_t **p, u64 x);
void tree_attachL(node_t *p, u64 x);
void tree_attachR(node_t *p, u64 x);
void tree_attachLL(node_t *p, u64 x);
void tree_attachRR(node_t *p, u64 x);
void tree_attachLR(node_t *p, u64 x);
void tree_attachRL(node_t *p, u64 x);

// pathDeletingRightIgnoringIndex
static inline node_t **pathDeletingRightIgnoringIndex(node_t *p) {
   return &p->r;
}


// ─────────────────────────────────────────────────────────────────────────────
// Utility math
// ─────────────────────────────────────────────────────────────────────────────

// ⌊log₂(x)⌋ when x > 0, otherwise 0
static inline u64 util_log2(u64 x) {
   if (x == 0) return 0;
   return (u64)(63 - __builtin_clzll(x));
}

static inline bool util_gte_msb(u64 x, u64 y) {
   return x >= (~x & y);
}

#endif // BST_H
