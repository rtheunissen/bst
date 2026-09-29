#include "wbst_bottomup.h"
#include "tree_list_adapter.h"

static void wbst_require_balance(weight_balance_t wb) {
   weight_balance_require(wb);
}

typedef struct {
   node_t *root;
   node_t *node;
} wbst_bu_extract_result_t;

static wbst_bu_extract_result_t wbst_bu_extractMin(weight_balance_t wb,
                                                   node_t *p, u64 s) {
   wbst_require_balance(wb);
   tree_nonempty_root_shape_require(p, s);
   tree_persist_unshared(&p);
   if (p->l == NULL) {
      node_t *r = p->r;
      p->r = NULL;
      return (wbst_bu_extract_result_t){.root = r, .node = p};
   }
   u64 sl = sizeL(p), sr = sizeR(p, s);
   invariant(sl > 0);
   node_t **l = deleteL(p);
   wbst_bu_extract_result_t result = wbst_bu_extractMin(wb, *l, sl);
   *l = result.root;
   u64 deleted_size = size_after_removing_one(s);
   if (!wb.isBalanced(size_after_removing_one(sl), sr)) {
      tree_nonempty_root_shape_require(p->r, sr);
      u64 srl = sizeL(p->r), srr = sizeR(p->r, sr);
      if (wb.singleRotation(srr, srl)) {
         treeRotateLSized(&p, deleted_size);
      } else {
         treeRotateRLSized(&p, deleted_size);
      }
   }
   result.root = p;
   return result;
}

static wbst_bu_extract_result_t wbst_bu_extractMax(weight_balance_t wb,
                                                   node_t *p, u64 s) {
   wbst_require_balance(wb);
   tree_nonempty_root_shape_require(p, s);
   tree_persist_unshared(&p);
   if (p->r == NULL) {
      node_t *l = p->l;
      p->l = NULL;
      return (wbst_bu_extract_result_t){.root = l, .node = p};
   }
   u64 sl = sizeL(p), sr = sizeR(p, s);
   invariant(sr > 0);
   wbst_bu_extract_result_t result = wbst_bu_extractMax(wb, p->r, sr);
   p->r = result.root;
   u64 deleted_size = size_after_removing_one(s);
   if (!wb.isBalanced(size_after_removing_one(sr), sl)) {
      tree_nonempty_root_shape_require(p->l, sl);
      if (wb.singleRotation(sizeL(p->l), sizeR(p->l, sl))) {
         treeRotateRSized(&p, deleted_size);
      } else {
         treeRotateLRSized(&p, deleted_size);
      }
   }
   result.root = p;
   return result;
}

static node_t *wbst_bu_buildL(weight_balance_t wb, node_t *p, node_t *l,
                              node_t *r, u64 sl, u64 sr) {
   wbst_require_balance(wb);
   u64 size = tree_build_inputs_require(l, p, r, sl, sr);
   if (wb.isBalanced(sr, sl)) {
      p->l = l;
      p->r = r;
      p->s = sl;
      return p;
   }
   tree_nonempty_root_shape_require(l, sl);
   tree_persist_unshared(&l);
   u64 sll = sizeL(l), slr = sizeR(l, sl);
   l->r = wbst_bu_buildL(wb, p, l->r, r, slr, sr);
   slr = tree_size_from_child_sizes(slr, sr);
   if (!wb.isBalanced(sll, slr)) {
      invariant(l->r != NULL);
      invariant(l->r->s < slr);
      u64 srr = sizeR(l->r, slr), srl = sizeL(l->r);
      if (wb.singleRotation(srr, srl)) {
         treeRotateLSized(&l, size);
      } else {
         treeRotateRLSized(&l, size);
      }
   }
   return l;
}

