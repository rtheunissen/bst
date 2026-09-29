#ifndef REDBLACK_TOPDOWN_H
#define REDBLACK_TOPDOWN_H
#include "bst.h"
#include "list.h"
typedef struct {
   tree_t tree;
} redblack_topdown_t;
extern const list_vtable_t redblack_topdown_vtable;

void rb_td_insert(node_t **p, u64 size, u64 i, u64 x);
#endif
