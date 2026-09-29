#include "llrb.h"
#include "rank.h"
#include "tree_list_adapter.h"

// ─────────────────────────────────────────────────────────────────────────────
// LLRB internals
//
// A left-leaning red-black tree, based on Sedgewick's simplification.
// Red links (0-children) may only lean left.
// ─────────────────────────────────────────────────────────────────────────────

static node_t *llrb_fixUp(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   invariant(!tree_shared(p));
   //
   // Right-leaning red link: rotate left to make it left-leaning.
   //
   if (isRed(p, p->r) && !isRed(p, p->l)) {
      treeRotateLSized(&p, size);
   }
   //
   // Two consecutive left red links: rotate right.
   //
   if (isRed(p, p->l) && isRed(p->l, p->l->l)) {
      treeRotateRSized(&p, size);
   }
   //
   // Both children red: promote (color flip).
   //
   if (isRed(p, p->l) && isRed(p, p->r)) {
      promote(p);
   }
   return p;
}

static node_t *llrb_insert(node_t *p, u64 size, u64 i, u64 x) {
   u64 inserted_size = tree_insert_inputs_require(p, i, size);
   if (p == NULL) {
      invariant(i == 0);
      return tree_allocate((node_t){.x = x});
   }
   tree_persist_unshared(&p);
   if (i <= p->s) {
      u64 child_size = sizeL(p);
      node_t **l = insertL(p);
      *l = llrb_insert(*l, child_size, i, x);
   } else {
      u64 child_size = sizeR(p, size);
      node_t **r = insertR(p, &i);
      *r = llrb_insert(*r, child_size, i, x);
   }
   return llrb_fixUp(p, inserted_size);
}

// moveRedLeft makes p->l or one of its children red, assuming that p is red
// and both p->l and p->l->l are black.
static node_t *llrb_moveRedLeft(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   invariant(!tree_shared(p));
   demote(p);
   if (p->r != NULL && isRed(p->r, p->r->l)) {
      u64 right_size = sizeR(p, size);
      tree_persist_unshared(&p->r);
      treeRotateRSized(&p->r, right_size);
      treeRotateLSized(&p, size);
      promote(p);
   }
   return p;
}

// moveRedRight makes p->r or one of its children red, assuming that p is red
// and both p->r and p->r->l are black.
static node_t *llrb_moveRedRight(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   invariant(!tree_shared(p));
   demote(p);
   if (p->l != NULL && isRed(p->l, p->l->l)) {
      treeRotateRSized(&p, size);
      promote(p);
   }
   return p;
}

typedef struct {
   node_t *root;
   node_t *node;
} llrb_extract_result_t;

static llrb_extract_result_t llrb_deleteMin(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   tree_persist_unshared(&p);
   if (p->l == NULL) {
      node_t *r = p->r;
      p->r = NULL;
      return (llrb_extract_result_t){.root = r, .node = p};
   }
   if (!isRed(p, p->l) && !isRed(p->l, p->l->l)) {
      p = llrb_moveRedLeft(p, size);
   }
   invariant(p->l != NULL);
   u64 child_size = sizeL(p);
   node_t **l = deleteL(p);
   llrb_extract_result_t result = llrb_deleteMin(*l, child_size);
   *l = result.root;
   result.root = llrb_fixUp(p, size_after_removing_one(size));
   return result;
}

typedef struct {
   node_t *root;
   u64 value;
} llrb_delete_result_t;

