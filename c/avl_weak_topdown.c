#include "avl_weak_topdown.h"
#include "avl_weak_bottomup.h"
#include "avl_bottomup.h"
#include "rank.h"
#include "tree_list_adapter.h"

// AVLWeakTopDown
// ═══════════════════════════════════════════════════════════════════════════════

static node_t *avlw_td_rebalanceOnDelete(node_t *p) {
   invariant(p != NULL);
   invariant(!tree_shared(p));
   if (isThreeChild(p, p->r)) {
      if (isOneChild(p->l, p->l->l)) {
         treeRotateRProvisional(&p);
         promote(p);
         demote(p->r);
         if (p->r->l == NULL) {
            assert_bst(isTwoTwo(p->r));
            demote(p->r);
         }
      } else {
         treeRotateLRProvisional(&p);
         promote(p);
         promote(p);
         demote(p->l);
         demote(p->r);
         demote(p->r);
      }
   } else if (isThreeChild(p, p->l)) {
      if (isOneChild(p->r, p->r->r)) {
         treeRotateLProvisional(&p);
         promote(p);
         demote(p->l);
         if (p->l->r == NULL) {
            assert_bst(isTwoTwo(p->l));
            demote(p->l);
         }
      } else {
         treeRotateRLProvisional(&p);
         promote(p);
         promote(p);
         demote(p->l);
         demote(p->l);
         demote(p->r);
      }
   }
   return p;
}

static void avlw_td_rebalanceAfterDissolve(node_t **g, node_t **p) {
   invariant(g != NULL);
   invariant(p != NULL);
   invariant(*g != NULL);
   invariant(*p != NULL);
   invariant(!tree_shared(*g));
   invariant(!tree_shared(*p));
   if (isLeaf(*p) && isTwoTwo(*p)) {
      demote(*p);
   }
   *g = avlw_td_rebalanceOnDelete(*g);
   *p = avlw_td_rebalanceOnDelete(*p);
}

static bool avlw_td_resetSafeNode(node_t *p) {
   invariant(p != NULL);
   invariant(!tree_shared(p));
   if (isTwoTwo(p)) {
      demote(p);
      return true;
   }
   if (isTwoChild(p, p->l) && isTwoTwo(p->r)) {
      tree_persist_unshared(&p->r);
      demote(p);
      demote(p->r);
      return true;
   }
   if (isTwoChild(p, p->r) && isTwoTwo(p->l)) {
      tree_persist_unshared(&p->l);
      demote(p);
      demote(p->l);
      return true;
   }
   return false;
}

static u64 avlw_td_dissolve(node_t **p, u64 s) {
   invariant(p != NULL);
   node_t *old = *p;
   tree_nonempty_root_shape_require(old, s);
   tree_share(old->l);
   tree_share(old->r);
   u64 x = old->x;
   *p = avlw_join(old->l, old->r, old->s, sizeR(old, s));
   tree_release(old);
   return x;
}

static u64 avlw_td_delete(node_t **p, u64 s, u64 i) {
   invariant(p != NULL);
   (void)tree_delete_inputs_require(*p, i, s);
   if ((*p)->s == i) {
      return avlw_td_dissolve(p, s);
   }
   node_t **g = p;
   for (;;) {
      (void)tree_delete_inputs_require(*p, i, s);
      tree_persist_unshared(p);
      node_t *node = *p;
      invariant(node != NULL);
      if (avlw_td_resetSafeNode(node)) {
         *g = avlw_td_rebalanceOnDelete(*g);
      }
      node = *p;
      invariant(node != NULL);
      invariant(!tree_shared(node));
      if (i < node->s) {
         u64 child_size = node->s;
         deleteL(node);
         invariant(node->l != NULL);
         (void)tree_delete_inputs_require(node->l, i, child_size);
         if (node->l->s == i) {
            u64 x = avlw_td_dissolve(&node->l, child_size);
            avlw_td_rebalanceAfterDissolve(g, p);
            return x;
         }
         g = p;
         p = &node->l;
         s = child_size;
      } else {
         u64 child_size = sizeR(node, s);
         deleteR(node, &i);
         invariant(node->r != NULL);
         (void)tree_delete_inputs_require(node->r, i, child_size);
         if (node->r->s == i) {
            u64 x = avlw_td_dissolve(&node->r, child_size);
            avlw_td_rebalanceAfterDissolve(g, p);
            return x;
         }
         g = p;
         p = &node->r;
         s = child_size;
      }
   }
}

