#ifndef REDBLACK_BOTTOMUP_H
#define REDBLACK_BOTTOMUP_H
#include "bst.h"
#include "list.h"
#include "rank.h"
typedef struct {
   tree_t tree;
} redblack_bottomup_t;
extern const list_vtable_t redblack_bottomup_vtable;

typedef struct {
   node_t *root;
   u64 value;
} rb_bu_delete_result_t;

// Shared RB functions
node_t *rb_balanceInsertL(node_t *p, u64 size);
node_t *rb_balanceInsertR(node_t *p, u64 size);
node_t *rb_balanceDeleteL(node_t *p, u64 size);
node_t *rb_balanceDeleteR(node_t *p, u64 size);
node_t *rb_bu_insert(node_t *p, u64 size, u64 i, u64 x);
rb_bu_delete_result_t rb_bu_delete(node_t *p, u64 s, u64 i);
node_t *rb_build(node_t *l, node_t *p, node_t *r, u64 sl, u64 sr);
node_t *rb_join(node_t *l, node_t *r, u64 sl, u64 sr);
tree_split_result_t rb_split(node_t *root, u64 size, u64 split_size);
#endif
