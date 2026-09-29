#include "avl_weak_bottomup.h"
#include "avl_bottomup.h"
#include "rank.h"
#include "tree_list_adapter.h"

// AVLWeakBottomUp
// ═══════════════════════════════════════════════════════════════════════════════

static node_t *avlw_rebalanceOnDelete(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   if (isLeaf(p) && isTwoTwo(p)) {
      demote(p);
      return p;
   }
   if (isThreeChild(p, p->r)) {
      if (isTwoChild(p, p->l)) {
         demote(p);
      } else if (isTwoTwo(p->l)) {
         demote(p->l);
         demote(p);
      } else if (isOneChild(p->l, p->l->l)) {
         treeRotateRSized(&p, size);
         promote(p);
         demote(p->r);
         if (p->r->l == NULL) {
            assert_bst(isTwoTwo(p->r));
            demote(p->r);
         }
      } else {
         treeRotateLRSized(&p, size);
         promote(p);
         promote(p);
         demote(p->l);
         demote(p->r);
         demote(p->r);
      }
   } else if (isThreeChild(p, p->l)) {
      if (isTwoChild(p, p->r)) {
         demote(p);
      } else if (isTwoTwo(p->r)) {
         demote(p->r);
         demote(p);
      } else if (isOneChild(p->r, p->r->r)) {
         treeRotateLSized(&p, size);
         promote(p);
         demote(p->l);
         if (p->l->r == NULL) {
            assert_bst(isTwoTwo(p->l));
            demote(p->l);
         }
      } else {
         treeRotateRLSized(&p, size);
         promote(p);
         promote(p);
         demote(p->l);
         demote(p->l);
         demote(p->r);
      }
   }
   return p;
}

typedef struct {
   node_t *root;
   node_t *node;
} avlw_extract_result_t;

static avlw_extract_result_t avlw_extractMin(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   if (p->l == NULL) {
      node_t *min = tree_replacedByRightSubtree_sized(&p, size);
      return (avlw_extract_result_t){.root = p, .node = min};
   }
   tree_persist_unshared(&p);
   u64 child_size = sizeL(p);
   node_t **l = deleteL(p);
   avlw_extract_result_t result = avlw_extractMin(*l, child_size);
   *l = result.root;
   result.root = avlw_rebalanceOnDelete(p, size_after_removing_one(size));
   return result;
}

static avlw_extract_result_t avlw_extractMax(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   if (p->r == NULL) {
      node_t *max = tree_replacedByLeftSubtree_sized(&p, size);
      return (avlw_extract_result_t){.root = p, .node = max};
   }
   tree_persist_unshared(&p);
   u64 child_size = sizeR(p, size);
   avlw_extract_result_t result = avlw_extractMax(p->r, child_size);
   p->r = result.root;
   result.root = avlw_rebalanceOnDelete(p, size_after_removing_one(size));
   return result;
}

static node_t *avlw_buildL(node_t *l, node_t *p, node_t *r, u64 sl, u64 sr) {
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
   tree_nonempty_root_shape_require(r, sr);
   tree_persist_unshared(&r);
   u64 right_left_size = sizeL(r);
   r->s = tree_size_from_child_sizes(sl, right_left_size);
   r->l = avlw_buildL(l, p, r->l, sl, right_left_size);
   return avl_balanceInsertL(r, size);
}

static node_t *avlw_buildR(node_t *l, node_t *p, node_t *r, u64 sl, u64 sr) {
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
   tree_nonempty_root_shape_require(l, sl);
   tree_persist_unshared(&l);
   u64 left_right_size = sizeR(l, sl);
   l->r = avlw_buildR(l->r, p, r, left_right_size, sr);
   return avl_balanceInsertR(l, size);
}

node_t *avlw_build(node_t *l, node_t *p, node_t *r, u64 sl, u64 sr) {
   tree_build_inputs_require(l, p, r, sl, sr);
   tree_debug_require_size(l, sl);
   tree_debug_require_size(r, sr);
   if (rank_of(l) < rank_of(r)) {
      return avlw_buildL(l, p, r, sl, sr);
   }
   return avlw_buildR(l, p, r, sl, sr);
}

node_t *avlw_join(node_t *l, node_t *r, u64 sl, u64 sr) {
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
      avlw_extract_result_t extracted = avlw_extractMin(r, sr);
      node_t *r2 = extracted.root;
      node_t *p = extracted.node;
      invariant(p != NULL);
      return avlw_build(l, p, r2, sl, size_after_removing_one(sr));
   } else {
      avlw_extract_result_t extracted = avlw_extractMax(l, sl);
      node_t *l2 = extracted.root;
      node_t *p = extracted.node;
      invariant(p != NULL);
      invariant(sl > 0);
      return avlw_build(l2, p, r, rank_of_last(sl), sr);
   }
}

typedef struct {
   node_t *root;
   u64 value;
} avlw_delete_result_t;

