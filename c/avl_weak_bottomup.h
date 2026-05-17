#ifndef AVL_WEAK_BOTTOMUP_H
#define AVL_WEAK_BOTTOMUP_H
#include "bst.h"
#include "list.h"
typedef struct { tree_t tree; } avl_weak_bottomup_t;
extern const list_vtable_t avl_weak_bottomup_vtable;

// Shared functions used by AVLWeakTopDown and AVLRelaxedTopDown
node_t *avlw_build(node_t *l, node_t *p, node_t *r, u64 sl);
node_t *avlw_join(node_t *l, node_t *r, u64 sl, u64 sr);
void    avlw_split(node_t *p, u64 i, u64 s, node_t **out_l, node_t **out_r);
#endif
