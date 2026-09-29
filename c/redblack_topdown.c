#include "redblack_topdown.h"
#include "redblack_bottomup.h"
#include "tree_list_adapter.h"

void rb_td_insert(node_t **p, u64 size, u64 i, u64 x) {
   invariant(p != NULL);
   (void)tree_insert_inputs_require(*p, i, size);
   if (*p == NULL) {
      invariant(i == 0);
      tree_attach(p, x);
      return;
   }
   tree_persist_unshared(p);
   if (isRedRed(*p)) {
      promote(*p);
   }
   u64 subtree_size = size;
   for (;;) {
      invariant((*p)->s < subtree_size);
      (void)tree_insert_inputs_require(*p, i, subtree_size);
      u64 left_size = sizeL(*p);
      assert_bst(!isRedRed(*p));
      if (i <= left_size) {
         (void)tree_insert_inputs_require((*p)->l, i, left_size);
         if ((*p)->l == NULL) {
            tree_attachL(*p, x);
            return;
         }
         if (isRedRed((*p)->l)) {
            tree_pathLeftSized(&p, &subtree_size);
            promote(*p);
            continue;
         }
         if (!isRed(*p, (*p)->l)) {
            tree_pathLeftSized(&p, &subtree_size);
            continue;
         }
         assert_bst(isRed(*p, (*p)->l) && !isRedRed((*p)->l));
         u64 left_left_size = sizeL((*p)->l);
         if (i <= left_left_size) {
            (void)tree_insert_inputs_require((*p)->l->l, i, left_left_size);
            if ((*p)->l->l == NULL) {
               tree_attachLL(*p, x);
               if (isRed((*p)->l, (*p)->l->l)) {
                  treeRotateRSized(p, size_after_inserting_one(subtree_size));
               }
               return;
            }
            if (!isBlack((*p)->l, (*p)->l->l)) {
               tree_pathLeftSized(&p, &subtree_size);
               tree_pathLeftSized(&p, &subtree_size);
               promote(*p);
               continue;
            }
            if (!isRedRed((*p)->l->l)) {
               tree_pathLeftSized(&p, &subtree_size);
               tree_pathLeftSized(&p, &subtree_size);
               continue;
            }
            treeRotateRSized(p, subtree_size);
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
               if (isRed((*p)->l, (*p)->l->r)) {
                  treeRotateLRSized(p, size_after_inserting_one(subtree_size));
               }
               return;
            }
            if (!isRedRed((*p)->l->r)) {
               tree_pathLeftSized(&p, &subtree_size);
               tree_pathRightSized(&p, &subtree_size, &i);
               continue;
            }
            if (!isBlack((*p)->l, (*p)->l->r)) {
               tree_pathLeftSized(&p, &subtree_size);
               tree_pathRightSized(&p, &subtree_size, &i);
               promote(*p);
               continue;
            }
            treeRotateLRSized(p, subtree_size);
            promote(*p);
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
         if (!isRedRed((*p)->r) && !isRed(*p, (*p)->r)) {
            tree_pathRightSized(&p, &subtree_size, &i);
            continue;
         }
         if (isRedRed((*p)->r)) {
            tree_pathRightSized(&p, &subtree_size, &i);
            promote(*p);
            continue;
         }
         u64 right_left_boundary = rank_after_predecessor((*p)->s, (*p)->r);
         if (i > right_left_boundary) {
            u64 right_right_size = sizeR((*p)->r, right_size);
            u64 right_right_rank =
                rank_in_right_subtree(right_rank, sizeL((*p)->r));
            (void)tree_insert_inputs_require((*p)->r->r, right_right_rank,
                                             right_right_size);
            if ((*p)->r->r == NULL) {
               tree_attachRR(*p, x);
               if (isRed((*p)->r, (*p)->r->r)) {
                  treeRotateLSized(p, size_after_inserting_one(subtree_size));
               }
               return;
            }
            if (!isRedRed((*p)->r->r)) {
               tree_pathRightSized(&p, &subtree_size, &i);
               tree_pathRightSized(&p, &subtree_size, &i);
               continue;
            }
            if (!isOneChild((*p)->r, (*p)->r->r)) {
               tree_pathRightSized(&p, &subtree_size, &i);
               tree_pathRightSized(&p, &subtree_size, &i);
               promote(*p);
               continue;
            }
            treeRotateLSized(p, subtree_size);
            tree_pathRightSized(&p, &subtree_size, &i);
            promote(*p);
            continue;
         } else {
            u64 right_left_size = sizeL((*p)->r);
            (void)tree_insert_inputs_require((*p)->r->l, right_rank,
                                             right_left_size);
            if ((*p)->r->l == NULL) {
               tree_attachRL(*p, x);
               if (isRed((*p)->r, (*p)->r->l)) {
                  treeRotateRLSized(p, size_after_inserting_one(subtree_size));
               }
               return;
            }
            if (!isRedRed((*p)->r->l)) {
               tree_pathRightSized(&p, &subtree_size, &i);
               tree_pathLeftSized(&p, &subtree_size);
               continue;
            }
            if (!isBlack((*p)->r, (*p)->r->l)) {
               tree_pathRightSized(&p, &subtree_size, &i);
               tree_pathLeftSized(&p, &subtree_size);
               promote(*p);
               continue;
            }
            treeRotateRLSized(p, subtree_size);
            promote(*p);
            if (i > (*p)->s) {
               tree_pathRightSized(&p, &subtree_size, &i);
            } else {
               tree_pathLeftSized(&p, &subtree_size);
            }
         }
      }
   }
}

