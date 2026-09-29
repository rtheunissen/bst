#include "wbst_relaxed.h"
#include "wbst_bottomup.h"
#include "tree_list_adapter.h"

// Forward declaration
static node_t *wbst_rlx_join_impl(weight_balance_t wb, node_t *l, node_t *r,
                                  u64 sl, u64 sr);

static void wbst_rlx_require_balance(weight_balance_t wb) {
   weight_balance_require(wb);
}

static node_t *wbst_rlx_balance(weight_balance_t wb, node_t *p, u64 s) {
   wbst_rlx_require_balance(wb);
   tree_sized_root_shape_require(p, s);
   if (s < 4) {
      return p;
   }
   invariant(p != NULL);
   invariant(p->s < s);
   tree_persist_unshared(&p);
   if (!wb.isBalanced(sizeL(p), sizeR(p, s)) ||
       !wb.isBalanced(sizeR(p, s), sizeL(p))) {
      p = tree_partition_sized(p, s, s >> 1);
   }
   p->l = wbst_rlx_balance(wb, p->l, sizeL(p));
   p->r = wbst_rlx_balance(wb, p->r, sizeR(p, s));
   return p;
}

static void
wbst_rlx_note_unbalanced_insert(weight_balance_t wb, node_t ***unbalanced_node,
                                u64 *unbalanced_size, node_t **candidate,
                                u64 candidate_size, u64 stable_side_size,
                                u64 growing_side_size) {
   wbst_rlx_require_balance(wb);
   invariant(unbalanced_node != NULL);
   invariant(unbalanced_size != NULL);
   invariant(candidate != NULL);
   invariant(*candidate != NULL);
   invariant(candidate_size > 0);
   invariant(stable_side_size <= candidate_size);
   invariant(growing_side_size <= candidate_size);
   invariant(tree_size_from_child_sizes(stable_side_size, growing_side_size) ==
             candidate_size);

   if (*unbalanced_node != NULL) {
      return;
   }
   if (!wb.isBalanced(stable_side_size, growing_side_size)) {
      *unbalanced_node = candidate;
      *unbalanced_size = candidate_size;
   }
}

void wbst_rlx_insert(weight_balance_t wb, node_t **p, u64 s, u64 i, u64 x,
                     u64 tree_size) {
   wbst_rlx_require_balance(wb);
   invariant(p != NULL);
   invariant(i <= s);
   invariant(s <= tree_size);
   tree_debug_require_size(*p, s);
   node_t **unbalanced_node = NULL;
   u64 unbalanced_size = 0;
   u64 depth = 0;
   u64 inserted_tree_size = sequence_insert_inputs_require(i, tree_size);
   for (;;) {
      (void)tree_insert_inputs_require(*p, i, s);
      if (*p == NULL) {
         invariant(s == 0);
         invariant(i == 0);
         *p = tree_allocate((node_t){.x = x});
         int max_height = wb.maxHeight(inserted_tree_size);
         invariant(max_height >= 0);
         if (depth > (u64)max_height) {
            invariant(unbalanced_node != NULL);
            invariant(*unbalanced_node != NULL);
            invariant(unbalanced_size > 0);
            *unbalanced_node =
                wbst_rlx_balance(wb, *unbalanced_node, unbalanced_size);
         }
         return;
      }
      tree_persist_unshared(p);
      depth = size_after_inserting_one(depth);
      u64 sl = sizeL(*p), sr = sizeR(*p, s);
      u64 inserted_size = sequence_insert_inputs_require(i, s);
      if (i <= sl) {
         u64 inserted_sl = sequence_insert_inputs_require(i, sl);
         wbst_rlx_note_unbalanced_insert(wb, &unbalanced_node, &unbalanced_size,
                                         p, inserted_size, sr, inserted_sl);
         p = insertL(*p);
         s = sl;
      } else {
         u64 right_i = rank_in_right_subtree(i, sl);
         u64 inserted_sr = sequence_insert_inputs_require(right_i, sr);
         wbst_rlx_note_unbalanced_insert(wb, &unbalanced_node, &unbalanced_size,
                                         p, inserted_size, sl, inserted_sr);
         p = insertR(*p, &i);
         invariant(i == right_i);
         s = sr;
      }
   }
}

static node_t *wbst_rlx_join_left_heavy(weight_balance_t wb, node_t *l,
                                        node_t *r, u64 sl, u64 sr) {
   wbst_rlx_require_balance(wb);
   tree_join_inputs_require(l, r, sl, sr);
   if (r == NULL) {
      invariant(sr == 0);
      return l;
   }
   if (wb.isBalanced(sr, sl)) {
      invariant(l != NULL);
      invariant(sl > 0);
      node_t *p = tree_deleteMax_sized(&l, sl);
      invariant(p != NULL);
      p->l = l;
      p->r = r;
      p->s = rank_of_last(sl);
      return p;
   }
   tree_nonempty_root_shape_require(l, sl);
   tree_persist_unshared(&l);
   l->r = wbst_rlx_join_impl(wb, l->r, r, sizeR(l, sl), sr);
   return l;
}

static node_t *wbst_rlx_join_right_heavy(weight_balance_t wb, node_t *l,
                                         node_t *r, u64 sl, u64 sr) {
   wbst_rlx_require_balance(wb);
   tree_join_inputs_require(l, r, sl, sr);
   if (l == NULL) {
      invariant(sl == 0);
      return r;
   }
   if (wb.isBalanced(sl, sr)) {
      invariant(r != NULL);
      invariant(sr > 0);
      node_t *p = tree_deleteMin_sized(&r, sr);
      invariant(p != NULL);
      p->l = l;
      p->r = r;
      p->s = sl;
      return p;
   }
   tree_nonempty_root_shape_require(r, sr);
   tree_persist_unshared(&r);
   r->l = wbst_rlx_join_impl(wb, l, r->l, sl, r->s);
   r->s = rank_after_prefix(sl, r);
   return r;
}

