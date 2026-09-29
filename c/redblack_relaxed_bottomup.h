#ifndef REDBLACK_RELAXED_BOTTOMUP_H
#define REDBLACK_RELAXED_BOTTOMUP_H
#include "bst.h"
#include "list.h"
typedef struct {
   tree_t tree;
} redblack_relaxed_bottomup_t;
extern const list_vtable_t redblack_relaxed_bottomup_vtable;

node_t *rb_relaxed_join(node_t *l, node_t *r, u64 sl, u64 sr);
#endif
