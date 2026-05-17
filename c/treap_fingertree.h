#ifndef TREAP_FINGERTREE_H
#define TREAP_FINGERTREE_H
#include "bst.h"
#include "list.h"
#include "random.h"
typedef struct { tree_t tree; xoshiro256_t source; } treap_fingertree_t;
extern const list_vtable_t treap_fingertree_vtable;
#endif
