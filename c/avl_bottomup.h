#ifndef AVL_BOTTOMUP_H
#define AVL_BOTTOMUP_H
#include "bst.h"
#include "list.h"
#include "rank.h"

typedef struct {
   tree_t tree;
} avl_bottomup_t;
extern const list_vtable_t avl_bottomup_vtable;

typedef struct {
   node_t *root;
   u64 value;
} avl_bu_delete_result_t;

typedef struct {
   node_t *root;
   node_t *node;
} avl_extract_result_t;

// Shared AVL balancing functions (used by all AVL variants for build/join)
node_t *avl_balanceInsertL(node_t *p, u64 size);
node_t *avl_balanceInsertR(node_t *p, u64 size);
node_t *avl_balanceDeleteL(node_t *p, u64 size);
node_t *avl_balanceDeleteR(node_t *p, u64 size);
node_t *avl_buildL(node_t *l, node_t *p, node_t *r, u64 sl, u64 sr);
node_t *avl_buildR(node_t *l, node_t *p, node_t *r, u64 sl, u64 sr);
node_t *avl_build(node_t *l, node_t *p, node_t *r, u64 sl, u64 sr);
avl_extract_result_t avl_deleteMin(node_t *p, u64 size);
avl_extract_result_t avl_deleteMax(node_t *p, u64 size);
node_t *avl_joinL(node_t *l, node_t *r, u64 sl, u64 sr);
node_t *avl_joinR(node_t *l, node_t *r, u64 sl, u64 sr);
node_t *avl_join(node_t *l, node_t *r, u64 sl, u64 sr);
tree_split_result_t avl_split(node_t *root, u64 split_size, u64 size);

// AVLBottomUp insert/delete (used by variants)
node_t *avl_bu_insert_op(node_t *p, u64 size, u64 i, u64 x);
avl_bu_delete_result_t avl_bu_delete_op(node_t *p, u64 s, u64 i);

#endif
