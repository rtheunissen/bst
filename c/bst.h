#ifndef BST_H
#define BST_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

typedef uint64_t u64;

#if defined(__GNUC__) || defined(__clang__)
#define BST_NORETURN __attribute__((noreturn))
#define BST_RETURNS_NONNULL __attribute__((returns_nonnull))
#else
#define BST_NORETURN
#define BST_RETURNS_NONNULL
#endif

#if defined(__clang__)
#define BST_ASSUME(cond) __builtin_assume(cond)
#elif defined(__GNUC__)
#define BST_ASSUME(cond)                                                       \
   do {                                                                        \
      if (!(cond)) {                                                           \
         __builtin_unreachable();                                              \
      }                                                                        \
   } while (0)
#else
#define BST_ASSUME(cond) ((void)0)
#endif

static inline void BST_NORETURN bst_assertion_failed(const char *kind,
                                                     const char *file, int line,
                                                     const char *condition) {
   fprintf(stderr, "%s failed: %s:%d: %s\n", kind, file, line, condition);
   abort();
}

static inline void BST_NORETURN bst_unreachable_reached(const char *file,
                                                        int line) {
   fprintf(stderr, "unreachable reached: %s:%d\n", file, line);
   abort();
}

#define bst_unreachable() bst_unreachable_reached(__FILE__, __LINE__)

