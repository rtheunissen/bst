#ifndef AVL_WEAK_TOPDOWN_H
#define AVL_WEAK_TOPDOWN_H
#include "bst.h"
#include "list.h"
typedef struct { tree_t tree; } avl_weak_topdown_t;
extern const list_vtable_t avl_weak_topdown_vtable;

// Insert used by AVLRelaxedTopDown
void avlw_td_insert(node_t **p, u64 i, u64 x);
#endif
