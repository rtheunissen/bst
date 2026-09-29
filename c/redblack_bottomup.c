#include "redblack_bottomup.h"
#include "tree_list_adapter.h"

// RedBlack family: BottomUp, TopDown, RelaxedBottomUp, RelaxedTopDown

#include "rank.h"

// ─────────────────────────────────────────────────────────────────────────────
// Shared RedBlack balancing
// ─────────────────────────────────────────────────────────────────────────────

node_t *rb_balanceInsertL(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   invariant(!tree_shared(p));
   if (isZeroChild(p, p->l)) {
      if (isZeroChild(p, p->r)) {
         if (isZeroChild(p->l, p->l->l) || isZeroChild(p->l, p->l->r)) {
            promote(p);
            return p;
         }
      } else {
         if (isZeroChild(p->l, p->l->l)) {
            treeRotateRSized(&p, size);
            return p;
         }
         if (isZeroChild(p->l, p->l->r)) {
            treeRotateLRSized(&p, size);
            return p;
         }
      }
   }
   return p;
}

node_t *rb_balanceInsertR(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   invariant(!tree_shared(p));
   if (isZeroChild(p, p->r)) {
      if (isZeroChild(p, p->l)) {
         if (isZeroChild(p->r, p->r->r) || isZeroChild(p->r, p->r->l)) {
            promote(p);
            return p;
         }
      } else {
         if (isZeroChild(p->r, p->r->r)) {
            treeRotateLSized(&p, size);
            return p;
         }
         if (isZeroChild(p->r, p->r->l)) {
            treeRotateRLSized(&p, size);
            return p;
         }
      }
   }
   return p;
}

node_t *rb_balanceDeleteL(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   invariant(!tree_shared(p));
   if (isZeroChild(p, p->l)) {
      return p;
   }
   if (isOneChild(p, p->l)) {
      return p;
   }
   if (isZeroChild(p, p->r)) {
      assert_bst(isOneOne(p->r));
      treeRotateLSized(&p, size);
      u64 left_size = sizeL(p);
      if (isZeroChild(p->l->r, p->l->r->r)) {
         treeRotateLSized(&p->l, left_size);
         promote(p->l);
         demote(p->l->l);
         return p;
      }
      if (isZeroChild(p->l->r, p->l->r->l)) {
         treeRotateRLSized(&p->l, left_size);
         promote(p->l);
         demote(p->l->l);
         return p;
      }
      demote(p->l);
      return p;
   } else {
      if (isZeroChild(p->r, p->r->r)) {
         treeRotateLSized(&p, size);
         promote(p);
         demote(p->l);
         return p;
      }
      if (isZeroChild(p->r, p->r->l)) {
         treeRotateRLSized(&p, size);
         promote(p);
         demote(p->l);
         return p;
      }
      assert_bst(isOneOne(p->r));
      demote(p);
      return p;
   }
}

node_t *rb_balanceDeleteR(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   invariant(!tree_shared(p));
   if (isZeroChild(p, p->r)) {
      return p;
   }
   if (isOneChild(p, p->r)) {
      return p;
   }
   if (isZeroChild(p, p->l)) {
      assert_bst(isOneOne(p->l));
      treeRotateRSized(&p, size);
      u64 right_size = sizeR(p, size);
      if (isZeroChild(p->r->l, p->r->l->l)) {
         treeRotateRSized(&p->r, right_size);
         promote(p->r);
         demote(p->r->r);
         return p;
      }
      if (isZeroChild(p->r->l, p->r->l->r)) {
         treeRotateLRSized(&p->r, right_size);
         promote(p->r);
         demote(p->r->r);
         return p;
      }
      demote(p->r);
      return p;
   } else {
      if (isZeroChild(p->l, p->l->l)) {
         treeRotateRSized(&p, size);
         promote(p);
         demote(p->r);
         return p;
      }
      if (isZeroChild(p->l, p->l->r)) {
         treeRotateLRSized(&p, size);
         promote(p);
         demote(p->r);
         return p;
      }
      assert_bst(isOneOne(p->l));
      demote(p);
      return p;
   }
}

// ─────────────────────────────────────────────────────────────────────────────
// RedBlack Build / Join / Split
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   node_t *root;
   node_t *node;
} rb_extract_result_t;

