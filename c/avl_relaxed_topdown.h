#ifndef AVL_RELAXED_TOPDOWN_H
#define AVL_RELAXED_TOPDOWN_H
#include "bst.h"
#include "list.h"
typedef struct { tree_t tree; } avl_relaxed_topdown_t;
extern const list_vtable_t avl_relaxed_topdown_vtable;
#endif
