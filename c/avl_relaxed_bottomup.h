#ifndef AVL_RELAXED_BOTTOMUP_H
#define AVL_RELAXED_BOTTOMUP_H
#include "bst.h"
#include "list.h"
typedef struct {
   tree_t tree;
} avl_relaxed_bottomup_t;
extern const list_vtable_t avl_relaxed_bottomup_vtable;

// Shared functions used by AVLRelaxedTopDown
node_t *avlr_build(node_t *l, node_t *p, node_t *r, u64 sl, u64 sr);
node_t *avlr_join(node_t *l, node_t *r, u64 sl, u64 sr);
tree_split_result_t avlr_split(node_t *root, u64 split_size, u64 size);
#endif