static rb_extract_result_t rb_deleteMin(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   tree_persist_unshared(&p);
   if (p->l == NULL) {
      node_t *r = p->r;
      p->r = NULL;
      return (rb_extract_result_t){.root = r, .node = p};
   }
   invariant(p->s > 0);
   u64 child_size = sizeL(p);
   node_t **l = deleteL(p);
   rb_extract_result_t result = rb_deleteMin(*l, child_size);
   *l = result.root;
   result.root = rb_balanceDeleteL(p, size_after_removing_one(size));
   return result;
}

static rb_extract_result_t rb_deleteMax(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   tree_persist_unshared(&p);
   if (p->r == NULL) {
      node_t *l = p->l;
      p->l = NULL;
      return (rb_extract_result_t){.root = l, .node = p};
   }
   u64 child_size = sizeR(p, size);
   rb_extract_result_t result = rb_deleteMax(p->r, child_size);
   p->r = result.root;
   result.root = rb_balanceDeleteR(p, size_after_removing_one(size));
   return result;
}

node_t *rb_build(node_t *l, node_t *p, node_t *r, u64 sl, u64 sr) {
   u64 size = tree_build_inputs_require(l, p, r, sl, sr);
   tree_debug_require_size(l, sl);
   tree_debug_require_size(r, sr);
   if (rank_of(l) == rank_of(r)) {
      p->l = l;
      p->r = r;
      p->s = sl;
      int child_rank = rank_of(l);
      if (child_rank < 0) {
         invariant(l == NULL);
         invariant(r == NULL);
         p->y = 0;
      } else {
         p->y = (u64)child_rank;
      }
      if (child_rank >= 0 && (hasZeroChild(l) || hasZeroChild(r))) {
         promote(p);
      }
      return p;
   }
   if (rank_of(l) < rank_of(r)) {
      invariant(r != NULL);
      tree_persist_unshared(&r);
      u64 right_left_size = sizeL(r);
      r->s = tree_size_from_child_sizes(sl, right_left_size);
      r->l = rb_build(l, p, r->l, sl, right_left_size);
      return rb_balanceInsertL(r, size);
   } else {
      invariant(l != NULL);
      invariant(l->s < sl);
      tree_persist_unshared(&l);
      u64 left_right_size = sizeR(l, sl);
      l->r = rb_build(l->r, p, r, left_right_size, sr);
      return rb_balanceInsertR(l, size);
   }
}

node_t *rb_join(node_t *l, node_t *r, u64 sl, u64 sr) {
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
      rb_extract_result_t extracted = rb_deleteMin(r, sr);
      node_t *r2 = extracted.root;
      node_t *p = extracted.node;
      invariant(p != NULL);
      return rb_build(l, p, r2, sl, size_after_removing_one(sr));
   } else {
      rb_extract_result_t extracted = rb_deleteMax(l, sl);
      node_t *l2 = extracted.root;
      node_t *p = extracted.node;
      invariant(p != NULL);
      invariant(sl > 0);
      return rb_build(l2, p, r, rank_of_last(sl), sr);
   }
}

static tree_split_result_t rb_split_impl(node_t *root, u64 size,
                                         u64 split_size) {
   (void)tree_split_inputs_require(root, split_size, size);
   if (root == NULL) {
      return (tree_split_result_t){.left = NULL, .right = NULL};
   }
   tree_persist_unshared(&root);
   u64 left_size = sizeL(root);
   u64 right_size = sizeR(root, size);
   if (split_size <= root->s) {
      tree_split_result_t split = rb_split_impl(root->l, left_size, split_size);

      u64 right_split_size = suffix_size_after_prefix(left_size, split_size);
      return (tree_split_result_t){
          .left = split.left,
          .right = rb_build(split.right, root, root->r, right_split_size,
                            right_size),
      };
   }

   u64 right_rank = rank_in_right_subtree(split_size, left_size);
   tree_split_result_t split = rb_split_impl(root->r, right_size, right_rank);
   return (tree_split_result_t){
       .left = rb_build(root->l, root, split.left, left_size, right_rank),
       .right = split.right,
   };
}

tree_split_result_t rb_split(node_t *root, u64 size, u64 split_size) {
   (void)tree_split_inputs_require(root, split_size, size);
   if (root != NULL) {
      tree_debug_require_size(root, size);
   }
   return rb_split_impl(root, size, split_size);
}

// ─────────────────────────────────────────────────────────────────────────────
// RedBlackBottomUp insert/delete
// ─────────────────────────────────────────────────────────────────────────────

