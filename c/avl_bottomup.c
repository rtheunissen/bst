#include "avl_bottomup.h"
#include "tree_list_adapter.h"

// ─────────────────────────────────────────────────────────────────────────────
// Shared AVL balancing (used by ALL AVL variants)
// ─────────────────────────────────────────────────────────────────────────────

node_t *avl_balanceInsertL(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   if (!isZeroChild(p, p->l)) {
      return p;
   }
   if (isOneChild(p, p->r)) {
      promote(p);
      return p;
   }
   if (isTwoChild(p->l, p->l->r)) {
      treeRotateRSized(&p, size);
      demote(p->r);
      return p;
   }
   assert_bst(isOneChild(p->l, p->l->r));
   treeRotateLRSized(&p, size);
   promote(p);
   demote(p->l);
   demote(p->r);
   return p;
}

node_t *avl_balanceInsertR(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   if (isZeroChild(p, p->r)) {
      if (isOneChild(p, p->l)) {
         promote(p);
      } else if (isTwoChild(p->r, p->r->l)) {
         treeRotateLSized(&p, size);
         demote(p->l);
      } else {
         treeRotateRLSized(&p, size);
         promote(p);
         demote(p->l);
         demote(p->r);
      }
   }
   return p;
}

node_t *avl_balanceDeleteL(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   if (isTwoTwo(p)) {
      demote(p);
      return p;
   }
   if (isThreeChild(p, p->l)) {
      assert_bst(isOneChild(p, p->r));
      if (isTwoChild(p->r, p->r->r)) {
         treeRotateRLSized(&p, size);
         promote(p);
         demote(p->r);
         demote(p->l);
         demote(p->l);
         return p;
      }
      assert_bst(isOneChild(p->r, p->r->r));
      if (isTwoChild(p->r, p->r->l)) {
         treeRotateLSized(&p, size);
         demote(p->l);
         demote(p->l);
         return p;
      } else {
         assert_bst(isOneChild(p->r, p->r->l));
         assert_bst(isOneChild(p->r, p->r->r));
         treeRotateLSized(&p, size);
         promote(p);
         demote(p->l);
         return p;
      }
   }
   return p;
}

node_t *avl_balanceDeleteR(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   if (isTwoTwo(p)) {
      demote(p);
      return p;
   }
   if (isThreeChild(p, p->r)) {
      if (isTwoChild(p->l, p->l->l)) {
         treeRotateLRSized(&p, size);
         promote(p);
         demote(p->l);
         demote(p->r);
         demote(p->r);
      } else {
         if (isTwoChild(p->l, p->l->r)) {
            treeRotateRSized(&p, size);
            demote(p->r);
            demote(p->r);
         } else {
            treeRotateRSized(&p, size);
            promote(p);
            demote(p->r);
         }
      }
   }
   return p;
}

// ─────────────────────────────────────────────────────────────────────────────
// Build / Join / Split (shared across AVL family)
// ─────────────────────────────────────────────────────────────────────────────

