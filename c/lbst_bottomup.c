#include "lbst_bottomup.h"
#include "wbst_bottomup.h"
#include "tree_list_adapter.h"

// LBSTBottomUp (LogSize weight balance)

static void lbst_bu_insert_op(lbst_bottomup_t *t, u64 i, u64 x) {
   invariant(t != NULL);
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   t->tree.root = wbst_bu_insert(t->wb, t->tree.root, t->tree.size, i, x);
   tree_list_sized_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
}

static u64 lbst_bu_delete_op(lbst_bottomup_t *t, u64 i) {
   invariant(t != NULL);
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   wbst_bu_delete_result_t result =
       wbst_bu_delete(t->wb, t->tree.root, t->tree.size, i);
   t->tree.root = result.root;
   tree_list_sized_deleted(&t->tree);
   invariant(t->tree.size == deleted_size);
   return result.value;
}

static void lbst_bu_verify_tree(lbst_bottomup_t *t) {
   invariant(t != NULL);
   wbst_bu_verifyBalance(t->wb, t->tree.root, t->tree.size);
}

WB_VT_BODY(
    lbst_bottomup_t, lbst_bu, wb_log_size,
    { lbst_bu_insert_op((lbst_bottomup_t *)self->impl, i, x); },
    { return lbst_bu_delete_op((lbst_bottomup_t *)self->impl, i); },
    { WBST_BU_SPLIT(lbst_bottomup_t); }, { WBST_BU_JOIN(lbst_bottomup_t); },
    { lbst_bu_verify_tree((lbst_bottomup_t *)self->impl); })

const list_vtable_t lbst_bottomup_vtable =
    TREE_LIST_VTABLE("LBSTBottomUp", lbst_bu);