TREE_VT_SIMPLE(redblack_topdown_t, rb_td)
static void rb_td_vt_verify(list_t *self) {
   list_require(self);
   redblack_topdown_t *t = (redblack_topdown_t *)self->impl;
   verifyRedBlackRanksSized(t->tree.root, t->tree.size);
   verifyRedBlackHeight(t->tree.root, t->tree.size);
}
static void rb_td_vt_insert(list_t *self, u64 i, u64 x) {
   list_require(self);
   redblack_topdown_t *t = (redblack_topdown_t *)self->impl;
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   rb_td_insert(&t->tree.root, size, i, x);
   tree_list_sized_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
}
static u64 rb_td_vt_delete(list_t *self, u64 i) {
   list_require(self);
   redblack_topdown_t *t = (redblack_topdown_t *)self->impl;
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   rb_bu_delete_result_t result = rb_bu_delete(t->tree.root, t->tree.size, i);
   t->tree.root = result.root;
   tree_list_sized_deleted(&t->tree);
   invariant(t->tree.size == deleted_size);
   return result.value;
}
static void rb_td_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   tree_list_require_split(self, i, out_l, out_r);
   redblack_topdown_t *t = (redblack_topdown_t *)self->impl;
   u64 size = t->tree.size;
   u64 right_size = tree_split_inputs_require(t->tree.root, i, size);
   tree_share(t->tree.root);
   tree_split_result_t split = rb_split(t->tree.root, size, i);
   TREE_LIST_SPLIT_RESULT_PREPARED(redblack_topdown_t, size, split.left, i,
                                   split.right, right_size);
}
static list_t rb_td_vt_join(list_t *self, list_t *other) {
   tree_list_require_join(self, other);
   redblack_topdown_t *t = (redblack_topdown_t *)self->impl;
   redblack_topdown_t *that = (redblack_topdown_t *)other->impl;
   tree_join_roots_t roots =
       tree_list_prepare_join_roots(&t->tree, &that->tree);
   node_t *root =
       rb_join(roots.left, roots.right, t->tree.size, that->tree.size);
   u64 joined_size = tree_list_join_size(t->tree.size, that->tree.size);
   TREE_LIST_JOIN_RESULT(redblack_topdown_t, root, joined_size);
}
const list_vtable_t redblack_topdown_vtable =
    TREE_LIST_VTABLE("RedBlackTopDown", rb_td);

// ═══════════════════════════════════════════════════════════════════════════════