static node_t *wbst_bu_buildR(weight_balance_t wb, node_t *p, node_t *l,
                              node_t *r, u64 sl, u64 sr) {
   wbst_require_balance(wb);
   u64 size = tree_build_inputs_require(l, p, r, sl, sr);
   if (wb.isBalanced(sl, sr)) {
      p->l = l;
      p->r = r;
      p->s = sl;
      return p;
   }
   tree_nonempty_root_shape_require(r, sr);
   tree_persist_unshared(&r);
   u64 srl = sizeL(r), srr = sizeR(r, sr);
   r->l = wbst_bu_buildR(wb, p, l, r->l, sl, srl);
   r->s = rank_after_predecessor(sl, r);
   if (!wb.isBalanced(srr, r->s)) {
      invariant(r->l != NULL);
      invariant(r->l->s < r->s);
      if (wb.singleRotation(sizeL(r->l), sizeR(r->l, r->s))) {
         treeRotateRSized(&r, size);
      } else {
         treeRotateLRSized(&r, size);
      }
   }
   return r;
}

node_t *wbst_bu_build(weight_balance_t wb, node_t *l, node_t *p, node_t *r,
                      u64 sl, u64 sr) {
   wbst_require_balance(wb);
   tree_build_inputs_require(l, p, r, sl, sr);
   tree_debug_require_size(l, sl);
   tree_debug_require_size(r, sr);
   if (sl <= sr) {
      return wbst_bu_buildR(wb, p, l, r, sl, sr);
   }
   return wbst_bu_buildL(wb, p, l, r, sl, sr);
}

node_t *wbst_bu_join(weight_balance_t wb, node_t *l, node_t *r, u64 sl,
                     u64 sr) {
   wbst_require_balance(wb);
   tree_join_inputs_require(l, r, sl, sr);
#ifdef DEBUG
   tree_debug_require_join_inputs(l, r, sl, sr);
#endif
   node_t *k = NULL;
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
      wbst_bu_extract_result_t extracted = wbst_bu_extractMin(wb, r, sr);
      r = extracted.root;
      k = extracted.node;
      invariant(k != NULL);
      return wbst_bu_build(wb, l, k, r, sl, size_after_removing_one(sr));
   } else {
      invariant(sl > 0);
      wbst_bu_extract_result_t extracted = wbst_bu_extractMax(wb, l, sl);
      l = extracted.root;
      k = extracted.node;
      invariant(k != NULL);
      return wbst_bu_build(wb, l, k, r, rank_of_last(sl), sr);
   }
}

node_t *wbst_bu_insert(weight_balance_t wb, node_t *p, u64 s, u64 i, u64 x) {
   wbst_require_balance(wb);
   u64 inserted_size = tree_insert_inputs_require(p, i, s);
   if (p == NULL) {
      invariant(s == 0);
      invariant(i == 0);
      return tree_allocate((node_t){.x = x});
   }
   tree_persist_unshared(&p);
   u64 sl = sizeL(p), sr = sizeR(p, s);
   if (i <= p->s) {
      u64 inserted_sl = size_after_inserting_one(sl);
      node_t **l = insertL(p);
      *l = wbst_bu_insert(wb, *l, sl, i, x);
      if (!wb.isBalanced(sr, inserted_sl)) {
         invariant(p->l != NULL);
         invariant(p->l->s < p->s);
         if (!wb.singleRotation(sizeL(p->l), sizeR(p->l, p->s))) {
            treeRotateLRSized(&p, inserted_size);
         } else {
            treeRotateRSized(&p, inserted_size);
         }
      }
   } else {
      invariant(i > sl);
      u64 inserted_sr = size_after_inserting_one(sr);
      node_t **r = insertR(p, &i);
      *r = wbst_bu_insert(wb, *r, sr, i, x);
      if (!wb.isBalanced(sl, inserted_sr)) {
         invariant(p->r != NULL);
         invariant(p->r->s < inserted_sr);
         if (!wb.singleRotation(sizeR(p->r, inserted_sr), sizeL(p->r))) {
            treeRotateRLSized(&p, inserted_size);
         } else {
            treeRotateLSized(&p, inserted_size);
         }
      }
   }
   return p;
}

