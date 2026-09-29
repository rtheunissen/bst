#include "lbst_relaxed.h"
#include "wbst_relaxed.h"
#include "wbst_bottomup.h"
#include "tree_list_adapter.h"

typedef struct {
   tree_t left;
   tree_t right;
} lbst_rlx_split_result_t;

static void lbst_rlx_insert_op(lbst_relaxed_t *t, u64 i, u64 x) {
   invariant(t != NULL);
   wbst_rlx_insert(t->wb, &t->tree.root, t->tree.size, i, x, t->tree.size);
   tree_list_sized_inserted(&t->tree);
}

static u64 lbst_rlx_delete_op(lbst_relaxed_t *t, u64 i) {
   invariant(t != NULL);
   return wbst_rlx_delete(t->wb, &t->tree, i);
}

static lbst_rlx_split_result_t lbst_rlx_split_op(lbst_relaxed_t *t, u64 i) {
   invariant(t != NULL);
   tree_share(t->tree.root);
   tree_split_result_t split =
       tree_split_roots_sized(t->tree.root, i, t->tree.size);

   tree_t left = tree_list_make_sized_tree(split.left, i);
   u64 right_size = tree_list_split_right_size(t->tree.size, i);
   tree_t right = tree_list_make_sized_tree(split.right, right_size);

   wbst_rlx_rebalance_if_too_tall(t->wb, &left);
   wbst_rlx_rebalance_if_too_tall(t->wb, &right);
   return (lbst_rlx_split_result_t){.left = left, .right = right};
}

static tree_t lbst_rlx_join_op(lbst_relaxed_t *t, lbst_relaxed_t *that) {
   invariant(t != NULL);
   invariant(that != NULL);
   weight_balance_require_equal(t->wb, that->wb);
   tree_join_roots_t roots =
       tree_list_prepare_join_roots(&t->tree, &that->tree);
   u64 joined_size = tree_list_join_size(t->tree.size, that->tree.size);
   node_t *joined_root = wbst_rlx_join(t->wb, roots.left, roots.right,
                                       t->tree.size, that->tree.size);
   tree_t joined = tree_list_make_sized_tree(joined_root, joined_size);
   wbst_rlx_rebalance_if_too_tall(t->wb, &joined);
   return joined;
}

static void lbst_rlx_verify_tree(lbst_relaxed_t *t) {
   invariant(t != NULL);
   wbst_rlx_verify(t->wb, &t->tree);
}

WB_VT_BODY(
    lbst_relaxed_t, lbst_rlx, wb_log_weight,
    { lbst_rlx_insert_op((lbst_relaxed_t *)self->impl, i, x); },
    { return lbst_rlx_delete_op((lbst_relaxed_t *)self->impl, i); },
    {
       lbst_relaxed_t *t = (lbst_relaxed_t *)self->impl;
       u64 size = t->tree.size;
       lbst_rlx_split_result_t split = lbst_rlx_split_op(t, i);
       TREE_LIST_SPLIT_RESULT_FIELD_PREPARED(
           lbst_relaxed_t, size, wb, t->wb, split.left.root, split.left.size,
           split.right.root, split.right.size);
    },
    {
       lbst_relaxed_t *t = (lbst_relaxed_t *)self->impl;
       lbst_relaxed_t *that = (lbst_relaxed_t *)other->impl;
       tree_t joined = lbst_rlx_join_op(t, that);
       TREE_LIST_JOIN_RESULT_FIELD(lbst_relaxed_t, wb, t->wb, joined.root,
                                   joined.size);
    },
    { lbst_rlx_verify_tree((lbst_relaxed_t *)self->impl); })

const list_vtable_t lbst_relaxed_vtable =
    TREE_LIST_VTABLE("LBSTRelaxed", lbst_rlx);
