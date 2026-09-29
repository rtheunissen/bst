#include "wbst_topdown.h"
#include "wbst_bottomup.h"
#include "tree_list_adapter.h"

static void wbst_td_require_balance(weight_balance_t wb) {
   weight_balance_require(wb);
}

static void wbst_td_pathL(node_t ***p, u64 *s) {
   invariant(p != NULL);
   invariant(*p != NULL);
   invariant(**p != NULL);
   invariant(s != NULL);
   tree_nonempty_root_shape_require(**p, *s);
   invariant(!tree_shared(**p));
   invariant((**p)->s < UINT64_MAX);
   *s = (**p)->s;
   (**p)->s = size_after_inserting_one((**p)->s);
   *p = &(**p)->l;
}

static void wbst_td_pathR(node_t ***p, u64 *s, u64 *i) {
   invariant(p != NULL);
   invariant(*p != NULL);
   invariant(**p != NULL);
   invariant(s != NULL);
   invariant(i != NULL);
   tree_nonempty_root_shape_require(**p, *s);
   invariant(!tree_shared(**p));
   invariant((**p)->s < *s);
   invariant(*i > (**p)->s);
   u64 sr = sizeR(**p, *s);
   *i = rank_in_right_subtree(*i, (**p)->s);
   *s = sr;
   *p = &(**p)->r;
}

static void wbst_td_deleteL(node_t ***p, u64 *s) {
   invariant(p != NULL);
   invariant(*p != NULL);
   invariant(**p != NULL);
   invariant(s != NULL);
   tree_nonempty_root_shape_require(**p, *s);
   invariant(!tree_shared(**p));
   invariant((**p)->s > 0);
   *s = (**p)->s;
   (**p)->s = size_after_removing_one((**p)->s);
   *p = &(**p)->l;
}

static void wbst_td_deleteR(node_t ***p, u64 *s, u64 *i) {
   invariant(p != NULL);
   invariant(*p != NULL);
   invariant(**p != NULL);
   invariant(s != NULL);
   invariant(i != NULL);
   tree_nonempty_root_shape_require(**p, *s);
   invariant(!tree_shared(**p));
   invariant((**p)->s < *s);
   invariant(*i > (**p)->s);
   u64 sr = sizeR(**p, *s);
   *i = rank_in_right_subtree(*i, (**p)->s);
   *s = sr;
   *p = &(**p)->r;
}

static void wbst_td_rebalanceR(weight_balance_t wb, node_t **p, u64 size,
                               u64 sr) {
   wbst_td_require_balance(wb);
   invariant(p != NULL);
   invariant(*p != NULL);
   tree_nonempty_root_shape_require(*p, size);
   tree_nonempty_root_shape_require((*p)->r, sr);
   invariant(!tree_shared(*p));
   if (wb.singleRotation(sizeR((*p)->r, sr), sizeL((*p)->r))) {
      treeRotateLSized(p, size);
   } else {
      treeRotateRLSized(p, size);
   }
}

static void wbst_td_rebalanceL(weight_balance_t wb, node_t **p, u64 size,
                               u64 sl) {
   wbst_td_require_balance(wb);
   invariant(p != NULL);
   invariant(*p != NULL);
   tree_nonempty_root_shape_require(*p, size);
   tree_nonempty_root_shape_require((*p)->l, sl);
   invariant(!tree_shared(*p));
   if (wb.singleRotation(sizeL((*p)->l), sizeR((*p)->l, sl))) {
      treeRotateRSized(p, size);
   } else {
      treeRotateLRSized(p, size);
   }
}