wbst_bu_delete_result_t wbst_bu_delete(weight_balance_t wb, node_t *p, u64 s,
                                       u64 i) {
   wbst_require_balance(wb);
   u64 deleted_size = tree_delete_inputs_require(p, i, s);
   u64 sl = sizeL(p), sr = sizeR(p, s);
   if (i == p->s) {
      u64 value = p->x;
      if (p->l == NULL) {
         node_t *r = p->r;
         tree_share(r);
         tree_release(p);
         return (wbst_bu_delete_result_t){.root = r, .value = value};
      }
      if (p->r == NULL) {
         node_t *l = p->l;
         tree_share(l);
         tree_release(p);
         return (wbst_bu_delete_result_t){.root = l, .value = value};
      }
      tree_persist_unshared(&p);
      if (sl > sr) {
         invariant(sl > 0);
         node_t **l = deleteL(p);
         wbst_bu_extract_result_t extracted = wbst_bu_extractMax(wb, *l, sl);
         *l = extracted.root;
         node_t *max = extracted.node;
         invariant(max != NULL);
         p->x = max->x;
         tree_release(max);
      } else {
         invariant(sr > 0);
         wbst_bu_extract_result_t extracted = wbst_bu_extractMin(wb, p->r, sr);
         p->r = extracted.root;
         node_t *min = extracted.node;
         invariant(min != NULL);
         p->x = min->x;
         tree_release(min);
      }
      return (wbst_bu_delete_result_t){.root = p, .value = value};
   }
   tree_persist_unshared(&p);
   wbst_bu_delete_result_t result;
   if (i < p->s) {
      invariant(p->l != NULL);
      invariant(sl > 0);
      node_t **l = deleteL(p);
      result = wbst_bu_delete(wb, *l, sl, i);
      *l = result.root;
      if (!wb.isBalanced(size_after_removing_one(sl), sr)) {
         tree_nonempty_root_shape_require(p->r, sr);
         u64 srl = sizeL(p->r), srr = sizeR(p->r, sr);
         if (wb.singleRotation(srr, srl)) {
            treeRotateLSized(&p, deleted_size);
         } else {
            treeRotateRLSized(&p, deleted_size);
         }
      }
   } else {
      invariant(i > sl);
      invariant(p->r != NULL);
      invariant(sr > 0);
      node_t **r = deleteR(p, &i);
      result = wbst_bu_delete(wb, *r, sr, i);
      *r = result.root;
      if (!wb.isBalanced(size_after_removing_one(sr), sl)) {
         tree_nonempty_root_shape_require(p->l, sl);
         u64 sll = sizeL(p->l), slr = sizeR(p->l, sl);
         if (wb.singleRotation(sll, slr)) {
            treeRotateRSized(&p, deleted_size);
         } else {
            treeRotateLRSized(&p, deleted_size);
         }
      }
   }
   result.root = p;
   return result;
}

static tree_split_result_t wbst_bu_split_impl(weight_balance_t wb, node_t *root,
                                              u64 split_size, u64 size) {
   wbst_require_balance(wb);
   (void)tree_split_inputs_require(root, split_size, size);
   if (root == NULL) {
      return (tree_split_result_t){.left = NULL, .right = NULL};
   }
   tree_persist_unshared(&root);
   u64 left_size = sizeL(root);
   u64 right_size = sizeR(root, size);
   if (split_size <= root->s) {
      tree_split_result_t split =
          wbst_bu_split_impl(wb, root->l, split_size, left_size);

      u64 right_split_size = suffix_size_after_prefix(left_size, split_size);
      return (tree_split_result_t){
          .left = split.left,
          .right = wbst_bu_build(wb, split.right, root, root->r,
                                 right_split_size, right_size),
      };
   }

   u64 right_rank = rank_in_right_subtree(split_size, left_size);
   tree_split_result_t split =
       wbst_bu_split_impl(wb, root->r, right_rank, right_size);
   return (tree_split_result_t){
       .left =
           wbst_bu_build(wb, root->l, root, split.left, left_size, right_rank),
       .right = split.right,
   };
}

tree_split_result_t wbst_bu_split(weight_balance_t wb, node_t *root,
                                  u64 split_size, u64 size) {
   wbst_require_balance(wb);
   (void)tree_split_inputs_require(root, split_size, size);
   if (root != NULL) {
      tree_debug_require_size(root, size);
   }
   return wbst_bu_split_impl(wb, root, split_size, size);
}

