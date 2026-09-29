#include "lbst_topdown.h"
#include "wbst_topdown.h"
#include "tree_list_adapter.h"

// LBSTTopDown (LogWeight weight balance)

static void lbst_td_insert_op(lbst_topdown_t *t, u64 i, u64 x) {
   invariant(t != NULL);
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   wbst_td_insert(t->wb, &t->tree.root, t->tree.size, i, x);
   tree_list_sized_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
}

static u64 lbst_td_delete_op(lbst_topdown_t *t, u64 i) {
   invariant(t != NULL);
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   node_t *deleted =
       wbst_td_delete_internal(t->wb, &t->tree.root, t->tree.size, i);
   invariant(deleted != NULL);
   tree_list_sized_deleted(&t->tree);
   invariant(t->tree.size == deleted_size);
   u64 x = deleted->x;
   tree_release(deleted);
   return x;
}

static void lbst_td_verify_tree(lbst_topdown_t *t) {
   invariant(t != NULL);
   wbst_td_verifyBalance(t->wb, t->tree.root, t->tree.size);
}

WB_VT_BODY(
    lbst_topdown_t, lbst_td, wb_log_weight,
    { lbst_td_insert_op((lbst_topdown_t *)self->impl, i, x); },
    { return lbst_td_delete_op((lbst_topdown_t *)self->impl, i); },
    { WBST_TD_SPLIT(lbst_topdown_t); }, { WBST_TD_JOIN(lbst_topdown_t); },
    { lbst_td_verify_tree((lbst_topdown_t *)self->impl); })

const list_vtable_t lbst_topdown_vtable =
    TREE_LIST_VTABLE("LBSTTopDown", lbst_td);