static node_t *wbst_rlx_join_impl(weight_balance_t wb, node_t *l, node_t *r,
                                  u64 sl, u64 sr) {
   wbst_rlx_require_balance(wb);
   tree_join_inputs_require(l, r, sl, sr);
   if (sl > sr) {
      return wbst_rlx_join_left_heavy(wb, l, r, sl, sr);
   }
   return wbst_rlx_join_right_heavy(wb, l, r, sl, sr);
}

node_t *wbst_rlx_join(weight_balance_t wb, node_t *l, node_t *r, u64 sl,
                      u64 sr) {
   wbst_rlx_require_balance(wb);
   tree_join_inputs_require(l, r, sl, sr);
#ifdef DEBUG
   tree_debug_require_join_inputs(l, r, sl, sr);
#endif
   return wbst_rlx_join_impl(wb, l, r, sl, sr);
}

void wbst_rlx_verify(weight_balance_t wb, tree_t *tree) {
   wbst_rlx_require_balance(wb);
   invariant(tree != NULL);
   tree_sized_handle_require(*tree);
   node_path_metrics_t metrics =
       tree_verify_size_path_metrics(tree->root, tree->size);
   if (tree->root == NULL) {
      invariant(metrics.nodes == 0);
      return;
   }
   invariant(nodeMaximumPathLengthFromMetrics(metrics) <=
             wb.maxHeight(tree->size));
}

void wbst_rlx_rebalance_if_too_tall(weight_balance_t wb, tree_t *tree) {
   wbst_rlx_require_balance(wb);
   invariant(tree != NULL);
   tree_sized_handle_require(*tree);
   if (tree->root == NULL) {
      return;
   }
   tree_debug_require_size(tree->root, tree->size);
   int max_height = wb.maxHeight(tree->size);
   if (nodeHeightExceeds(tree->root, max_height)) {
      tree->root = wbst_rlx_balance(wb, tree->root, tree->size);
   }
#ifdef DEBUG
   wbst_rlx_verify(wb, tree);
#endif
}

u64 wbst_rlx_delete(weight_balance_t wb, tree_t *tree, u64 i) {
   wbst_rlx_require_balance(wb);
   invariant(tree != NULL);
   u64 size = tree->size;
   u64 deleted_size = tree_delete_inputs_require(tree->root, i, size);

   int max_height_before = wb.maxHeight(size);
   u64 x = tree_delete_base(tree, i);
   invariant(tree->size == deleted_size);
   if (tree->root != NULL && wb.maxHeight(tree->size) < max_height_before) {
      wbst_rlx_rebalance_if_too_tall(wb, tree);
   }
   return x;
}

WB_VT_BODY(
    wbst_relaxed_t, wbst_rlx, wb_three_two,
    {
       wbst_relaxed_t *t = (wbst_relaxed_t *)self->impl;
       wbst_rlx_insert(t->wb, &t->tree.root, t->tree.size, i, x, t->tree.size);
       tree_list_sized_inserted(&t->tree);
    },
    {
       wbst_relaxed_t *t = (wbst_relaxed_t *)self->impl;
       return wbst_rlx_delete(t->wb, &t->tree, i);
    },
    {
       wbst_relaxed_t *t = (wbst_relaxed_t *)self->impl;
       u64 size = t->tree.size;
       tree_share(t->tree.root);
       tree_split_result_t split =
           tree_split_roots_sized(t->tree.root, i, size);
       tree_t left = tree_list_make_sized_tree(split.left, i);
       u64 right_size = tree_list_split_right_size(size, i);
       tree_t right = tree_list_make_sized_tree(split.right, right_size);
       wbst_rlx_rebalance_if_too_tall(t->wb, &left);
       wbst_rlx_rebalance_if_too_tall(t->wb, &right);
       TREE_LIST_SPLIT_RESULT_FIELD_PREPARED(wbst_relaxed_t, size, wb, t->wb,
                                             left.root, left.size, right.root,
                                             right.size);
    },
    {
       wbst_relaxed_t *t = (wbst_relaxed_t *)self->impl;
       wbst_relaxed_t *that = (wbst_relaxed_t *)other->impl;
       weight_balance_require_equal(t->wb, that->wb);
       tree_join_roots_t roots =
           tree_list_prepare_join_roots(&t->tree, &that->tree);
       u64 joined_size = tree_list_join_size(t->tree.size, that->tree.size);
       node_t *joined_root = wbst_rlx_join(t->wb, roots.left, roots.right,
                                           t->tree.size, that->tree.size);
       tree_t joined = tree_list_make_sized_tree(joined_root, joined_size);
       wbst_rlx_rebalance_if_too_tall(t->wb, &joined);
       TREE_LIST_JOIN_RESULT_FIELD(wbst_relaxed_t, wb, t->wb, joined.root,
                                   joined.size);
    },
    {
       wbst_relaxed_t *t = (wbst_relaxed_t *)self->impl;
       wbst_rlx_verify(t->wb, &t->tree);
    })
const list_vtable_t wbst_relaxed_vtable =
    TREE_LIST_VTABLE("WBSTRelaxed", wbst_rlx);