typedef struct {
   node_t *node;
   u64 size;
   u64 left_size;
   u64 right_size;
   int state;
   bool is_right_child;
   int left_height;
   int right_height;
} wbst_verify_balance_frame_t;

enum {
   WBST_VERIFY_BALANCE_STACK_INLINE_CAP = 128,
};

static void
wbst_verify_balance_stack_push(wbst_verify_balance_frame_t **stack,
                               wbst_verify_balance_frame_t *inline_stack,
                               u64 *len, u64 *cap, node_t *node, u64 size,
                               bool is_right_child) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(len != NULL);
   invariant(cap != NULL);
   invariant(*stack != NULL);
   invariant(*cap > 0);
   invariant(*len <= *cap);
   if (node == NULL) {
      invariant(size == 0);
      return;
   }
   invariant(size > 0);

   if (*len == *cap) {
      *stack = (wbst_verify_balance_frame_t *)stack_array_grow(
          *stack, inline_stack, *len, cap, sizeof(**stack));
   }

   invariant(*len < *cap);
   u64 index = *len;
   *len = size_after_inserting_one(*len);
   (*stack)[index] = (wbst_verify_balance_frame_t){
       .node = node,
       .size = size,
       .left_size = 0,
       .right_size = 0,
       .state = 0,
       .is_right_child = is_right_child,
       .left_height = -1,
       .right_height = -1,
   };
}

static wbst_verify_balance_frame_t *
wbst_verify_balance_stack_top(wbst_verify_balance_frame_t *stack, u64 len) {
   invariant(stack != NULL);
   invariant(len > 0);
   return &stack[rank_of_last(len)];
}

static int wbst_verify_balance_height_from_children(int left_height,
                                                    int right_height) {
   invariant(left_height >= -1);
   invariant(right_height >= -1);
   int child_height = MAX(left_height, right_height);
   invariant(child_height < INT_MAX);
   return child_height < 0 ? 0
                           : (int)size_after_inserting_one((u64)child_height);
}

static void
wbst_verify_balance_set_child_size(wbst_verify_balance_frame_t *parent,
                                   bool is_right_child, u64 size) {
   invariant(parent != NULL);
   invariant(size > 0);
   if (is_right_child) {
      invariant(parent->right_size == 0);
      parent->right_size = size;
   } else {
      invariant(parent->left_size == 0);
      parent->left_size = size;
   }
}

static void
wbst_verify_balance_set_child_height(wbst_verify_balance_frame_t *parent,
                                     bool is_right_child, int height) {
   invariant(parent != NULL);
   invariant(height >= 0);
   if (is_right_child) {
      invariant(parent->right_height == -1);
      parent->right_height = height;
   } else {
      invariant(parent->left_height == -1);
      parent->left_height = height;
   }
}

static void
wbst_verify_balance_stack_dispose(wbst_verify_balance_frame_t *stack,
                                  wbst_verify_balance_frame_t *inline_stack,
                                  u64 len, u64 cap) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(cap > 0);
   invariant(len <= cap);
   invariant(stack == inline_stack ||
             cap > WBST_VERIFY_BALANCE_STACK_INLINE_CAP);
   if (stack != inline_stack) {
      free(stack);
   }
}

static void wbst_verify_balance_finish_frame(wbst_verify_balance_frame_t *stack,
                                             u64 *len, bool is_right_child,
                                             int height, u64 size,
                                             u64 *root_size) {
   invariant(stack != NULL);
   invariant(len != NULL);
   invariant(*len > 0);
   invariant(height >= 0);
   invariant(size > 0);
   invariant(root_size != NULL);

   *len = size_after_removing_one(*len);
   if (*len == 0) {
      *root_size = size;
      return;
   }

   wbst_verify_balance_frame_t *parent =
       wbst_verify_balance_stack_top(stack, *len);
   wbst_verify_balance_set_child_size(parent, is_right_child, size);
   wbst_verify_balance_set_child_height(parent, is_right_child, height);
}

