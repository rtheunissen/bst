#ifndef TREAP_TOPDOWN_H
#define TREAP_TOPDOWN_H

#include "bst.h"
#include "list.h"
#include "random.h"

typedef struct {
   tree_t tree;
   xoshiro256_t source;
} treap_topdown_t;

extern const list_vtable_t treap_topdown_vtable;

#endif // TREAP_TOPDOWN_H
