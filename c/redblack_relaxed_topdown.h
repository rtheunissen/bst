#ifndef REDBLACK_RELAXED_TOPDOWN_H
#define REDBLACK_RELAXED_TOPDOWN_H
#include "bst.h"
#include "list.h"
typedef struct {
   tree_t tree;
} redblack_relaxed_topdown_t;
extern const list_vtable_t redblack_relaxed_topdown_vtable;
#endif