void wbst_verifyBalance(weight_balance_t wb, node_t *p, u64 s, u64 min_size) {
   weight_balance_require(wb);
   tree_sized_root_shape_require(p, s);
   invariant(min_size > 0);

   wbst_verify_balance_frame_t
       inline_stack[WBST_VERIFY_BALANCE_STACK_INLINE_CAP];
   wbst_verify_balance_frame_t *stack = inline_stack;
   u64 len = 0;
   u64 cap = WBST_VERIFY_BALANCE_STACK_INLINE_CAP;
   u64 visited = 0;
   u64 root_size = 0;
   node_seen_t seen = {0};

   wbst_verify_balance_stack_push(&stack, inline_stack, &len, &cap, p, s,
                                  false);
   while (len > 0) {
      wbst_verify_balance_frame_t *frame =
          wbst_verify_balance_stack_top(stack, len);

      if (frame->state == 0) {
         frame->state = 1;
         node_seen_insert(&seen, frame->node);
         invariant(sizeL(frame->node) < frame->size);
         wbst_verify_balance_stack_push(&stack, inline_stack, &len, &cap,
                                        frame->node->l, sizeL(frame->node),
                                        false);
         continue;
      }

      if (frame->state == 1) {
         frame->state = 2;
         wbst_verify_balance_stack_push(&stack, inline_stack, &len, &cap,
                                        frame->node->r,
                                        sizeR(frame->node, frame->size), true);
         continue;
      }

      node_t *node = frame->node;
      u64 size = frame->size;
      u64 sl = sizeL(node);
      u64 sr = sizeR(node, size);

      invariant((node->l == NULL) == (sl == 0));
      invariant((node->r == NULL) == (sr == 0));
      invariant((node->l == NULL) == (frame->left_height == -1));
      invariant((node->r == NULL) == (frame->right_height == -1));
      invariant((node->l == NULL) == (frame->left_size == 0));
      invariant((node->r == NULL) == (frame->right_size == 0));
      invariant(frame->left_size == sl);
      invariant(frame->right_size == sr);
      if (size >= min_size) {
         invariant(wb.isBalanced(sl, sr));
         invariant(wb.isBalanced(sr, sl));
      }

      int h = wbst_verify_balance_height_from_children(frame->left_height,
                                                       frame->right_height);
      invariant(h <= wb.maxHeight(size));
      visited = size_after_inserting_one(visited);

      wbst_verify_balance_finish_frame(stack, &len, frame->is_right_child, h,
                                       size, &root_size);
   }

   wbst_verify_balance_stack_dispose(stack, inline_stack, len, cap);
   invariant(visited == s);
   invariant(root_size == s);
   node_seen_dispose(&seen);
}

void wbst_bu_verifyBalance(weight_balance_t wb, node_t *p, u64 s) {
   wbst_verifyBalance(wb, p, s, 1);
}

static void wbst_bu_insert_op(wbst_bottomup_t *t, u64 i, u64 x) {
   invariant(t != NULL);
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   t->tree.root = wbst_bu_insert(t->wb, t->tree.root, t->tree.size, i, x);
   tree_list_sized_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
}

static u64 wbst_bu_delete_op(wbst_bottomup_t *t, u64 i) {
   invariant(t != NULL);
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   wbst_bu_delete_result_t result =
       wbst_bu_delete(t->wb, t->tree.root, t->tree.size, i);
   t->tree.root = result.root;
   tree_list_sized_deleted(&t->tree);
   invariant(t->tree.size == deleted_size);
   return result.value;
}

static void wbst_bu_verify_tree(wbst_bottomup_t *t) {
   invariant(t != NULL);
   wbst_bu_verifyBalance(t->wb, t->tree.root, t->tree.size);
}

WB_VT_BODY(
    wbst_bottomup_t, wbst_bu, wb_three_two,
    { wbst_bu_insert_op((wbst_bottomup_t *)self->impl, i, x); },
    { return wbst_bu_delete_op((wbst_bottomup_t *)self->impl, i); },
    { WBST_BU_SPLIT(wbst_bottomup_t); }, { WBST_BU_JOIN(wbst_bottomup_t); },
    { wbst_bu_verify_tree((wbst_bottomup_t *)self->impl); })
const list_vtable_t wbst_bottomup_vtable =
    TREE_LIST_VTABLE("WBSTBottomUp", wbst_bu);