static avlw_delete_result_t avlw_delete(node_t *p, u64 s, u64 i) {
   u64 deleted_size = tree_delete_inputs_require(p, i, s);
   u64 sl = sizeL(p), sr = sizeR(p, s);
   if (i == p->s) {
      u64 value = p->x;
      tree_share(p->l);
      tree_share(p->r);
      node_t *root = avlw_join(p->l, p->r, sl, sr);
      tree_release(p);
      return (avlw_delete_result_t){.root = root, .value = value};
   }
   tree_persist_unshared(&p);
   avlw_delete_result_t result;
   if (i < p->s) {
      invariant(p->l != NULL);
      node_t **l = deleteL(p);
      result = avlw_delete(*l, sl, i);
      *l = result.root;
   } else {
      invariant(i > p->s);
      invariant(p->r != NULL);
      node_t **r = deleteR(p, &i);
      result = avlw_delete(*r, sr, i);
      *r = result.root;
   }
   result.root = avlw_rebalanceOnDelete(p, deleted_size);
   return result;
}

static tree_split_result_t avlw_split_impl(node_t *root, u64 split_size,
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
          avlw_split_impl(root->l, split_size, left_size);

      u64 right_split_size = suffix_size_after_prefix(left_size, split_size);
      return (tree_split_result_t){
          .left = split.left,
          .right = avlw_build(split.right, root, root->r, right_split_size,
                              right_size),
      };
   }

   u64 right_rank = rank_in_right_subtree(split_size, left_size);
   tree_split_result_t split = avlw_split_impl(root->r, right_rank, right_size);
   return (tree_split_result_t){
       .left = avlw_build(root->l, root, split.left, left_size, right_rank),
       .right = split.right,
   };
}

tree_split_result_t avlw_split(node_t *root, u64 split_size, u64 size) {
   (void)tree_split_inputs_require(root, split_size, size);
   if (root != NULL) {
      tree_debug_require_size(root, size);
   }
   return avlw_split_impl(root, split_size, size);
}

TREE_VT_SIMPLE(avl_weak_bottomup_t, avlw_bu)

static void avlw_bu_vt_verify(list_t *self) {
   list_require(self);
   avl_weak_bottomup_t *t = (avl_weak_bottomup_t *)self->impl;
   verifyAVLWeakRanksSized(t->tree.root, t->tree.size);
   verifyAVLWeakHeight(t->tree.root, t->tree.size);
}
static void avlw_bu_vt_insert(list_t *self, u64 i, u64 x) {
   list_require(self);
   avl_weak_bottomup_t *t = (avl_weak_bottomup_t *)self->impl;
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   // Uses AVLBottomUp's insert (which uses avl_balanceInsertL/R)
   t->tree.root = avl_bu_insert_op(t->tree.root, size, i, x);
   tree_list_sized_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
}
static u64 avlw_bu_vt_delete(list_t *self, u64 i) {
   list_require(self);
   avl_weak_bottomup_t *t = (avl_weak_bottomup_t *)self->impl;
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   avlw_delete_result_t result = avlw_delete(t->tree.root, t->tree.size, i);
   t->tree.root = result.root;
   tree_list_sized_deleted(&t->tree);
   invariant(t->tree.size == deleted_size);
   return result.value;
}
static void avlw_bu_vt_split(list_t *self, u64 i, list_t *out_l,
                             list_t *out_r) {
   tree_list_require_split(self, i, out_l, out_r);
   avl_weak_bottomup_t *t = (avl_weak_bottomup_t *)self->impl;
   u64 size = t->tree.size;
   u64 right_size = tree_split_inputs_require(t->tree.root, i, size);
   tree_share(t->tree.root);
   tree_split_result_t split = avlw_split(t->tree.root, i, size);
   TREE_LIST_SPLIT_RESULT_PREPARED(avl_weak_bottomup_t, size, split.left, i,
                                   split.right, right_size);
}
static list_t avlw_bu_vt_join(list_t *self, list_t *other) {
   tree_list_require_join(self, other);
   avl_weak_bottomup_t *t = (avl_weak_bottomup_t *)self->impl;
   avl_weak_bottomup_t *that = (avl_weak_bottomup_t *)other->impl;
   tree_join_roots_t roots =
       tree_list_prepare_join_roots(&t->tree, &that->tree);
   node_t *root =
       avlw_join(roots.left, roots.right, t->tree.size, that->tree.size);
   u64 joined_size = tree_list_join_size(t->tree.size, that->tree.size);
   TREE_LIST_JOIN_RESULT(avl_weak_bottomup_t, root, joined_size);
}

// avl_bu_insert_op is defined in avl_bottomup.c and declared in avl_bottomup.h

const list_vtable_t avl_weak_bottomup_vtable =
    TREE_LIST_VTABLE("AVLWeakBottomUp", avlw_bu);

// ═══════════════════════════════════════════════════════════════════════════════
