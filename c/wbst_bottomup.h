#ifndef WBST_BOTTOMUP_H
#define WBST_BOTTOMUP_H
#include "bst.h"
#include "list.h"
#include "weight.h"
typedef struct {
   tree_t tree;
   weight_balance_t wb;
} wbst_bottomup_t;
extern const list_vtable_t wbst_bottomup_vtable;

typedef struct {
   node_t *root;
   u64 value;
} wbst_bu_delete_result_t;

// Shared WBST functions (used by LBST variants)
node_t *wbst_bu_insert(weight_balance_t wb, node_t *p, u64 s, u64 i, u64 x);
wbst_bu_delete_result_t wbst_bu_delete(weight_balance_t wb, node_t *p, u64 s,
                                       u64 i);
node_t *wbst_bu_join(weight_balance_t wb, node_t *l, node_t *r, u64 sl, u64 sr);
node_t *wbst_bu_build(weight_balance_t wb, node_t *l, node_t *p, node_t *r,
                      u64 sl, u64 sr);
tree_split_result_t wbst_bu_split(weight_balance_t wb, node_t *root,
                                  u64 split_size, u64 size);
void wbst_verifyBalance(weight_balance_t wb, node_t *p, u64 s, u64 min_size);
void wbst_bu_verifyBalance(weight_balance_t wb, node_t *p, u64 s);
#endif