static node_t *avlw_td_tdDeleteMin(node_t **p, u64 size) {
   invariant(p != NULL);
   tree_nonempty_root_shape_require(*p, size);
   node_t **g = p;
   if ((*p)->l == NULL) {
      return tree_replacedByRightSubtree_sized(p, size);
   }
   u64 subtree_size = size;
   for (;;) {
      tree_nonempty_root_shape_require(*p, subtree_size);
      tree_persist_unshared(p);
      if (avlw_td_resetSafeNode(*p)) {
         *g = avlw_td_rebalanceOnDelete(*g);
      }
      u64 child_size = sizeL(*p);
      node_t **l = deleteL(*p);
      tree_nonempty_root_shape_require(*l, child_size);
      if ((*l)->l == NULL) {
         node_t *min = tree_replacedByRightSubtree_sized(l, child_size);
         avlw_td_rebalanceAfterDissolve(g, p);
         return min;
      }
      g = p;
      p = l;
      subtree_size = child_size;
   }
}

static node_t *avlw_td_tdDeleteMax(node_t **p, u64 size) {
   invariant(p != NULL);
   tree_nonempty_root_shape_require(*p, size);
   node_t **g = p;
   if ((*p)->r == NULL) {
      return tree_replacedByLeftSubtree_sized(p, size);
   }
   u64 subtree_size = size;
   for (;;) {
      tree_persist_unshared(p);
      if (avlw_td_resetSafeNode(*p)) {
         *g = avlw_td_rebalanceOnDelete(*g);
      }
      invariant((*p)->r != NULL);
      u64 child_size = sizeR(*p, subtree_size);
      node_t **r = &(*p)->r;
      tree_nonempty_root_shape_require(*r, child_size);
      if ((*r)->r == NULL) {
         node_t *max = tree_replacedByLeftSubtree_sized(r, child_size);
         avlw_td_rebalanceAfterDissolve(g, p);
         return max;
      }
      g = p;
      p = r;
      subtree_size = child_size;
   }
}

static node_t *avlw_td_join(node_t *l, node_t *r, u64 sl, u64 sr) {
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
      node_t *p = avlw_td_tdDeleteMin(&r, sr);
      invariant(p != NULL);
      return avlw_build(l, p, r, sl, size_after_removing_one(sr));
   } else {
      invariant(sl > 0);
      node_t *p = avlw_td_tdDeleteMax(&l, sl);
      invariant(p != NULL);
      return avlw_build(l, p, r, rank_of_last(sl), sr);
   }
}