//
// Assertions are checks within code to verify that certain conditions are met.
//
// `assert_bst` can be toggled with -DDEBUG. `invariant` is always on.
//
#ifdef DEBUG
#define assert_bst(cond)                                                       \
   do {                                                                        \
      bool bst_assertion_condition = (cond);                                   \
      if (!bst_assertion_condition) {                                          \
         bst_assertion_failed("assertion", __FILE__, __LINE__, #cond);         \
      }                                                                        \
      BST_ASSUME(bst_assertion_condition);                                     \
   } while (0)
#else
#define assert_bst(cond) ((void)0)
#endif

#ifdef BST_DISABLE_INVARIANTS
#define invariant(cond) ((void)0)
#else
#define invariant(cond)                                                        \
   do {                                                                        \
      bool bst_invariant_condition = (cond);                                   \
      if (!bst_invariant_condition) {                                          \
         bst_assertion_failed("invariant", __FILE__, __LINE__, #cond);         \
      }                                                                        \
      BST_ASSUME(bst_invariant_condition);                                     \
   } while (0)
#endif

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static inline bool bst_name_is_printable_token(const char *name) {
   if (name == NULL || name[0] == '\0') {
      return false;
   }
   for (const unsigned char *p = (const unsigned char *)name; *p != '\0'; p++) {
      if (*p < '!' || *p > '~') {
         return false;
      }
   }
   return true;
}

// Registry names that become output paths must be printable single components.
static inline bool bst_name_is_path_component(const char *name) {
   if (!bst_name_is_printable_token(name) || strcmp(name, ".") == 0 ||
       strcmp(name, "..") == 0) {
      return false;
   }
   for (const unsigned char *p = (const unsigned char *)name; *p != '\0'; p++) {
      if (*p == '/' || *p == '\\') {
         return false;
      }
   }
   return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Checked allocation
// ─────────────────────────────────────────────────────────────────────────────

static inline u64 bst_array_max_count(size_t elem_size) {
   invariant(elem_size > 0);
   u64 max_count = (u64)(SIZE_MAX / elem_size);
   invariant(max_count > 0);
   return max_count;
}

static inline size_t bst_array_bytes(u64 count, size_t elem_size) {
   invariant(count > 0);
   invariant(count <= bst_array_max_count(elem_size));
   return (size_t)count * elem_size;
}

typedef struct {
   uintptr_t start;
   uintptr_t end;
} bst_array_byte_range_t;

static inline bst_array_byte_range_t bst_array_byte_range(const void *ptr,
                                                          size_t bytes) {
   invariant(ptr != NULL);
   invariant(bytes > 0);
   uintptr_t byte_count = (uintptr_t)bytes;
   invariant((size_t)byte_count == bytes);
   uintptr_t start = (uintptr_t)ptr;
   invariant(start <= UINTPTR_MAX - byte_count);
   return (bst_array_byte_range_t){
       .start = start,
       .end = start + byte_count,
   };
}

static inline void bst_array_copy(void *dst, const void *src, u64 count,
                                  size_t size) {
   if (count == 0) {
      return;
   }
   invariant(size > 0);
   invariant(dst != NULL);
   invariant(src != NULL);
   size_t bytes = bst_array_bytes(count, size);
   bst_array_byte_range_t dst_range = bst_array_byte_range(dst, bytes);
   bst_array_byte_range_t src_range = bst_array_byte_range(src, bytes);
   invariant(dst_range.end <= src_range.start ||
             src_range.end <= dst_range.start);
   memcpy(dst, src, bytes);
}

static inline void bst_array_move(void *dst, const void *src, u64 count,
                                  size_t size) {
   if (count == 0) {
      return;
   }
   invariant(size > 0);
   invariant(dst != NULL);
   invariant(src != NULL);
   size_t bytes = bst_array_bytes(count, size);
   (void)bst_array_byte_range(dst, bytes);
   (void)bst_array_byte_range(src, bytes);
   memmove(dst, src, bytes);
}

static inline void bst_array_zero(void *ptr, u64 count, size_t size) {
   if (count == 0) {
      return;
   }
   invariant(size > 0);
   invariant(ptr != NULL);
   size_t bytes = bst_array_bytes(count, size);
   (void)bst_array_byte_range(ptr, bytes);
   memset(ptr, 0, bytes);
}

static inline u64 bst_array_next_cap(u64 cap, u64 needed, u64 initial,
                                     size_t elem_size) {
   invariant(needed > cap);
   invariant(initial > 0);
   invariant(elem_size > 0);

   u64 max_cap = bst_array_max_count(elem_size);
   invariant(cap <= max_cap);
   invariant(needed <= max_cap);
   invariant(initial <= max_cap);

   u64 next = cap == 0 ? initial : cap;
   while (next < needed) {
      if (next > max_cap / 2) {
         next = needed;
         break;
      }
      next *= 2;
   }
   invariant(next > cap);
   invariant(next >= needed);
   invariant(next <= max_cap);
   return next;
}

static inline void *BST_RETURNS_NONNULL bst_malloc(size_t size) {
   invariant(size > 0);
   void *ptr = malloc(size);
   invariant(ptr != NULL);
   return ptr;
}

static inline void *BST_RETURNS_NONNULL bst_malloc_array(u64 count,
                                                         size_t size) {
   return bst_malloc(bst_array_bytes(count, size));
}

static inline void *BST_RETURNS_NONNULL bst_calloc_array(u64 count,
                                                         size_t size) {
   (void)bst_array_bytes(count, size);
   void *ptr = calloc((size_t)count, size);
   invariant(ptr != NULL);
   return ptr;
}

static inline void *BST_RETURNS_NONNULL bst_calloc(size_t count, size_t size) {
   return bst_calloc_array((u64)count, size);
}

static inline void *BST_RETURNS_NONNULL bst_realloc(void *ptr, size_t size) {
   invariant(ptr != NULL);
   invariant(size > 0);
   void *next = realloc(ptr, size);
   invariant(next != NULL);
   return next;
}

static inline void *BST_RETURNS_NONNULL bst_realloc_array(void *ptr, u64 count,
                                                          size_t size) {
   return bst_realloc(ptr, bst_array_bytes(count, size));
}

// ─────────────────────────────────────────────────────────────────────────────
// Measurements
//
// Controlled by -DMEASUREMENTS. When enabled, global counters track rotations,
// allocations, and other structural metrics per operation.
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   u64 rotations;
   u64 allocations;
   u64 partition_count;
   u64 partition_depth;
} measurement_snapshot_t;

static inline void
measurement_snapshot_require(measurement_snapshot_t snapshot) {
   invariant(snapshot.partition_count > 0 || snapshot.partition_depth == 0);
}

#ifdef MEASUREMENTS
extern u64 measurement_rotations;
extern u64 measurement_allocations;
extern u64 measurement_partition_count;
extern u64 measurement_partition_depth;

static inline void measurement(u64 *addr, u64 delta) {
   invariant(addr != NULL);
   invariant(*addr <= UINT64_MAX - delta);
   *addr = *addr + delta;
}

static inline void measurement_record_rotation(void) {
   measurement(&measurement_rotations, 1);
}

static inline void measurement_record_allocation(void) {
   measurement(&measurement_allocations, 1);
}

static inline void measurement_record_partition(void) {
   measurement(&measurement_partition_count, 1);
}

static inline void measurement_record_partition_descent(void) {
   invariant(measurement_partition_count > 0);
   measurement(&measurement_partition_depth, 1);
}

static inline void measurement_reset_all(void) {
   measurement_rotations = 0;
   measurement_allocations = 0;
   measurement_partition_count = 0;
   measurement_partition_depth = 0;
}

static inline measurement_snapshot_t measurement_snapshot(void) {
   measurement_snapshot_t snapshot = {
       .rotations = measurement_rotations,
       .allocations = measurement_allocations,
       .partition_count = measurement_partition_count,
       .partition_depth = measurement_partition_depth,
   };
   measurement_snapshot_require(snapshot);
   return snapshot;
}
#else
#define measurement(addr, delta) ((void)0)

static inline void measurement_record_rotation(void) {}

static inline void measurement_record_allocation(void) {}

static inline void measurement_record_partition(void) {}

static inline void measurement_record_partition_descent(void) {}

static inline void measurement_reset_all(void) {}

static inline measurement_snapshot_t measurement_snapshot(void) {
   measurement_snapshot_t snapshot = {0};
   measurement_snapshot_require(snapshot);
   return snapshot;
}
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
   struct node *l; // Pointers to the left and right subtrees.
   struct node *r;

   u64 s; // Size of the left subtree (also position).
   u64 x; // Data
   u64 y; // Rank / priority
#ifdef PERSISTENT
   u64 refs; // Reference count for copy-on-write persistence.
#endif
} node_t;

//
// Tree
//
static inline bool tree_root_size_is_valid(node_t *root, u64 size) {
   return (root == NULL) == (size == 0);
}

static inline void tree_root_size_require(node_t *root, u64 size) {
   invariant(tree_root_size_is_valid(root, size));
}

static inline bool tree_sized_root_is_valid(node_t *root, u64 size) {
   return tree_root_size_is_valid(root, size) &&
          (root == NULL || root->s < size);
}

static inline void tree_sized_root_require(node_t *root, u64 size) {
   invariant(tree_sized_root_is_valid(root, size));
}

static inline void tree_nonempty_root_require(node_t *root, u64 size) {
   tree_sized_root_require(root, size);
   invariant(root != NULL);
   invariant(size > 0);
}

static inline bool tree_sized_root_shape_is_valid(node_t *root, u64 size) {
   if (!tree_sized_root_is_valid(root, size)) {
      return false;
   }
   if (root == NULL) {
      return true;
   }

   u64 right_size = size - 1 - root->s;
   return (root->l == NULL) == (root->s == 0) &&
          (root->r == NULL) == (right_size == 0);
}

static inline void tree_sized_root_shape_require(node_t *root, u64 size) {
   invariant(tree_sized_root_shape_is_valid(root, size));
}

typedef struct tree {
   node_t *root;
   u64 size;
} tree_t;

static inline bool tree_handle_is_valid(tree_t tree) {
   return tree_root_size_is_valid(tree.root, tree.size);
}

static inline void tree_handle_require(tree_t tree) {
   invariant(tree_handle_is_valid(tree));
}

static inline bool tree_sized_handle_is_valid(tree_t tree) {
   return tree_sized_root_shape_is_valid(tree.root, tree.size);
}

static inline void tree_sized_handle_require(tree_t tree) {
   invariant(tree_sized_handle_is_valid(tree));
}

// Checked sum for sequence sizes.
static inline u64 size_sum(u64 left_size, u64 right_size) {
   invariant(left_size <= UINT64_MAX - right_size);
   return left_size + right_size;
}

// Size after adding one item to a sequence that is not already at max size.
static inline u64 size_after_inserting_one(u64 size) {
   invariant(size < UINT64_MAX);
   return size + 1;
}

// Size remaining after removing one item from a nonempty sequence.
static inline u64 size_after_removing_one(u64 size) {
   invariant(size > 0);
   return size - 1;
}

static inline void tree_nonempty_root_shape_require(node_t *root, u64 size) {
   tree_sized_root_shape_require(root, size);
   invariant(root != NULL);
   invariant(size > 0);
}

static inline u64 tree_join_inputs_require(node_t *left, node_t *right,
                                           u64 left_size, u64 right_size) {
   tree_sized_root_shape_require(left, left_size);
   tree_sized_root_shape_require(right, right_size);
   return size_sum(left_size, right_size);
}

// Total subtree size from left and right child subtree sizes.
static inline u64 tree_size_from_child_sizes(u64 left_size, u64 right_size) {
   u64 children = size_sum(left_size, right_size);
   return size_after_inserting_one(children);
}

// Maximum leaf count whose full-binary physical tree fits in node_count nodes.
static inline u64 bst_full_binary_tree_max_leaves_for_nodes(u64 node_count) {
   invariant(node_count > 0);
   return node_count / 2 + (node_count & 1);
}

// Physical node count for a full binary tree with leaf_count logical leaves.
static inline u64 bst_full_binary_tree_nodes_from_leaves(u64 leaf_count) {
   invariant(leaf_count > 0);
   invariant(leaf_count <=
             bst_full_binary_tree_max_leaves_for_nodes(UINT64_MAX));
   return size_sum(leaf_count, size_after_removing_one(leaf_count));
}

static inline u64 tree_build_inputs_require(node_t *left, node_t *pivot,
                                            node_t *right, u64 left_size,
                                            u64 right_size) {
   tree_sized_root_shape_require(left, left_size);
   invariant(pivot != NULL);
   tree_sized_root_shape_require(right, right_size);
   return tree_size_from_child_sizes(left_size, right_size);
}

// Number of items left after removing prefix_size items from size items.
static inline u64 suffix_size_after_prefix(u64 size, u64 prefix_size) {
   invariant(prefix_size <= size);
   return size - prefix_size;
}

// Number of items right of a split in a sequence of size items.
static inline u64 sequence_split_inputs_require(u64 split_size, u64 size) {
   return suffix_size_after_prefix(size, split_size);
}

// Number of valid insertion positions in a sequence of size items.
static inline u64 sequence_insert_position_count(u64 size) {
   return size_after_inserting_one(size);
}

// Size after inserting at a valid gap in a sequence.
static inline u64 sequence_insert_inputs_require(u64 index, u64 size) {
   u64 positions = sequence_insert_position_count(size);
   invariant(index < positions);
   return positions;
}

// Existing item rank in a nonempty sequence.
static inline void sequence_rank_inputs_require(u64 rank, u64 size) {
   invariant(rank < size);
}

// Size after deleting an existing item from a sequence.
static inline u64 sequence_delete_inputs_require(u64 rank, u64 size) {
   sequence_rank_inputs_require(rank, size);
   return size_after_removing_one(size);
}

static inline u64 tree_split_inputs_require(node_t *root, u64 split_size,
                                            u64 size) {
   tree_sized_root_shape_require(root, size);
   return sequence_split_inputs_require(split_size, size);
}

// Size after inserting at a valid gap in a tree.
static inline u64 tree_insert_inputs_require(node_t *root, u64 index,
                                             u64 size) {
   tree_sized_root_shape_require(root, size);
   return sequence_insert_inputs_require(index, size);
}

// Existing item at rank in a nonempty tree.
static inline void tree_rank_inputs_require(node_t *root, u64 rank, u64 size) {
   tree_nonempty_root_shape_require(root, size);
   sequence_rank_inputs_require(rank, size);
}

// Size after deleting an existing item from a tree.
static inline u64 tree_delete_inputs_require(node_t *root, u64 rank, u64 size) {
   tree_nonempty_root_shape_require(root, size);
   return sequence_delete_inputs_require(rank, size);
}

// Number of items after rank in a sequence of size items.
static inline u64 suffix_size_after_rank(u64 size, u64 rank) {
   invariant(rank < size);
   return suffix_size_after_prefix(size, size_after_inserting_one(rank));
}

// Rank of the final item in a nonempty sequence.
static inline u64 rank_of_last(u64 size) {
   return size_after_removing_one(size);
}

// Rank of the same item when the sequence is counted from the end.
static inline u64 rank_from_end(u64 size, u64 rank) {
   invariant(rank < size);
   u64 final_rank = rank_of_last(size);
   return suffix_size_after_prefix(final_rank, rank);
}

// Local rank after descending past a root with left_size nodes before it.
static inline u64 rank_in_right_subtree(u64 rank, u64 left_size) {
   invariant(rank > left_size);
   u64 prefix_size = size_after_inserting_one(left_size);
   return suffix_size_after_prefix(rank, prefix_size);
}

// Rank of subtree's root after prefix_size nodes that are all before subtree.
static inline u64 rank_after_prefix(u64 prefix_size, node_t *subtree) {
   invariant(subtree != NULL);
   u64 rank = size_sum(prefix_size, subtree->s);
   invariant(rank < UINT64_MAX);
   return rank;
}

// Rank of subtree's root when subtree starts immediately after predecessor_rank.
static inline u64 rank_after_predecessor(u64 predecessor_rank,
                                         node_t *subtree) {
   invariant(subtree != NULL);
   u64 prefix_size = size_after_inserting_one(predecessor_rank);
   return rank_after_prefix(prefix_size, subtree);
}

// ─────────────────────────────────────────────────────────────────────────────
// Inline node operations
// ─────────────────────────────────────────────────────────────────────────────

// Returns the number of nodes in the left subtree of p*.
static inline u64 sizeL(node_t *p) {
   invariant(p != NULL);
   return p->s;
}

// Returns the number of nodes in the right subtree of p*, given the size of p*.
static inline u64 sizeR(node_t *p, u64 s) {
   invariant(p != NULL);
   return suffix_size_after_rank(s, p->s);
}

static inline bool isLeaf(node_t *p) {
   invariant(p != NULL);
   return p->l == NULL && p->r == NULL;
}

static inline node_t *rotateL(node_t *p) {
   invariant(p != NULL);
   invariant(p->r != NULL);
   measurement_record_rotation();
   node_t *r = p->r;
   p->r = r->l;
   r->l = p;
   r->s = rank_after_predecessor(p->s, r);
   return r;
}

static inline node_t *rotateR(node_t *p) {
   invariant(p != NULL);
   invariant(p->l != NULL);
   measurement_record_rotation();
   node_t *l = p->l;
   invariant(l->s < p->s);
   p->l = l->r;
   l->r = p;
   p->s = rank_in_right_subtree(p->s, l->s);
   return l;
}

// Rotates the LEFT subtree LEFT, then rotates the root RIGHT.
static inline node_t *rotateLR(node_t *p) {
   invariant(p != NULL);
   invariant(p->l != NULL);
   invariant(p->l->r != NULL);
   p->l = rotateL(p->l);
   return rotateR(p);
}

// Rotates the RIGHT subtree RIGHT, then rotates the root LEFT.
static inline node_t *rotateRL(node_t *p) {
   invariant(p != NULL);
   invariant(p->r != NULL);
   invariant(p->r->l != NULL);
   p->r = rotateR(p->r);
   return rotateL(p);
}

static inline node_t **insertL(node_t *p) {
   invariant(p != NULL);
   invariant(p->s < UINT64_MAX);
   p->s = size_after_inserting_one(p->s);
   return &p->l;
}

static inline node_t **insertR(node_t *p, u64 *i) {
   invariant(p != NULL);
   invariant(i != NULL);
   invariant(*i > p->s);
   *i = rank_in_right_subtree(*i, p->s);
   return &p->r;
}

static inline node_t **deleteL(node_t *p) {
   invariant(p != NULL);
   invariant(p->s > 0);
   p->s = size_after_removing_one(p->s);
   return &p->l;
}

static inline node_t **deleteR(node_t *p, u64 *i) {
   invariant(p != NULL);
   invariant(i != NULL);
   invariant(*i > p->s);
   *i = rank_in_right_subtree(*i, p->s);
   return &p->r;
}

// Allocator and persistence helpers are split into subheaders because they rely
// on node_t and are included here after the node layout is known.
#include "tree_arena.h"
#include "tree_persistence.h"

// ─────────────────────────────────────────────────────────────────────────────
// Tree operations — declarations (defined in tree.c)
// ─────────────────────────────────────────────────────────────────────────────

tree_t tree_clone(tree_t *tree);
tree_t tree_clone_physical(tree_t *tree);
bool tree_contains_shared_node(node_t *root);

// Base insert/delete/lookup/update (used by trees that don't override them)
void tree_insert_base(tree_t *tree, u64 i, u64 x);
u64 tree_delete_base(tree_t *tree, u64 i);
u64 tree_lookup(tree_t *tree, u64 i);
void tree_update_base(tree_t *tree, u64 i, u64 x);

// Internal insert/delete on subtree pointers
void tree_insert_internal(node_t **p, u64 s, u64 i, u64 x);
u64 tree_delete_internal(node_t **p, u64 s, u64 i);

// Split/join (base implementations). In non-persistent builds these move node
// ownership out of the input tree handles; in persistent builds they preserve
// the inputs by sharing and copy-on-write.
typedef struct {
   node_t *left;
   node_t *right;
} tree_split_result_t;

typedef struct {
   tree_t left;
   tree_t right;
} tree_split_t;

typedef struct {
   node_t *left;
   node_t *right;
} tree_join_roots_t;

tree_split_result_t tree_split_roots(node_t *root, u64 split_size);
tree_split_result_t tree_split_roots_sized(node_t *root, u64 split_size,
                                           u64 original_size);
tree_split_t tree_split_base(tree_t *tree, u64 i);
tree_join_roots_t tree_prepare_join_roots(tree_t *left, tree_t *right);
tree_t tree_join_base(tree_t *tree, tree_t *other);
node_t *tree_join_internal(node_t *l, node_t *r, u64 sl, u64 sr);
#if defined(PERSISTENT) || defined(DEBUG)
bool tree_roots_overlap(tree_t *left, tree_t *right);
#endif
#ifdef DEBUG
void tree_debug_require_join_inputs(node_t *left, node_t *right, u64 left_size,
                                    u64 right_size);
#endif

// Dissolve (replace a node with one of its children or a neighbor)
node_t *tree_dissolve(node_t *p, u64 s);

// Delete min/max from subtree
node_t *tree_deleteMin(node_t **p);
node_t *tree_deleteMax(node_t **p);
node_t *tree_deleteMin_sized(node_t **p, u64 size);
node_t *tree_deleteMax_sized(node_t **p, u64 size);

// Partition
node_t *tree_partition_sized(node_t *p, u64 s, u64 i);
node_t *tree_partition(node_t *p, u64 i);

// Inline-backed stack storage grows the same way across traversal, verifier,
// and persistence helpers.
static inline void *BST_RETURNS_NONNULL stack_array_grow(
    void *data, const void *inline_data, u64 len, u64 *cap, size_t elem_size) {
   invariant(data != NULL);
   invariant(inline_data != NULL);
   invariant(cap != NULL);
   invariant(elem_size > 0);
   invariant(len == *cap);
   invariant(*cap > 0);
   invariant(len < UINT64_MAX);
   u64 old_cap = *cap;
   u64 new_cap = bst_array_next_cap(old_cap, size_after_inserting_one(len),
                                    old_cap, elem_size);
   invariant(new_cap > old_cap);
   invariant(new_cap > len);

   void *next;
   if ((const void *)data == inline_data) {
      next = bst_malloc_array(new_cap, elem_size);
      bst_array_copy(next, inline_data, len, elem_size);
   } else {
      next = bst_realloc_array(data, new_cap, elem_size);
   }

   *cap = new_cap;
   return next;
}

// Inorder traversal
typedef struct {
   node_t **data;
   u64 len;
   u64 cap;
} node_stack_t;

enum {
   NODE_STACK_INLINE_CAP = 128,
};

void node_stack_grow(node_stack_t *stack, node_t **inline_data);

static inline void node_stack_push(node_stack_t *stack, node_t **inline_data,
                                   node_t *node) {
   invariant(stack != NULL);
   invariant(inline_data != NULL);
   invariant(stack->data != NULL);
   invariant(stack->cap > 0);
   invariant(stack->len <= stack->cap);
   if (node == NULL) {
      return;
   }
   if (stack->len == stack->cap) {
      node_stack_grow(stack, inline_data);
   }
   invariant(stack->len < stack->cap);
   u64 index = stack->len;
   stack->len = size_after_inserting_one(stack->len);
   stack->data[index] = node;
}

static inline node_t *node_stack_pop(node_stack_t *stack) {
   invariant(stack != NULL);
   invariant(stack->data != NULL);
   invariant(stack->len <= stack->cap);
   invariant(stack->len > 0);
   stack->len = size_after_removing_one(stack->len);
   return stack->data[stack->len];
}

static inline void node_stack_free(node_stack_t *stack, node_t **inline_data) {
   invariant(stack != NULL);
   invariant(inline_data != NULL);
   invariant(stack->data != NULL);
   invariant(stack->cap > 0);
   invariant(stack->len <= stack->cap);
   invariant(stack->data == inline_data || stack->cap > NODE_STACK_INLINE_CAP);
   if (stack->data != inline_data) {
      free(stack->data);
   }
   stack->data = NULL;
   stack->len = 0;
   stack->cap = 0;
}

enum {
   NODE_SEEN_INLINE_CAP = 64,
};

typedef struct {
   node_t *inline_slots[NODE_SEEN_INLINE_CAP];
   node_t **slots;
   u64 len;
   u64 cap;
} node_seen_t;

static inline u64 node_pointer_hash(node_t *node) {
   invariant(node != NULL);
   u64 x = (u64)(uintptr_t)node;
   x ^= x >> 33;
   x *= 0xff51afd7ed558ccdULL;
   x ^= x >> 33;
   x *= 0xc4ceb9fe1a85ec53ULL;
   x ^= x >> 33;
   return x;
}

static inline void node_seen_init(node_seen_t *seen) {
   invariant(seen != NULL);
   invariant(seen->slots == NULL);
   invariant(seen->len == 0);
   invariant(seen->cap == 0);

   bst_array_zero(seen->inline_slots, NODE_SEEN_INLINE_CAP,
                  sizeof(seen->inline_slots[0]));
   seen->slots = seen->inline_slots;
   seen->cap = NODE_SEEN_INLINE_CAP;
}

static inline void node_seen_require_table(const node_seen_t *seen) {
   invariant(seen != NULL);
   invariant(seen->slots != NULL);
   invariant(seen->cap > 1);
   invariant((seen->cap & (seen->cap - 1)) == 0);
   invariant(seen->len < seen->cap);
   invariant(seen->slots != seen->inline_slots ||
             seen->cap == NODE_SEEN_INLINE_CAP);
   invariant(seen->slots == seen->inline_slots ||
             seen->cap > NODE_SEEN_INLINE_CAP);
}

static inline bool node_seen_insert_raw(node_seen_t *seen, node_t *node) {
   node_seen_require_table(seen);
   invariant(node != NULL);

   u64 mask = seen->cap - 1;
   u64 i = node_pointer_hash(node) & mask;
   u64 probes = 0;
   for (;;) {
      invariant(probes < seen->cap);
      if (seen->slots[i] == NULL) {
         seen->slots[i] = node;
         seen->len = size_after_inserting_one(seen->len);
         return true;
      }
      if (seen->slots[i] == node) {
         return false;
      }
      i = (i + 1) & mask;
      probes = size_after_inserting_one(probes);
   }
}

static inline bool node_seen_contains(const node_seen_t *seen, node_t *node) {
   invariant(seen != NULL);
   invariant(node != NULL);
   if (seen->slots == NULL) {
      invariant(seen->len == 0);
      invariant(seen->cap == 0);
      return false;
   }
   node_seen_require_table(seen);

   u64 mask = seen->cap - 1;
   u64 i = node_pointer_hash(node) & mask;
   u64 probes = 0;
   for (;;) {
      invariant(probes < seen->cap);
      if (seen->slots[i] == NULL) {
         return false;
      }
      if (seen->slots[i] == node) {
         return true;
      }
      i = (i + 1) & mask;
      probes = size_after_inserting_one(probes);
   }
}

static inline void node_seen_grow(node_seen_t *seen) {
   node_seen_require_table(seen);
   invariant(seen->cap <= UINT64_MAX / 2);
   invariant(seen->cap <= bst_array_max_count(sizeof(seen->slots[0])) / 2);

   u64 next_cap = size_sum(seen->cap, seen->cap);
   node_seen_t grown = {
       .cap = next_cap,
   };
   grown.slots = (node_t **)bst_calloc_array(grown.cap, sizeof(grown.slots[0]));
   for (u64 i = 0; i < seen->cap; i++) {
      if (seen->slots[i] != NULL) {
         bool inserted = node_seen_insert_raw(&grown, seen->slots[i]);
         invariant(inserted);
      }
   }
   invariant(grown.len == seen->len);

   if (seen->slots == seen->inline_slots) {
      bst_array_zero(seen->inline_slots, NODE_SEEN_INLINE_CAP,
                     sizeof(seen->inline_slots[0]));
   } else {
      free(seen->slots);
   }
   seen->slots = grown.slots;
   seen->len = grown.len;
   seen->cap = grown.cap;
}

static inline void node_seen_insert(node_seen_t *seen, node_t *node) {
   invariant(seen != NULL);
   invariant(node != NULL);
   if (seen->slots == NULL) {
      node_seen_init(seen);
   }
   node_seen_require_table(seen);
   u64 grow_at = seen->cap - seen->cap / 4;
   if (seen->len >= grow_at) {
      node_seen_grow(seen);
   }
   node_seen_require_table(seen);
   invariant(seen->len < seen->cap - 1);
   bool inserted = node_seen_insert_raw(seen, node);
   invariant(inserted);
   node_seen_require_table(seen);
}

static inline void node_seen_dispose(node_seen_t *seen) {
   invariant(seen != NULL);
   if (seen->slots != NULL) {
      node_seen_require_table(seen);
      if (seen->slots != seen->inline_slots) {
         free(seen->slots);
      }
   } else {
      invariant(seen->len == 0);
      invariant(seen->cap == 0);
   }
   bst_array_zero(seen->inline_slots, NODE_SEEN_INLINE_CAP,
                  sizeof(seen->inline_slots[0]));
   seen->slots = NULL;
   seen->len = 0;
   seen->cap = 0;
}

void inorder(node_t *p, void (*visit)(u64, void *), void *ctx);

// Verification
typedef void (*tree_verify_node_fn)(node_t *node, void *ctx);

void tree_verify_each_node(node_t *p, tree_verify_node_fn verify, void *ctx);
u64 tree_verify_size(node_t *p, u64 s);
u64 tree_verify_size_each_node(node_t *p, u64 s, tree_verify_node_fn verify,
                               void *ctx);
u64 tree_verify_reachable_size(node_t *p);
u64 tree_verify_reachable_size_each_node(node_t *p, tree_verify_node_fn verify,
                                         void *ctx);

static inline void tree_debug_require_size(node_t *root, u64 size) {
#ifdef DEBUG
   assert_bst(tree_verify_size(root, size) == size);
#else
   (void)root;
   (void)size;
#endif
}

static inline void tree_debug_require_reachable_size(node_t *root, u64 size) {
#ifdef DEBUG
   assert_bst(tree_verify_reachable_size(root) == size);
#else
   (void)root;
   (void)size;
#endif
}

typedef struct {
   u64 nodes;
   u64 total_depth;
   u64 maximum_depth;
} node_path_metrics_t;

// Path metrics (O(n), iterative)
node_path_metrics_t nodePathMetrics(node_t *p);
bool nodeHeightExceeds(node_t *p, int maximum_height);
node_path_metrics_t tree_verify_size_path_metrics(node_t *p, u64 s);

static inline u64 nodePathMetricsMaximumNodesForHeight(u64 maximum_depth) {
   if (maximum_depth >= 63) {
      return UINT64_MAX;
   }
   return (((u64)1) << (maximum_depth + 1)) - 1;
}

static inline u64 nodePathMetricsSpineExtraCapacityAtDepth(u64 depth) {
   invariant(depth > 0);
   if (depth >= 64) {
      return UINT64_MAX;
   }
   return (((u64)1) << depth) - 1;
}

static inline __uint128_t nodePathMetricsPathTotalDepth(u64 nodes) {
   invariant(nodes > 0);
   return ((__uint128_t)nodes * (__uint128_t)rank_of_last(nodes)) / 2;
}

static inline __uint128_t
nodePathMetricsMinimumTotalDepthForHeight(u64 nodes, u64 maximum_depth) {
   invariant(nodes > 0);
   invariant(maximum_depth < nodes);

   u64 path_nodes = size_after_inserting_one(maximum_depth);
   invariant(path_nodes <= nodes);
   __uint128_t total = nodePathMetricsPathTotalDepth(path_nodes);

   u64 remaining = nodes - path_nodes;
   u64 depth = 1;

   while (remaining > 0) {
      invariant(depth <= maximum_depth);
      u64 extra_capacity_at_depth =
          nodePathMetricsSpineExtraCapacityAtDepth(depth);
      u64 take = MIN(remaining, extra_capacity_at_depth);
      total += (__uint128_t)depth * (__uint128_t)take;
      remaining -= take;
      if (remaining == 0) {
         break;
      }

      invariant(depth < maximum_depth);
      depth = size_after_inserting_one(depth);
   }

   return total;
}

static inline __uint128_t
nodePathMetricsPerfectTotalDepthForHeight(u64 maximum_depth) {
   invariant(maximum_depth < 63);

   __uint128_t total = 0;
   for (u64 depth = 1; depth <= maximum_depth; depth++) {
      total += (__uint128_t)depth * (((__uint128_t)1) << depth);
   }
   return total;
}

static inline __uint128_t
nodePathMetricsMaximumTotalDepthForHeight(u64 nodes, u64 maximum_depth) {
   invariant(nodes > 0);
   invariant(maximum_depth < nodes);
   invariant(nodes <= nodePathMetricsMaximumNodesForHeight(maximum_depth));

   u64 remaining_nodes = nodes;
   u64 remaining_height = maximum_depth;
   __uint128_t total = 0;

   for (;;) {
      invariant(remaining_nodes > 0);
      invariant(remaining_nodes <=
                nodePathMetricsMaximumNodesForHeight(remaining_height));

      u64 path_nodes = size_after_inserting_one(remaining_height);
      if (remaining_nodes <= path_nodes) {
         total += nodePathMetricsPathTotalDepth(remaining_nodes);
         return total;
      }

      if (remaining_height > 62) {
         u64 skipped = remaining_height - 62;
         __uint128_t first = rank_of_last(remaining_nodes);
         __uint128_t last = remaining_nodes - skipped;
         total += ((__uint128_t)skipped * (first + last)) / 2;
         remaining_nodes -= skipped;
         remaining_height -= skipped;
         continue;
      }

      invariant(remaining_height > 0);
      u64 child_height = size_after_removing_one(remaining_height);
      u64 child_capacity = nodePathMetricsMaximumNodesForHeight(child_height);
      u64 child_nodes = size_after_removing_one(remaining_nodes);
      total += child_nodes;

      if (child_nodes <= child_capacity) {
         remaining_nodes = child_nodes;
         remaining_height = child_height;
         continue;
      }

      total += nodePathMetricsPerfectTotalDepthForHeight(child_height);
      remaining_nodes = child_nodes - child_capacity;
      remaining_height = child_height;
   }
}

static inline void nodePathMetricsRequire(node_path_metrics_t metrics) {
   if (metrics.nodes == 0) {
      invariant(metrics.total_depth == 0);
      invariant(metrics.maximum_depth == 0);
      return;
   }

   invariant(metrics.maximum_depth < metrics.nodes);
   invariant(metrics.maximum_depth <= metrics.total_depth);

   if (metrics.nodes == 1) {
      invariant(metrics.total_depth == 0);
      invariant(metrics.maximum_depth == 0);
      return;
   }

   invariant(metrics.maximum_depth > 0);
   invariant(metrics.nodes <=
             nodePathMetricsMaximumNodesForHeight(metrics.maximum_depth));
   __uint128_t minimum_total_depth = nodePathMetricsMinimumTotalDepthForHeight(
       metrics.nodes, metrics.maximum_depth);
   invariant((__uint128_t)metrics.total_depth >= minimum_total_depth);
   __uint128_t maximum_total_depth = nodePathMetricsMaximumTotalDepthForHeight(
       metrics.nodes, metrics.maximum_depth);
   invariant(maximum_total_depth <=
             nodePathMetricsPathTotalDepth(metrics.nodes));
   invariant((__uint128_t)metrics.total_depth <= maximum_total_depth);
}

static inline double
nodeAveragePathLengthFromMetrics(node_path_metrics_t metrics) {
   nodePathMetricsRequire(metrics);

   if (metrics.nodes == 0) {
      return 0.0;
   }
   return (double)metrics.total_depth / (double)metrics.nodes;
}

static inline int
nodeMaximumPathLengthFromMetrics(node_path_metrics_t metrics) {
   nodePathMetricsRequire(metrics);

   if (metrics.nodes == 0) {
      return -1;
   }
   invariant(metrics.maximum_depth <= (u64)INT_MAX);
   return (int)metrics.maximum_depth;
}

// Height (O(n))
int height(node_t *p);

// Maximum path length (same as height)
static inline int nodeMaximumPathLength(node_t *p) {
   return height(p);
}

// Average path length (O(n))
double nodeAveragePathLength(node_t *p);

// Node count (O(n))
u64 node_size(node_t *p);

// Tree rotations that persist children first. The Provisional variants skip
// whole-subtree verification for top-down algorithms in transient states.
void treeRotateL(node_t **p);
void treeRotateR(node_t **p);
void treeRotateRL(node_t **p);
void treeRotateLR(node_t **p);
void treeRotateLSized(node_t **p, u64 size);
void treeRotateRSized(node_t **p, u64 size);
void treeRotateRLSized(node_t **p, u64 size);
void treeRotateLRSized(node_t **p, u64 size);
void treeRotateLProvisional(node_t **p);
void treeRotateRProvisional(node_t **p);
void treeRotateRLProvisional(node_t **p);
void treeRotateLRProvisional(node_t **p);

// Raw extreme-splice helpers. These do not adjust cached sizes on the path;
// callers must repair affected nodes before exposing the tree again.
void tree_appendR_raw(node_t **p, node_t *n);
void tree_appendL_raw(node_t **p, node_t *n);

// replacedBy helpers (for top-down delete)
node_t *tree_replacedByRightSubtree(node_t **p);
node_t *tree_replacedByLeftSubtree(node_t **p);
node_t *tree_replacedByRightSubtree_sized(node_t **p, u64 size);
node_t *tree_replacedByLeftSubtree_sized(node_t **p, u64 size);

// pathLeft/pathRight (triple-pointer path traversal for top-down insert)
void tree_pathLeft(node_t ***p);
void tree_pathRight(node_t ***p, u64 *i);
void tree_pathLeftSized(node_t ***p, u64 *size);
void tree_pathRightSized(node_t ***p, u64 *size, u64 *i);

// attach helpers (allocate + link)
void tree_attach(node_t **p, u64 x);
void tree_attachL(node_t *p, u64 x);
void tree_attachR(node_t *p, u64 x);
void tree_attachLL(node_t *p, u64 x);
void tree_attachRR(node_t *p, u64 x);
void tree_attachLR(node_t *p, u64 x);
void tree_attachRL(node_t *p, u64 x);

// ─────────────────────────────────────────────────────────────────────────────
// Utility math
// ─────────────────────────────────────────────────────────────────────────────

// ⌊log₂(x)⌋ when x > 0, otherwise 0
static inline u64 util_log2(u64 x) {
   if (x == 0) {
      return 0;
   }
   return (u64)(63 - __builtin_clzll(x));
}

// Equivalent to y == 0 || (x > 0 && floor(log2(x)) >= floor(log2(y))).
static inline bool util_gte_msb(u64 x, u64 y) {
   bool result = x >= (~x & y);
#ifdef DEBUG
   bool expected = y == 0 || (x > 0 && util_log2(x) >= util_log2(y));
   assert_bst(result == expected);
#endif
   return result;
}

// ⌊2 log₂(size)⌋, exact for all u64 sizes.
int util_redblack_height_bound(u64 size);

// Largest AVL height whose minimal node count fits in size.
int util_avl_height_bound(u64 size);

#endif // BST_H
