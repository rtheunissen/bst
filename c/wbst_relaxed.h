#ifndef WBST_RELAXED_H
#define WBST_RELAXED_H
#include "bst.h"
#include "list.h"
#include "weight.h"
typedef struct { tree_t tree; weight_balance_t wb; } wbst_relaxed_t;
extern const list_vtable_t wbst_relaxed_vtable;

// Shared relaxed functions
void    wbst_rlx_insert(weight_balance_t wb, node_t **p, u64 s, u64 i, u64 x, u64 tree_size);
node_t *wbst_rlx_join(weight_balance_t wb, node_t *l, node_t *r, u64 sl, u64 sr);
#endif