static llrb_delete_result_t llrb_delete(node_t *p, u64 size, u64 i) {
   u64 deleted_size = tree_delete_inputs_require(p, i, size);
   tree_persist_unshared(&p);
   llrb_delete_result_t result;
   if (i < p->s) {
      if (!isRed(p, p->l) && (p->l == NULL || !isRed(p->l, p->l->l))) {
         p = llrb_moveRedLeft(p, size);
      }
      invariant(p->l != NULL);
      u64 child_size = sizeL(p);
      node_t **l = deleteL(p);
      result = llrb_delete(*l, child_size, i);
      *l = result.root;
   } else {
      if (isRed(p, p->l)) {
         treeRotateRSized(&p, size);
      }
      if (i == p->s && p->r == NULL) {
         u64 value = p->x;
         tree_release(p);
         return (llrb_delete_result_t){.root = NULL, .value = value};
      }
      if (!isRed(p, p->r) && (p->r == NULL || !isRed(p->r, p->r->l))) {
         p = llrb_moveRedRight(p, size);
      }
      if (i == p->s) {
         invariant(p->r != NULL);
         result.value = p->x;
         u64 child_size = sizeR(p, size);
         llrb_extract_result_t extracted = llrb_deleteMin(p->r, child_size);
         p->r = extracted.root;
         node_t *min = extracted.node;
         invariant(min != NULL);
         p->x = min->x;
         tree_release(min);
      } else {
         invariant(p->r != NULL);
         u64 child_size = sizeR(p, size);
         node_t **r = deleteR(p, &i);
         result = llrb_delete(*r, child_size, i);
         *r = result.root;
      }
   }
   result.root = llrb_fixUp(p, deleted_size);
   return result;
}

static node_t *llrb_balanceInsertL(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   if (isRed(p, p->l)) {
      if (isRed(p, p->r)) {
         if (isRed(p->l, p->l->l) || isRed(p->l, p->l->r)) {
            promote(p);
         }
      } else {
         if (isRed(p->l, p->l->l)) {
            treeRotateRSized(&p, size);
         } else if (isRed(p->l, p->l->r)) {
            treeRotateLRSized(&p, size);
         }
      }
   }
   return llrb_fixUp(p, size);
}

static node_t *llrb_balanceInsertR(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   if (isRed(p, p->r)) {
      if (isRed(p, p->l)) {
         if (isRed(p->r, p->r->r) || isRed(p->r, p->r->l)) {
            promote(p);
         }
      } else {
         if (isRed(p->r, p->r->r)) {
            treeRotateLSized(&p, size);
         } else if (isRed(p->r, p->r->l)) {
            treeRotateRLSized(&p, size);
         }
      }
   }
   return llrb_fixUp(p, size);
}

static node_t *llrb_build(node_t *l, node_t *p, node_t *r, u64 sl, u64 sr) {
   u64 size = tree_build_inputs_require(l, p, r, sl, sr);
   tree_debug_require_size(l, sl);
   tree_debug_require_size(r, sr);
   if (rank_of(l) == rank_of(r)) {
      p->l = l;
      p->r = r;
      p->s = sl;
      int child_rank = rank_of(l);
      if (child_rank < 0) {
         invariant(r == NULL);
         p->y = 0;
      } else {
         p->y = (u64)child_rank;
      }
      if (child_rank >= 0 && (hasZeroChild(l) || hasZeroChild(r))) {
         promote(p);
      }
      return llrb_fixUp(p, size);
   }
   if (rank_of(l) < rank_of(r)) {
      invariant(r != NULL);
      tree_persist_unshared(&r);
      u64 right_left_size = sizeL(r);
      r->s = tree_size_from_child_sizes(sl, right_left_size);
      r->l = llrb_build(l, p, r->l, sl, right_left_size);
      return llrb_balanceInsertL(r, size);
   } else {
      invariant(l != NULL);
      invariant(l->s < sl);
      tree_persist_unshared(&l);
      u64 left_right_size = sizeR(l, sl);
      l->r = llrb_build(l->r, p, r, left_right_size, sr);
      return llrb_balanceInsertR(l, size);
   }
}

static llrb_extract_result_t llrb_deleteMax(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   tree_persist_unshared(&p);
   if (isRed(p, p->l)) {
      treeRotateRSized(&p, size);
   }
   if (p->r == NULL) {
      node_t *l = p->l;
      p->l = NULL;
      return (llrb_extract_result_t){.root = l, .node = p};
   }
   if (!isRed(p, p->r) && (p->r->l == NULL || !isRed(p->r, p->r->l))) {
      p = llrb_moveRedRight(p, size);
   }
   u64 child_size = sizeR(p, size);
   llrb_extract_result_t result = llrb_deleteMax(p->r, child_size);
   p->r = result.root;
   result.root = llrb_fixUp(p, size_after_removing_one(size));
   return result;
}