void avlw_td_insert(node_t **p, u64 size, u64 i, u64 x) {
   invariant(p != NULL);
   (void)tree_insert_inputs_require(*p, i, size);
   if (*p == NULL) {
      invariant(i == 0);
      tree_attach(p, x);
      return;
   }
   tree_persist_unshared(p);
   if (isOneOne(*p)) {
      promote(*p);
   }
   u64 subtree_size = size;
   for (;;) {
      invariant((*p)->s < subtree_size);
      (void)tree_insert_inputs_require(*p, i, subtree_size);
      u64 left_size = sizeL(*p);
      assert_bst(!isOneOne(*p));
      if (i <= left_size) {
         (void)tree_insert_inputs_require((*p)->l, i, left_size);
         if ((*p)->l == NULL) {
            tree_attachL(*p, x);
            return;
         }
         if (!isOneOne((*p)->l)) {
            tree_pathLeftSized(&p, &subtree_size);
            continue;
         }
         if (!isOneChild(*p, (*p)->l)) {
            tree_pathLeftSized(&p, &subtree_size);
            promote(*p);
            continue;
         }
         u64 left_left_size = sizeL((*p)->l);
         if (i <= left_left_size) {
            (void)tree_insert_inputs_require((*p)->l->l, i, left_left_size);
            if ((*p)->l->l == NULL) {
               tree_attachLL(*p, x);
               treeRotateRSized(p, size_after_inserting_one(subtree_size));
               promote(*p);
               demote((*p)->r);
               return;
            }
            if (!isOneOne((*p)->l->l)) {
               tree_pathLeftSized(&p, &subtree_size);
               tree_pathLeftSized(&p, &subtree_size);
               continue;
            }
            treeRotateRSized(p, subtree_size);
            promote(*p);
            demote((*p)->r);
            tree_pathLeftSized(&p, &subtree_size);
            promote(*p);
            continue;
         } else {
            u64 left_right_size = sizeR((*p)->l, left_size);
            u64 left_right_rank = rank_in_right_subtree(i, left_left_size);
            (void)tree_insert_inputs_require((*p)->l->r, left_right_rank,
                                             left_right_size);
            if ((*p)->l->r == NULL) {
               tree_attachLR(*p, x);
               treeRotateLRSized(p, size_after_inserting_one(subtree_size));
               promote(*p);
               demote((*p)->r);
               return;
            }
            if (!isOneOne((*p)->l->r)) {
               tree_pathLeftSized(&p, &subtree_size);
               tree_pathRightSized(&p, &subtree_size, &i);
               continue;
            }
            treeRotateLRSized(p, subtree_size);
            promote(*p);
            promote(*p);
            demote((*p)->r);
            if (i <= (*p)->s) {
               tree_pathLeftSized(&p, &subtree_size);
            } else {
               tree_pathRightSized(&p, &subtree_size, &i);
            }
         }
      } else {
         u64 right_size = sizeR(*p, subtree_size);
         u64 right_rank = rank_in_right_subtree(i, left_size);
         (void)tree_insert_inputs_require((*p)->r, right_rank, right_size);
         if ((*p)->r == NULL) {
            tree_attachR(*p, x);
            return;
         }
         if (!isOneOne((*p)->r)) {
            tree_pathRightSized(&p, &subtree_size, &i);
            continue;
         }
         if (!isOneChild(*p, (*p)->r)) {
            tree_pathRightSized(&p, &subtree_size, &i);
            promote(*p);
            continue;
         }
         u64 right_left_boundary = rank_after_predecessor((*p)->s, (*p)->r);
         if (i > right_left_boundary) {
            u64 right_left_size = sizeL((*p)->r);
            u64 right_right_size = sizeR((*p)->r, right_size);
            u64 right_right_rank =
                rank_in_right_subtree(right_rank, right_left_size);
            (void)tree_insert_inputs_require((*p)->r->r, right_right_rank,
                                             right_right_size);
            if ((*p)->r->r == NULL) {
               tree_attachRR(*p, x);
               treeRotateLSized(p, size_after_inserting_one(subtree_size));
               promote(*p);
               demote((*p)->l);
               return;
            }
            if (!isOneOne((*p)->r->r)) {
               tree_pathRightSized(&p, &subtree_size, &i);
               tree_pathRightSized(&p, &subtree_size, &i);
               continue;
            }
            treeRotateLSized(p, subtree_size);
            promote(*p);
            demote((*p)->l);
            tree_pathRightSized(&p, &subtree_size, &i);
            promote(*p);
            continue;
         } else {
            u64 right_left_size = sizeL((*p)->r);
            (void)tree_insert_inputs_require((*p)->r->l, right_rank,
                                             right_left_size);
            if ((*p)->r->l == NULL) {
               tree_attachRL(*p, x);
               treeRotateRLSized(p, size_after_inserting_one(subtree_size));
               demote((*p)->l);
               promote(*p);
               return;
            }
            if (!isOneOne((*p)->r->l)) {
               tree_pathRightSized(&p, &subtree_size, &i);
               tree_pathLeftSized(&p, &subtree_size);
               continue;
            }
            treeRotateRLSized(p, subtree_size);
            promote(*p);
            promote(*p);
            demote((*p)->l);
            if (i > (*p)->s) {
               tree_pathRightSized(&p, &subtree_size, &i);
            } else {
               tree_pathLeftSized(&p, &subtree_size);
            }
         }
      }
   }
}