node_t *rb_bu_insert(node_t *p, u64 size, u64 i, u64 x) {
   u64 inserted_size = tree_insert_inputs_require(p, i, size);
   if (p == NULL) {
      invariant(i == 0);
      return tree_allocate((node_t){.x = x});
   }
   tree_persist_unshared(&p);
   if (i <= p->s) {
      u64 child_size = sizeL(p);
      node_t **l = insertL(p);
      *l = rb_bu_insert(*l, child_size, i, x);
      return rb_balanceInsertL(p, inserted_size);
   } else {
      u64 child_size = sizeR(p, size);
      node_t **r = insertR(p, &i);
      *r = rb_bu_insert(*r, child_size, i, x);
      return rb_balanceInsertR(p, inserted_size);
   }
}

rb_bu_delete_result_t rb_bu_delete(node_t *p, u64 s, u64 i) {
   u64 deleted_size = tree_delete_inputs_require(p, i, s);
   u64 sl = sizeL(p), sr = sizeR(p, s);
   if (i == sl) {
      u64 value = p->x;
      tree_share(p->l);
      tree_share(p->r);
      node_t *root = rb_join(p->l, p->r, sl, sr);
      tree_release(p);
      return (rb_bu_delete_result_t){.root = root, .value = value};
   }
   tree_persist_unshared(&p);
   rb_bu_delete_result_t result;
   if (i < sl) {
      invariant(p->l != NULL);
      node_t **l = deleteL(p);
      result = rb_bu_delete(*l, sl, i);
      *l = result.root;
      result.root = rb_balanceDeleteL(p, deleted_size);
      return result;
   } else {
      invariant(i > p->s);
      invariant(p->r != NULL);
      node_t **r = deleteR(p, &i);
      result = rb_bu_delete(*r, sr, i);
      *r = result.root;
      result.root = rb_balanceDeleteR(p, deleted_size);
      return result;
   }
}

// RedBlackBottomUp
// ═══════════════════════════════════════════════════════════════════════════════

TREE_VT_SIMPLE(redblack_bottomup_t, rb_bu)
static void rb_bu_vt_verify(list_t *self) {
   list_require(self);
   redblack_bottomup_t *t = (redblack_bottomup_t *)self->impl;
   verifyRedBlackRanksSized(t->tree.root, t->tree.size);
   verifyRedBlackHeight(t->tree.root, t->tree.size);
}
static void rb_bu_vt_insert(list_t *self, u64 i, u64 x) {
   list_require(self);
   redblack_bottomup_t *t = (redblack_bottomup_t *)self->impl;
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   t->tree.root = rb_bu_insert(t->tree.root, size, i, x);
   tree_list_sized_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
}
static u64 rb_bu_vt_delete(list_t *self, u64 i) {
   list_require(self);
   redblack_bottomup_t *t = (redblack_bottomup_t *)self->impl;
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   rb_bu_delete_result_t result = rb_bu_delete(t->tree.root, t->tree.size, i);
   t->tree.root = result.root;
   tree_list_sized_deleted(&t->tree);
   invariant(t->tree.size == deleted_size);
   return result.value;
}
static void rb_bu_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   tree_list_require_split(self, i, out_l, out_r);
   redblack_bottomup_t *t = (redblack_bottomup_t *)self->impl;
   u64 size = t->tree.size;
   u64 right_size = tree_split_inputs_require(t->tree.root, i, size);
   tree_share(t->tree.root);
   tree_split_result_t split = rb_split(t->tree.root, size, i);
   TREE_LIST_SPLIT_RESULT_PREPARED(redblack_bottomup_t, size, split.left, i,
                                   split.right, right_size);
}
static list_t rb_bu_vt_join(list_t *self, list_t *other) {
   tree_list_require_join(self, other);
   redblack_bottomup_t *t = (redblack_bottomup_t *)self->impl;
   redblack_bottomup_t *that = (redblack_bottomup_t *)other->impl;
   tree_join_roots_t roots =
       tree_list_prepare_join_roots(&t->tree, &that->tree);
   node_t *root =
       rb_join(roots.left, roots.right, t->tree.size, that->tree.size);
   u64 joined_size = tree_list_join_size(t->tree.size, that->tree.size);
   TREE_LIST_JOIN_RESULT(redblack_bottomup_t, root, joined_size);
}
const list_vtable_t redblack_bottomup_vtable =
    TREE_LIST_VTABLE("RedBlackBottomUp", rb_bu);

// ═══════════════════════════════════════════════════════════════════════════════
