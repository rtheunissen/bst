#include "avl_relaxed_bottomup.h"
#include "avl_bottomup.h"
#include "rank.h"
#include "tree_list_adapter.h"

// Forward declarations
static node_t *avlr_buildL(node_t *l, node_t *p, node_t *r, u64 sl, u64 sr);
static node_t *avlr_buildR(node_t *l, node_t *p, node_t *r, u64 sl, u64 sr);

// AVLRelaxedBottomUp  (uses AVLBottomUp insert, base Tree delete)
// ═══════════════════════════════════════════════════════════════════════════════

static node_t *avlr_buildL(node_t *l, node_t *p, node_t *r, u64 sl, u64 sr) {
   u64 size = tree_build_inputs_require(l, p, r, sl, sr);
   int rl = rank_of(l);
   int rr = rank_of(r);
   if (rank_at_most_one_above(rl, rr)) {
      p->l = l;
      p->r = r;
      p->s = sl;
      p->y = rank_above(rl);
      return p;
   }
   invariant(l != NULL);
   invariant(l->s < sl);
   tree_persist_unshared(&l);
   u64 left_right_size = sizeR(l, sl);
   l->r = avlr_build(l->r, p, r, left_right_size, sr);
   return avl_balanceInsertR(l, size);
}

static node_t *avlr_buildR(node_t *l, node_t *p, node_t *r, u64 sl, u64 sr) {
   u64 size = tree_build_inputs_require(l, p, r, sl, sr);
   int rl = rank_of(l);
   int rr = rank_of(r);
   if (rank_at_most_one_above(rr, rl)) {
      p->l = l;
      p->r = r;
      p->s = sl;
      p->y = rank_above(rr);
      return p;
   }
   invariant(r != NULL);
   tree_persist_unshared(&r);
   u64 right_left_size = sizeL(r);
   r->s = tree_size_from_child_sizes(sl, right_left_size);
   r->l = avlr_build(l, p, r->l, sl, right_left_size);
   return avl_balanceInsertL(r, size);
}

node_t *avlr_build(node_t *l, node_t *p, node_t *r, u64 sl, u64 sr) {
   tree_build_inputs_require(l, p, r, sl, sr);
   tree_debug_require_size(l, sl);
   tree_debug_require_size(r, sr);
   if (rank_of(l) <= rank_of(r)) {
      return avlr_buildR(l, p, r, sl, sr);
   }
   return avlr_buildL(l, p, r, sl, sr);
}

node_t *avlr_join(node_t *l, node_t *r, u64 sl, u64 sr) {
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
      return avlr_build(l, p, r, sl, size_after_removing_one(sr));
   } else {
      invariant(sl > 0);
      node_t *p = tree_deleteMax_sized(&l, sl);
      invariant(p != NULL);
      return avlr_build(l, p, r, rank_of_last(sl), sr);
   }
}

static tree_split_result_t avlr_split_impl(node_t *root, u64 split_size,
                                           u64 size) {
   (void)tree_split_inputs_require(root, split_size, size);
   if (root == NULL) {
      return (tree_split_result_t){.left = NULL, .right = NULL};
   }
   tree_persist_unshared(&root);
   u64 left_size = sizeL(root);
   u64 right_size = sizeR(root, size);
   if (split_size <= root->s) {
      tree_split_result_t split =
          avlr_split_impl(root->l, split_size, left_size);

      u64 right_split_size = suffix_size_after_prefix(left_size, split_size);
      return (tree_split_result_t){
          .left = split.left,
          .right = avlr_build(split.right, root, root->r, right_split_size,
                              right_size),
      };
   }

   u64 right_rank = rank_in_right_subtree(split_size, left_size);
   tree_split_result_t split = avlr_split_impl(root->r, right_rank, right_size);
   return (tree_split_result_t){
       .left = avlr_build(root->l, root, split.left, left_size, right_rank),
       .right = split.right,
   };
}

tree_split_result_t avlr_split(node_t *root, u64 split_size, u64 size) {
   (void)tree_split_inputs_require(root, split_size, size);
   if (root != NULL) {
      tree_debug_require_size(root, size);
   }
   return avlr_split_impl(root, split_size, size);
}

TREE_VT_SIMPLE(avl_relaxed_bottomup_t, avlr_bu)

static void avlr_bu_vt_verify(list_t *self) {
   list_require(self);
   avl_relaxed_bottomup_t *t = (avl_relaxed_bottomup_t *)self->impl;
   verifyAVLRelaxedRanksSized(t->tree.root, t->tree.size);
}
static void avlr_bu_vt_insert(list_t *self, u64 i, u64 x) {
   list_require(self);
   avl_relaxed_bottomup_t *t = (avl_relaxed_bottomup_t *)self->impl;
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   t->tree.root = avl_bu_insert_op(t->tree.root, size, i, x);
   tree_list_sized_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
}
static u64 avlr_bu_vt_delete(list_t *self, u64 i) {
   list_require(self);
   avl_relaxed_bottomup_t *t = (avl_relaxed_bottomup_t *)self->impl;
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   u64 x = tree_delete_base(&t->tree, i);
   invariant(t->tree.size == deleted_size);
   return x;
}
static void avlr_bu_vt_split(list_t *self, u64 i, list_t *out_l,
                             list_t *out_r) {
   tree_list_require_split(self, i, out_l, out_r);
   avl_relaxed_bottomup_t *t = (avl_relaxed_bottomup_t *)self->impl;
   u64 size = t->tree.size;
   u64 right_size = tree_split_inputs_require(t->tree.root, i, size);
   tree_share(t->tree.root);
   tree_split_result_t split = avlr_split(t->tree.root, i, size);
   TREE_LIST_SPLIT_RESULT_PREPARED(avl_relaxed_bottomup_t, size, split.left, i,
                                   split.right, right_size);
}
static list_t avlr_bu_vt_join(list_t *self, list_t *other) {
   tree_list_require_join(self, other);
   avl_relaxed_bottomup_t *t = (avl_relaxed_bottomup_t *)self->impl;
   avl_relaxed_bottomup_t *that = (avl_relaxed_bottomup_t *)other->impl;
   tree_join_roots_t roots =
       tree_list_prepare_join_roots(&t->tree, &that->tree);
   node_t *root =
       avlr_join(roots.left, roots.right, t->tree.size, that->tree.size);
   u64 joined_size = tree_list_join_size(t->tree.size, that->tree.size);
   TREE_LIST_JOIN_RESULT(avl_relaxed_bottomup_t, root, joined_size);
}
const list_vtable_t avl_relaxed_bottomup_vtable =
    TREE_LIST_VTABLE("AVLRelaxedBottomUp", avlr_bu);

// ═══════════════════════════════════════════════════════════════════════════════
