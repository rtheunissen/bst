#ifndef TREAP_BOTTOMUP_H
#define TREAP_BOTTOMUP_H

#include "bst.h"
#include "list.h"
#include "random.h"

typedef struct {
   tree_t tree;
   xoshiro256_t source;
} treap_bottomup_t;

extern const list_vtable_t treap_bottomup_vtable;

#endif // TREAP_BOTTOMUP_H