static tree_split_result_t avlw_td_split_impl(node_t *root, u64 split_size,
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
          avlw_td_split_impl(root->l, split_size, left_size);

      u64 right_split_size = suffix_size_after_prefix(left_size, split_size);
      return (tree_split_result_t){
          .left = split.left,
          .right = avlw_build(split.right, root, root->r, right_split_size,
                              right_size),
      };
   }

   u64 right_rank = rank_in_right_subtree(split_size, left_size);
   tree_split_result_t split =
       avlw_td_split_impl(root->r, right_rank, right_size);
   return (tree_split_result_t){
       .left = avlw_build(root->l, root, split.left, left_size, right_rank),
       .right = split.right,
   };
}

static tree_split_result_t avlw_td_split(node_t *root, u64 split_size,
                                         u64 size) {
   (void)tree_split_inputs_require(root, split_size, size);
   if (root != NULL) {
      tree_debug_require_size(root, size);
   }
   return avlw_td_split_impl(root, split_size, size);
}

TREE_VT_SIMPLE(avl_weak_topdown_t, avlw_td)

static void avlw_td_vt_verify(list_t *self) {
   list_require(self);
   avl_weak_topdown_t *t = (avl_weak_topdown_t *)self->impl;
   verifyAVLWeakRanksSized(t->tree.root, t->tree.size);
   verifyAVLWeakHeight(t->tree.root, t->tree.size);
}
static void avlw_td_vt_insert(list_t *self, u64 i, u64 x) {
   list_require(self);
   avl_weak_topdown_t *t = (avl_weak_topdown_t *)self->impl;
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   avlw_td_insert(&t->tree.root, size, i, x);
   tree_list_sized_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
}
static u64 avlw_td_vt_delete(list_t *self, u64 i) {
   list_require(self);
   avl_weak_topdown_t *t = (avl_weak_topdown_t *)self->impl;
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   u64 x = avlw_td_delete(&t->tree.root, t->tree.size, i);
   tree_list_sized_deleted(&t->tree);
   invariant(t->tree.size == deleted_size);
   return x;
}
static void avlw_td_vt_split(list_t *self, u64 i, list_t *out_l,
                             list_t *out_r) {
   tree_list_require_split(self, i, out_l, out_r);
   avl_weak_topdown_t *t = (avl_weak_topdown_t *)self->impl;
   u64 size = t->tree.size;
   u64 right_size = tree_split_inputs_require(t->tree.root, i, size);
   tree_share(t->tree.root);
   tree_split_result_t split = avlw_td_split(t->tree.root, i, size);
   TREE_LIST_SPLIT_RESULT_PREPARED(avl_weak_topdown_t, size, split.left, i,
                                   split.right, right_size);
}
static list_t avlw_td_vt_join(list_t *self, list_t *other) {
   tree_list_require_join(self, other);
   avl_weak_topdown_t *t = (avl_weak_topdown_t *)self->impl;
   avl_weak_topdown_t *that = (avl_weak_topdown_t *)other->impl;
   tree_join_roots_t roots =
       tree_list_prepare_join_roots(&t->tree, &that->tree);
   node_t *root =
       avlw_td_join(roots.left, roots.right, t->tree.size, that->tree.size);
   u64 joined_size = tree_list_join_size(t->tree.size, that->tree.size);
   TREE_LIST_JOIN_RESULT(avl_weak_topdown_t, root, joined_size);
}
const list_vtable_t avl_weak_topdown_vtable =
    TREE_LIST_VTABLE("AVLWeakTopDown", avlw_td);

// ═══════════════════════════════════════════════════════════════════════════════