void wbst_td_insert(weight_balance_t wb, node_t **p, u64 s, u64 i, u64 x) {
   wbst_td_require_balance(wb);
   invariant(p != NULL);
   for (;;) {
      (void)tree_insert_inputs_require(*p, i, s);
      if (*p == NULL) {
         invariant(s == 0);
         invariant(i == 0);
         *p = tree_allocate((node_t){.x = x});
         return;
      }
      tree_persist_unshared(p);
      u64 sl = sizeL(*p), sr = sizeR(*p, s);
      u64 inserted_sl = size_after_inserting_one(sl);
      u64 inserted_sr = size_after_inserting_one(sr);
      if (i <= (*p)->s) {
         if (wb.isBalanced(sr, inserted_sl)) {
            wbst_td_pathL(&p, &s);
         } else {
            invariant((*p)->l != NULL);
            if (i <= (*p)->l->s) {
               u64 inserted_left_left_size =
                   size_after_inserting_one(sizeL((*p)->l));
               if (wb.singleRotation(inserted_left_left_size,
                                     sizeR((*p)->l, sl))) {
                  treeRotateRSized(p, s);
                  wbst_td_pathL(&p, &s);
               } else {
                  treeRotateLRSized(p, s);
                  wbst_td_pathL(&p, &s);
                  wbst_td_pathL(&p, &s);
               }
            } else {
               u64 inserted_left_right_size =
                   size_after_inserting_one(sizeR((*p)->l, sl));
               if (wb.singleRotation(sizeL((*p)->l),
                                     inserted_left_right_size)) {
                  treeRotateRSized(p, s);
                  wbst_td_pathR(&p, &s, &i);
                  wbst_td_pathL(&p, &s);
               } else {
                  u64 left_right_boundary =
                      rank_after_predecessor((*p)->l->s, (*p)->l->r);
                  if (i <= left_right_boundary) {
                     treeRotateLRSized(p, s);
                     wbst_td_pathL(&p, &s);
                     wbst_td_pathR(&p, &s, &i);
                  } else {
                     treeRotateLRSized(p, s);
                     wbst_td_pathR(&p, &s, &i);
                     wbst_td_pathL(&p, &s);
                  }
               }
            }
         }
      } else {
         if (wb.isBalanced(sl, inserted_sr)) {
            wbst_td_pathR(&p, &s, &i);
            continue;
         }
         invariant((*p)->r != NULL);
         u64 right_left_boundary = rank_after_predecessor((*p)->s, (*p)->r);
         if (i > right_left_boundary) {
            u64 inserted_right_right_size =
                size_after_inserting_one(sizeR((*p)->r, sr));
            if (wb.singleRotation(inserted_right_right_size, sizeL((*p)->r))) {
               treeRotateLSized(p, s);
               wbst_td_pathR(&p, &s, &i);
            } else {
               treeRotateRLSized(p, s);
               wbst_td_pathR(&p, &s, &i);
               wbst_td_pathR(&p, &s, &i);
            }
         } else {
            u64 inserted_right_left_size =
                size_after_inserting_one(sizeL((*p)->r));
            if (wb.singleRotation(sizeR((*p)->r, sr),
                                  inserted_right_left_size)) {
               treeRotateLSized(p, s);
               wbst_td_pathL(&p, &s);
               wbst_td_pathR(&p, &s, &i);
            } else {
               u64 right_left_left_boundary =
                   rank_after_predecessor((*p)->s, (*p)->r->l);
               if (i > right_left_left_boundary) {
                  treeRotateRLSized(p, s);
                  wbst_td_pathR(&p, &s, &i);
                  wbst_td_pathL(&p, &s);
               } else {
                  treeRotateRLSized(p, s);
                  wbst_td_pathL(&p, &s);
                  wbst_td_pathR(&p, &s, &i);
               }
            }
         }
      }
   }
}