node_t *avl_buildL(node_t *l, node_t *p, node_t *r, u64 sl, u64 sr) {
   u64 size = tree_build_inputs_require(l, p, r, sl, sr);
   int rl = rank_of(l);
   int rr = rank_of(r);
   assert_bst(rr >= rl);
   if (rank_distance(rr, rl) <= 1) {
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
   r->l = avl_buildL(l, p, r->l, sl, right_left_size);
   return avl_balanceInsertL(r, size);
}

node_t *avl_buildR(node_t *l, node_t *p, node_t *r, u64 sl, u64 sr) {
   u64 size = tree_build_inputs_require(l, p, r, sl, sr);
   int rl = rank_of(l);
   int rr = rank_of(r);
   assert_bst(rl >= rr);
   if (rank_distance(rl, rr) <= 1) {
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
   l->r = avl_buildR(l->r, p, r, left_right_size, sr);
   return avl_balanceInsertR(l, size);
}

node_t *avl_build(node_t *l, node_t *p, node_t *r, u64 sl, u64 sr) {
   tree_build_inputs_require(l, p, r, sl, sr);
   tree_debug_require_size(l, sl);
   tree_debug_require_size(r, sr);
   if (rank_of(l) < rank_of(r)) {
      return avl_buildL(l, p, r, sl, sr);
   } else {
      return avl_buildR(l, p, r, sl, sr);
   }
}

avl_extract_result_t avl_deleteMin(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   tree_persist_unshared(&p);
   if (p->l == NULL) {
      node_t *r = p->r;
      p->r = NULL;
      return (avl_extract_result_t){.root = r, .node = p};
   }
   u64 child_size = sizeL(p);
   node_t **l = deleteL(p);
   avl_extract_result_t result = avl_deleteMin(*l, child_size);
   *l = result.root;
   result.root = avl_balanceDeleteL(p, size_after_removing_one(size));
   return result;
}

avl_extract_result_t avl_deleteMax(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   tree_persist_unshared(&p);
   if (p->r == NULL) {
      node_t *l = p->l;
      p->l = NULL;
      return (avl_extract_result_t){.root = l, .node = p};
   }
   u64 child_size = sizeR(p, size);
   avl_extract_result_t result = avl_deleteMax(p->r, child_size);
   p->r = result.root;
   result.root = avl_balanceDeleteR(p, size_after_removing_one(size));
   return result;
}

node_t *avl_joinL(node_t *l, node_t *r, u64 sl, u64 sr) {
   tree_sized_root_shape_require(l, sl);
   tree_sized_root_shape_require(r, sr);
   if (r == NULL) {
      invariant(sr == 0);
      tree_debug_require_size(l, sl);
      return l;
   }
   if (rankDifference(r, l) <= 1) {
      avl_extract_result_t extracted = avl_deleteMin(r, sr);
      node_t *r2 = extracted.root;
      node_t *p = extracted.node;
      invariant(p != NULL);
      return avl_build(l, p, r2, sl, size_after_removing_one(sr));
   }
   tree_persist_unshared(&r);
   u64 child_size = sizeL(r);
   r->s = rank_after_prefix(sl, r);
   r->l = avl_joinL(l, r->l, sl, child_size);
   return avl_balanceInsertL(r, size_sum(sl, sr));
}

node_t *avl_joinR(node_t *l, node_t *r, u64 sl, u64 sr) {
   tree_sized_root_shape_require(l, sl);
   tree_sized_root_shape_require(r, sr);
   if (l == NULL) {
      invariant(sl == 0);
      return r;
   }
   if (rankDifference(l, r) <= 1) {
      avl_extract_result_t extracted = avl_deleteMax(l, sl);
      node_t *l2 = extracted.root;
      node_t *p = extracted.node;
      invariant(p != NULL);
      invariant(sl > 0);
      return avl_build(l2, p, r, rank_of_last(sl), sr);
   }
   invariant(l->s < sl);
   tree_persist_unshared(&l);
   l->r = avl_joinR(l->r, r, sizeR(l, sl), sr);
   return avl_balanceInsertR(l, size_sum(sl, sr));
}

node_t *avl_join(node_t *l, node_t *r, u64 sl, u64 sr) {
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
      return avl_joinL(l, r, sl, sr);
   } else {
      return avl_joinR(l, r, sl, sr);
   }
}

static tree_split_result_t avl_split_impl(node_t *root, u64 split_size,
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
          avl_split_impl(root->l, split_size, left_size);

      u64 right_split_size = suffix_size_after_prefix(left_size, split_size);
      return (tree_split_result_t){
          .left = split.left,
          .right = avl_build(split.right, root, root->r, right_split_size,
                             right_size),
      };
   }

   u64 right_rank = rank_in_right_subtree(split_size, left_size);
   tree_split_result_t split = avl_split_impl(root->r, right_rank, right_size);
   return (tree_split_result_t){
       .left = avl_build(root->l, root, split.left, left_size, right_rank),
       .right = split.right,
   };
}

tree_split_result_t avl_split(node_t *root, u64 split_size, u64 size) {
   (void)tree_split_inputs_require(root, split_size, size);
   if (root != NULL) {
      tree_debug_require_size(root, size);
   }
   return avl_split_impl(root, split_size, size);
}

// ─────────────────────────────────────────────────────────────────────────────
// AVLBottomUp-specific insert/delete
// ─────────────────────────────────────────────────────────────────────────────