static node_t *llrb_join(node_t *l, node_t *r, u64 sl, u64 sr) {
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
      llrb_extract_result_t extracted = llrb_deleteMin(r, sr);
      node_t *r2 = extracted.root;
      node_t *p = extracted.node;
      invariant(p != NULL);
      return llrb_build(l, p, r2, sl, size_after_removing_one(sr));
   } else {
      llrb_extract_result_t extracted = llrb_deleteMax(l, sl);
      node_t *l2 = extracted.root;
      node_t *p = extracted.node;
      invariant(p != NULL);
      invariant(sl > 0);
      return llrb_build(l2, p, r, rank_of_last(sl), sr);
   }
}

static tree_split_result_t llrb_split_impl(node_t *root, u64 split_size,
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
          llrb_split_impl(root->l, split_size, left_size);

      u64 right_split_size = suffix_size_after_prefix(left_size, split_size);
      return (tree_split_result_t){
          .left = split.left,
          .right = llrb_build(split.right, root, root->r, right_split_size,
                              right_size),
      };
   }

   u64 right_rank = rank_in_right_subtree(split_size, left_size);
   tree_split_result_t split = llrb_split_impl(root->r, right_rank, right_size);
   return (tree_split_result_t){
       .left = llrb_build(root->l, root, split.left, left_size, right_rank),
       .right = split.right,
   };
}

static tree_split_result_t llrb_split(node_t *root, u64 split_size, u64 size) {
   (void)tree_split_inputs_require(root, split_size, size);
   if (root != NULL) {
      tree_debug_require_size(root, size);
   }
   return llrb_split_impl(root, split_size, size);
}

static void llrb_verifyLeftLeaningNode(node_t *p, void *ctx) {
   (void)ctx;
   // No right-leaning red links: if right is red, left must also be red.
   invariant(!isRed(p, p->r) || isRed(p, p->l));
}

static void llrb_verifyLeftLeaning(node_t *p, u64 size) {
   invariant(tree_verify_size_each_node(p, size, llrb_verifyLeftLeaningNode,
                                        NULL) == size);
}

// ─────────────────────────────────────────────────────────────────────────────
// List vtable
// ─────────────────────────────────────────────────────────────────────────────

TREE_LIST_SIMPLE_FUNCTIONS(llrb_t, llrb)

static void llrb_vt_verify(list_t *self) {
   list_require(self);
   llrb_t *t = (llrb_t *)self->impl;
   verifyRedBlackRanksSized(t->tree.root, t->tree.size);
   llrb_verifyLeftLeaning(t->tree.root, t->tree.size);
   verifyRedBlackHeight(t->tree.root, t->tree.size);
}

static void llrb_vt_insert(list_t *self, u64 i, u64 x) {
   list_require(self);
   llrb_t *t = (llrb_t *)self->impl;
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   t->tree.root = llrb_insert(t->tree.root, size, i, x);
   tree_list_sized_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
}

static u64 llrb_vt_delete(list_t *self, u64 i) {
   list_require(self);
   llrb_t *t = (llrb_t *)self->impl;
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   llrb_delete_result_t result = llrb_delete(t->tree.root, size, i);
   t->tree.root = result.root;
   tree_list_sized_deleted(&t->tree);
   invariant(t->tree.size == deleted_size);
   return result.value;
}

static void llrb_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   tree_list_require_split(self, i, out_l, out_r);
   llrb_t *t = (llrb_t *)self->impl;
   u64 size = t->tree.size;
   u64 right_size = tree_split_inputs_require(t->tree.root, i, size);
   tree_share(t->tree.root);
   tree_split_result_t split = llrb_split(t->tree.root, i, size);
   TREE_LIST_SPLIT_RESULT_PREPARED(llrb_t, size, split.left, i, split.right,
                                   right_size);
}

static list_t llrb_vt_join(list_t *self, list_t *other) {
   tree_list_require_join(self, other);
   llrb_t *t = (llrb_t *)self->impl;
   llrb_t *that = (llrb_t *)other->impl;
   tree_join_roots_t roots =
       tree_list_prepare_join_roots(&t->tree, &that->tree);
   node_t *root =
       llrb_join(roots.left, roots.right, t->tree.size, that->tree.size);
   u64 joined_size = tree_list_join_size(t->tree.size, that->tree.size);
   TREE_LIST_JOIN_RESULT(llrb_t, root, joined_size);
}

const list_vtable_t llrb_vtable = TREE_LIST_VTABLE("LLRB", llrb);