node_t *wbst_td_delete_internal(weight_balance_t wb, node_t **p, u64 s, u64 i) {
   wbst_td_require_balance(wb);
   invariant(p != NULL);
   for (;;) {
      invariant(*p != NULL);
      tree_rank_inputs_require(*p, i, s);
      u64 sl = sizeL(*p), sr = sizeR(*p, s);
      if (i == (*p)->s) {
         node_t *old = *p;
         node_t *l = old->l;
         node_t *r = old->r;
         if (tree_shared(old)) {
            tree_share(l);
            tree_share(r);
            node_t *deleted = tree_allocate((node_t){.x = old->x, .y = old->y});
            *p = wbst_bu_join(wb, l, r, sl, sr);
            tree_release(old);
            return deleted;
         } else {
            *p = wbst_bu_join(wb, l, r, sl, sr);
            old->l = NULL;
            old->r = NULL;
            old->s = 0;
            return old;
         }
      }
      tree_persist_unshared(p);
      node_t *node = *p;
      invariant(node != NULL);
      if (i <= node->s) {
         invariant(i < node->s);
         invariant(sl > 0);
         if (wb.isBalanced(size_after_removing_one(sl), sr)) {
            wbst_td_deleteL(&p, &s);
         } else {
            wbst_td_rebalanceR(wb, p, s, sr);
            wbst_td_deleteL(&p, &s);
            wbst_td_deleteL(&p, &s);
         }
      } else {
         invariant(i > node->s);
         invariant(sr > 0);
         if (wb.isBalanced(size_after_removing_one(sr), sl)) {
            wbst_td_deleteR(&p, &s, &i);
         } else {
            wbst_td_rebalanceL(wb, p, s, sl);
            wbst_td_deleteR(&p, &s, &i);
            wbst_td_deleteR(&p, &s, &i);
         }
      }
   }
}

static node_t *wbst_td_buildL(weight_balance_t wb, node_t *p, node_t *l,
                              node_t *r, u64 sl, u64 sr) {
   wbst_td_require_balance(wb);
   tree_build_inputs_require(l, p, r, sl, sr);
   if (wb.isBalanced(sr, sl)) {
      p->l = l;
      p->r = r;
      p->s = sl;
      return p;
   }
   tree_nonempty_root_shape_require(l, sl);
   tree_persist_unshared(&l);
   u64 sll = sizeL(l), slr = sizeR(l, sl);
   l->r = wbst_td_buildL(wb, p, l->r, r, slr, sr);
   slr = tree_size_from_child_sizes(slr, sr);
   if (!wb.isBalanced(sll, slr)) {
      wbst_td_rebalanceR(wb, &l, tree_size_from_child_sizes(sll, slr), slr);
   }
   return l;
}

static node_t *wbst_td_buildR(weight_balance_t wb, node_t *p, node_t *l,
                              node_t *r, u64 sl, u64 sr) {
   wbst_td_require_balance(wb);
   tree_build_inputs_require(l, p, r, sl, sr);
   if (wb.isBalanced(sl, sr)) {
      p->l = l;
      p->r = r;
      p->s = sl;
      return p;
   }
   tree_nonempty_root_shape_require(r, sr);
   tree_persist_unshared(&r);
   u64 srl = sizeL(r), srr = sizeR(r, sr);
   r->l = wbst_td_buildR(wb, p, l, r->l, sl, srl);
   r->s = rank_after_predecessor(sl, r);
   if (!wb.isBalanced(srr, r->s)) {
      wbst_td_rebalanceL(wb, &r, tree_size_from_child_sizes(r->s, srr), r->s);
   }
   return r;
}

node_t *wbst_td_build(weight_balance_t wb, node_t *l, node_t *p, node_t *r,
                      u64 sl, u64 sr) {
   wbst_td_require_balance(wb);
   tree_build_inputs_require(l, p, r, sl, sr);
   tree_debug_require_size(l, sl);
   tree_debug_require_size(r, sr);
   tree_persist_unshared(&p);
   if (sl <= sr) {
      return wbst_td_buildR(wb, p, l, r, sl, sr);
   }
   return wbst_td_buildL(wb, p, l, r, sl, sr);
}