node_t *avl_bu_insert_op(node_t *p, u64 size, u64 i, u64 x) {
   u64 inserted_size = tree_insert_inputs_require(p, i, size);
   if (p == NULL) {
      invariant(i == 0);
      return tree_allocate((node_t){.x = x});
   }
   tree_persist_unshared(&p);
   if (i <= sizeL(p)) {
      u64 child_size = sizeL(p);
      node_t **l = insertL(p);
      *l = avl_bu_insert_op(*l, child_size, i, x);
      return avl_balanceInsertL(p, inserted_size);
   } else {
      u64 child_size = sizeR(p, size);
      node_t **r = insertR(p, &i);
      *r = avl_bu_insert_op(*r, child_size, i, x);
      return avl_balanceInsertR(p, inserted_size);
   }
}

avl_bu_delete_result_t avl_bu_delete_op(node_t *p, u64 s, u64 i) {
   u64 deleted_size = tree_delete_inputs_require(p, i, s);
   u64 sl = sizeL(p);
   u64 sr = sizeR(p, s);
   if (i == sl) {
      u64 value = p->x;
      tree_share(p->l);
      tree_share(p->r);
      node_t *root = avl_join(p->l, p->r, sl, sr);
      tree_release(p);
      return (avl_bu_delete_result_t){.root = root, .value = value};
   }
   tree_persist_unshared(&p);
   avl_bu_delete_result_t result;
   if (i < sl) {
      invariant(p->l != NULL);
      node_t **l = deleteL(p);
      result = avl_bu_delete_op(*l, sl, i);
      *l = result.root;
      result.root = avl_balanceDeleteL(p, deleted_size);
      return result;
   } else {
      invariant(i > sl);
      invariant(p->r != NULL);
      node_t **r = deleteR(p, &i);
      result = avl_bu_delete_op(*r, sr, i);
      *r = result.root;
      result.root = avl_balanceDeleteR(p, deleted_size);
      return result;
   }
}

// ─────────────────────────────────────────────────────────────────────────────
// Vtable
// ─────────────────────────────────────────────────────────────────────────────

SIMPLE_VT_BODY(avl_bottomup_t, avl_bu)

static void avl_bu_vt_verify(list_t *self) {
   list_require(self);
   avl_bottomup_t *t = (avl_bottomup_t *)self->impl;
   verifyAVLRanks(t->tree.root, t->tree.size);
}

static void avl_bu_vt_insert(list_t *self, u64 i, u64 x) {
   list_require(self);
   avl_bottomup_t *t = (avl_bottomup_t *)self->impl;
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   t->tree.root = avl_bu_insert_op(t->tree.root, size, i, x);
   tree_list_sized_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
}

static u64 avl_bu_vt_delete(list_t *self, u64 i) {
   list_require(self);
   avl_bottomup_t *t = (avl_bottomup_t *)self->impl;
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   avl_bu_delete_result_t result =
       avl_bu_delete_op(t->tree.root, t->tree.size, i);
   t->tree.root = result.root;
   tree_list_sized_deleted(&t->tree);
   invariant(t->tree.size == deleted_size);
   return result.value;
}

static void avl_bu_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   tree_list_require_split(self, i, out_l, out_r);
   avl_bottomup_t *t = (avl_bottomup_t *)self->impl;
   u64 size = t->tree.size;
   u64 right_size = tree_split_inputs_require(t->tree.root, i, size);
   tree_share(t->tree.root);
   tree_split_result_t split = avl_split(t->tree.root, i, size);
   TREE_LIST_SPLIT_RESULT_PREPARED(avl_bottomup_t, size, split.left, i,
                                   split.right, right_size);
}

static list_t avl_bu_vt_join(list_t *self, list_t *other) {
   tree_list_require_join(self, other);
   avl_bottomup_t *t = (avl_bottomup_t *)self->impl;
   avl_bottomup_t *that = (avl_bottomup_t *)other->impl;
   tree_join_roots_t roots =
       tree_list_prepare_join_roots(&t->tree, &that->tree);
   node_t *root =
       avl_join(roots.left, roots.right, t->tree.size, that->tree.size);
   u64 joined_size = tree_list_join_size(t->tree.size, that->tree.size);
   TREE_LIST_JOIN_RESULT(avl_bottomup_t, root, joined_size);
}

const list_vtable_t avl_bottomup_vtable =
    TREE_LIST_VTABLE("AVLBottomUp", avl_bu);
