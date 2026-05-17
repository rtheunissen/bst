#ifndef REDBLACK_BOTTOMUP_H
#define REDBLACK_BOTTOMUP_H
#include "bst.h"
#include "list.h"
#include "rank.h"
typedef struct { tree_t tree; } redblack_bottomup_t;
extern const list_vtable_t redblack_bottomup_vtable;

// Shared RB functions
node_t *rb_bu_insert(node_t *p, u64 i, u64 x);
node_t *rb_bu_delete(node_t *p, u64 s, u64 i, u64 *x);
node_t *rb_build(node_t *l, node_t *p, node_t *r, u64 sl);
node_t *rb_join(node_t *l, node_t *r, u64 sl, u64 sr);
void    rb_split(node_t *p, u64 s, u64 i, node_t **out_l, node_t **out_r);
#endif
