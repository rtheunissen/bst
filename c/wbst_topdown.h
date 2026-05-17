#ifndef WBST_TOPDOWN_H
#define WBST_TOPDOWN_H
#include "bst.h"
#include "list.h"
#include "weight.h"
typedef struct { tree_t tree; weight_balance_t wb; } wbst_topdown_t;
extern const list_vtable_t wbst_topdown_vtable;

// Shared WBST topdown functions
void    wbst_td_insert(weight_balance_t wb, node_t **p, u64 s, u64 i, u64 x);
void    wbst_td_delete_internal(weight_balance_t wb, node_t **p, u64 s, u64 i, node_t **x);
node_t *wbst_td_build(weight_balance_t wb, node_t *l, node_t *p, node_t *r, u64 sl, u64 sr);
node_t *wbst_td_join(weight_balance_t wb, node_t *l, node_t *r, u64 sl, u64 sr);
void    wbst_td_split(weight_balance_t wb, node_t *p, u64 i, u64 s, node_t **out_l, node_t **out_r);
void    wbst_td_verifyBalance(weight_balance_t wb, node_t *p, u64 s);
#endif
