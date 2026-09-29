#include "redblack_relaxed_bottomup.h"
#include "redblack_bottomup.h"
#include "tree_list_adapter.h"

// RedBlackRelaxedBottomUp (rb insert, base delete, own join with base deleteMin/Max)
// ═══════════════════════════════════════════════════════════════════════════════

node_t *rb_relaxed_join(node_t *l, node_t *r, u64 sl, u64 sr) {
   tree_join_inputs_require(l, r, sl, sr);
#ifdef DEBUG
   tree_debug_require_join_inputs(l, r, sl, sr);
#endif

   if (l == NULL) {
      invariant(sl == 0);
      return r;
   }
   if (r == NULL) {
      invariant(sr == 0);
      return l;
   }

   if (rank_of(l) < rank_of(r) || (rank_of(l) == rank_of(r) && sl <= sr)) {
      node_t *p = tree_deleteMin_sized(&r, sr);
      invariant(p != NULL);
      return rb_build(l, p, r, sl, size_after_removing_one(sr));
   } else {
      invariant(sl > 0);
      node_t *p = tree_deleteMax_sized(&l, sl);
      invariant(p != NULL);
      return rb_build(l, p, r, rank_of_last(sl), sr);
   }
}

TREE_VT_SIMPLE(redblack_relaxed_bottomup_t, rbr_bu)

static void rbr_bu_vt_verify(list_t *self) {
   list_require(self);
   redblack_relaxed_bottomup_t *t = (redblack_relaxed_bottomup_t *)self->impl;
   verifyRedBlackRelaxedSized(t->tree.root, t->tree.size);
}

static void rbr_bu_vt_insert(list_t *self, u64 i, u64 x) {
   list_require(self);
   redblack_relaxed_bottomup_t *t = (redblack_relaxed_bottomup_t *)self->impl;
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   t->tree.root = rb_bu_insert(t->tree.root, size, i, x);
   tree_list_sized_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
}

static u64 rbr_bu_vt_delete(list_t *self, u64 i) {
   list_require(self);
   redblack_relaxed_bottomup_t *t = (redblack_relaxed_bottomup_t *)self->impl;
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   u64 x = tree_delete_base(&t->tree, i);
   invariant(t->tree.size == deleted_size);
   return x;
}

static void rbr_bu_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   tree_list_require_split(self, i, out_l, out_r);
   redblack_relaxed_bottomup_t *t = (redblack_relaxed_bottomup_t *)self->impl;
   u64 size = t->tree.size;
   u64 right_size = tree_split_inputs_require(t->tree.root, i, size);
   tree_share(t->tree.root);
   tree_split_result_t split = rb_split(t->tree.root, size, i);
   TREE_LIST_SPLIT_RESULT_PREPARED(redblack_relaxed_bottomup_t, size,
                                   split.left, i, split.right, right_size);
}

static list_t rbr_bu_vt_join(list_t *self, list_t *other) {
   tree_list_require_join(self, other);
   redblack_relaxed_bottomup_t *t = (redblack_relaxed_bottomup_t *)self->impl;
   redblack_relaxed_bottomup_t *that =
       (redblack_relaxed_bottomup_t *)other->impl;
   tree_join_roots_t roots =
       tree_list_prepare_join_roots(&t->tree, &that->tree);
   node_t *root =
       rb_relaxed_join(roots.left, roots.right, t->tree.size, that->tree.size);
   u64 joined_size = tree_list_join_size(t->tree.size, that->tree.size);
   TREE_LIST_JOIN_RESULT(redblack_relaxed_bottomup_t, root, joined_size);
}

const list_vtable_t redblack_relaxed_bottomup_vtable =
    TREE_LIST_VTABLE("RedBlackRelaxedBottomUp", rbr_bu);

// ═══════════════════════════════════════════════════════════════════════════════
