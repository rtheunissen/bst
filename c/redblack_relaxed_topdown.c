#include "redblack_relaxed_topdown.h"
#include "redblack_relaxed_bottomup.h"
#include "redblack_topdown.h"
#include "redblack_bottomup.h"
#include "tree_list_adapter.h"

// RedBlackRelaxedTopDown (td insert, base delete, own join with base deleteMin/Max)
// ═══════════════════════════════════════════════════════════════════════════════

TREE_VT_SIMPLE(redblack_relaxed_topdown_t, rbr_td)

static void rbr_td_vt_verify(list_t *self) {
   list_require(self);
   redblack_relaxed_topdown_t *t = (redblack_relaxed_topdown_t *)self->impl;
   verifyRedBlackRelaxedSized(t->tree.root, t->tree.size);
}

static void rbr_td_vt_insert(list_t *self, u64 i, u64 x) {
   list_require(self);
   redblack_relaxed_topdown_t *t = (redblack_relaxed_topdown_t *)self->impl;
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   rb_td_insert(&t->tree.root, size, i, x);
   tree_list_sized_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
}

static u64 rbr_td_vt_delete(list_t *self, u64 i) {
   list_require(self);
   redblack_relaxed_topdown_t *t = (redblack_relaxed_topdown_t *)self->impl;
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   u64 x = tree_delete_base(&t->tree, i);
   invariant(t->tree.size == deleted_size);
   return x;
}

static void rbr_td_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   tree_list_require_split(self, i, out_l, out_r);
   redblack_relaxed_topdown_t *t = (redblack_relaxed_topdown_t *)self->impl;
   u64 size = t->tree.size;
   u64 right_size = tree_split_inputs_require(t->tree.root, i, size);
   tree_share(t->tree.root);
   tree_split_result_t split = rb_split(t->tree.root, size, i);
   TREE_LIST_SPLIT_RESULT_PREPARED(redblack_relaxed_topdown_t, size, split.left,
                                   i, split.right, right_size);
}

static list_t rbr_td_vt_join(list_t *self, list_t *other) {
   tree_list_require_join(self, other);
   redblack_relaxed_topdown_t *t = (redblack_relaxed_topdown_t *)self->impl;
   redblack_relaxed_topdown_t *that = (redblack_relaxed_topdown_t *)other->impl;
   tree_join_roots_t roots =
       tree_list_prepare_join_roots(&t->tree, &that->tree);
   node_t *root =
       rb_relaxed_join(roots.left, roots.right, t->tree.size, that->tree.size);
   u64 joined_size = tree_list_join_size(t->tree.size, that->tree.size);
   TREE_LIST_JOIN_RESULT(redblack_relaxed_topdown_t, root, joined_size);
}

const list_vtable_t redblack_relaxed_topdown_vtable =
    TREE_LIST_VTABLE("RedBlackRelaxedTopDown", rbr_td);