node_t *wbst_td_join(weight_balance_t wb, node_t *l, node_t *r, u64 sl,
                     u64 sr) {
   wbst_td_require_balance(wb);
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
   if (sl <= sr) {
      invariant(sr > 0);
      node_t *min = wbst_td_delete_internal(wb, &r, sr, 0);
      invariant(min != NULL);
      return wbst_td_build(wb, l, min, r, sl, size_after_removing_one(sr));
   } else {
      invariant(sl > 0);
      node_t *max = wbst_td_delete_internal(wb, &l, sl, rank_of_last(sl));
      invariant(max != NULL);
      return wbst_td_build(wb, l, max, r, rank_of_last(sl), sr);
   }
}

static tree_split_result_t wbst_td_split_impl(weight_balance_t wb, node_t *root,
                                              u64 split_size, u64 size) {
   wbst_td_require_balance(wb);
   (void)tree_split_inputs_require(root, split_size, size);
   if (root == NULL) {
      return (tree_split_result_t){.left = NULL, .right = NULL};
   }
   tree_persist_unshared(&root);
   u64 left_size = sizeL(root);
   u64 right_size = sizeR(root, size);
   if (split_size <= root->s) {
      tree_split_result_t split =
          wbst_td_split_impl(wb, root->l, split_size, left_size);

      u64 right_split_size = suffix_size_after_prefix(left_size, split_size);
      return (tree_split_result_t){
          .left = split.left,
          .right = wbst_td_build(wb, split.right, root, root->r,
                                 right_split_size, right_size),
      };
   }

   u64 right_rank = rank_in_right_subtree(split_size, left_size);
   tree_split_result_t split =
       wbst_td_split_impl(wb, root->r, right_rank, right_size);
   return (tree_split_result_t){
       .left =
           wbst_td_build(wb, root->l, root, split.left, left_size, right_rank),
       .right = split.right,
   };
}

tree_split_result_t wbst_td_split(weight_balance_t wb, node_t *root,
                                  u64 split_size, u64 size) {
   wbst_td_require_balance(wb);
   (void)tree_split_inputs_require(root, split_size, size);
   if (root != NULL) {
      tree_debug_require_size(root, size);
   }
   return wbst_td_split_impl(wb, root, split_size, size);
}

void wbst_td_verifyBalance(weight_balance_t wb, node_t *p, u64 s) {
   wbst_verifyBalance(wb, p, s, 3);
}

static void wbst_td_insert_op(wbst_topdown_t *t, u64 i, u64 x) {
   invariant(t != NULL);
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   wbst_td_insert(t->wb, &t->tree.root, t->tree.size, i, x);
   tree_list_sized_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
}

static u64 wbst_td_delete_op(wbst_topdown_t *t, u64 i) {
   invariant(t != NULL);
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   node_t *deleted =
       wbst_td_delete_internal(t->wb, &t->tree.root, t->tree.size, i);
   invariant(deleted != NULL);
   tree_list_sized_deleted(&t->tree);
   invariant(t->tree.size == deleted_size);
   u64 x = deleted->x;
   tree_release(deleted);
   return x;
}

static void wbst_td_verify_tree(wbst_topdown_t *t) {
   invariant(t != NULL);
   wbst_td_verifyBalance(t->wb, t->tree.root, t->tree.size);
}

WB_VT_BODY(
    wbst_topdown_t, wbst_td, wb_three_two,
    { wbst_td_insert_op((wbst_topdown_t *)self->impl, i, x); },
    { return wbst_td_delete_op((wbst_topdown_t *)self->impl, i); },
    { WBST_TD_SPLIT(wbst_topdown_t); }, { WBST_TD_JOIN(wbst_topdown_t); },
    { wbst_td_verify_tree((wbst_topdown_t *)self->impl); })
const list_vtable_t wbst_topdown_vtable =
    TREE_LIST_VTABLE("WBSTTopDown", wbst_td);
