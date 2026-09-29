#include "conc.h"
#include "tree_list_adapter.h"

// ─────────────────────────────────────────────────────────────────────────────
// Conc tree internals
//
// https://api.semanticscholar.org/CorpusID:21267485
// Prokopec, A., & Odersky, M. (2015). Conc-Trees for Functional and Parallel Programming.
//
// Note: Conc trees store data only in leaves. Internal nodes are "link" nodes.
// The `s` field stores the total number of leaves in the subtree.
// The `y` field stores the height.
// ─────────────────────────────────────────────────────────────────────────────

static node_t *conc_asLeaf(u64 x) {
   return tree_allocate((node_t){.x = x, .s = 1});
}

static u64 conc_right_index(u64 i, u64 left_size) {
   invariant(i >= left_size);
   return i - left_size;
}

static node_t *conc_link(node_t *left, node_t *right) {
   invariant(left != NULL);
   invariant(right != NULL);
   u64 size = size_sum(left->s, right->s);
   u64 y = MAX(left->y, right->y);
   invariant(y < UINT64_MAX);
   return tree_allocate((node_t){
       .y = y + 1,
       .s = size,
       .l = left,
       .r = right,
   });
}

typedef struct {
   node_t *left;
   node_t *right;
} conc_child_pair_t;

static conc_child_pair_t conc_unpack(node_t *parent) {
   invariant(parent != NULL);
   invariant(!isLeaf(parent));
   tree_persist_unshared(&parent);
   conc_child_pair_t children = {
       .left = parent->l,
       .right = parent->r,
   };
   invariant(children.left != NULL);
   invariant(children.right != NULL);
   parent->l = NULL;
   parent->r = NULL;
   tree_release(parent);
   return children;
}

static node_t *conc_concat(node_t *left, node_t *right);

static node_t *conc_balanced(node_t *left, node_t *right) {
   if (left == NULL) {
      return right;
   }
   if (right == NULL) {
      return left;
   }
   return conc_concat(left, right);
}

static node_t *conc_concat(node_t *left, node_t *right) {
   invariant(left != NULL);
   invariant(right != NULL);
   if (left->y > right->y && ((left->y - right->y) > 1)) {
      u64 left_height = left->y;
      conc_child_pair_t left_children = conc_unpack(left);
      node_t *left_left = left_children.left;
      node_t *left_right = left_children.right;
      if (left_left->y >= left_right->y) {
         return conc_link(left_left, conc_concat(left_right, right));
      }
      conc_child_pair_t left_right_children = conc_unpack(left_right);
      node_t *left_right_left = left_right_children.left;
      node_t *left_right_right = left_right_children.right;
      node_t *right_spine = conc_concat(left_right_right, right);
      if (left_height - right_spine->y < 3) {
         return conc_link(conc_link(left_left, left_right_left), right_spine);
      } else {
         return conc_link(left_left, conc_link(left_right_left, right_spine));
      }
   }
   if (right->y > left->y && ((right->y - left->y) > 1)) {
      u64 right_height = right->y;
      conc_child_pair_t right_children = conc_unpack(right);
      node_t *right_left = right_children.left;
      node_t *right_right = right_children.right;
      if (right_right->y >= right_left->y) {
         return conc_link(conc_concat(left, right_left), right_right);
      }
      conc_child_pair_t right_left_children = conc_unpack(right_left);
      node_t *right_left_left = right_left_children.left;
      node_t *right_left_right = right_left_children.right;
      node_t *left_spine = conc_concat(left, right_left_left);
      if (right_height - left_spine->y < 3) {
         return conc_link(left_spine, conc_link(right_left_right, right_right));
      } else {
         return conc_link(conc_link(left_spine, right_left_right), right_right);
      }
   }
   return conc_link(left, right);
}

static node_t *conc_lookup(node_t *p, u64 i) {
   invariant(p != NULL);
   for (;;) {
      invariant(p != NULL);
      invariant(i < p->s);
      if (isLeaf(p)) {
         invariant(i == 0);
         return p;
      }
      invariant(p->l != NULL);
      invariant(p->r != NULL);
      if (i < p->l->s) {
         p = p->l;
      } else {
         i = conc_right_index(i, p->l->s);
         p = p->r;
      }
   }
}

static node_t *conc_update(node_t *root, u64 i, u64 x) {
   invariant(root != NULL);
   invariant(i < root->s);
   if (isLeaf(root)) {
      invariant(i == 0);
      tree_release(root);
      return conc_asLeaf(x);
   }
   invariant(root->l != NULL);
   invariant(root->r != NULL);
   u64 left_size = root->l->s;
   conc_child_pair_t children = conc_unpack(root);
   node_t *left = children.left;
   node_t *right = children.right;
   if (i < left_size) {
      return conc_link(conc_update(left, i, x), right);
   } else {
      u64 right_index = conc_right_index(i, left_size);
      return conc_link(left, conc_update(right, right_index, x));
   }
}

static node_t *conc_insert(node_t *root, u64 i, u64 x) {
   invariant(root != NULL);
   invariant(i <= root->s);
   if (isLeaf(root)) {
      invariant(root->s == 1);
      invariant(i <= 1);
      if (i == 0) {
         return conc_link(conc_asLeaf(x), root);
      } else {
         return conc_link(root, conc_asLeaf(x));
      }
   }
   invariant(root->l != NULL);
   invariant(root->r != NULL);
   u64 left_size = root->l->s;
   conc_child_pair_t children = conc_unpack(root);
   node_t *left = children.left;
   node_t *right = children.right;
   if (i < left_size) {
      return conc_balanced(conc_insert(left, i, x), right);
   } else {
      u64 right_index = conc_right_index(i, left_size);
      return conc_balanced(left, conc_insert(right, right_index, x));
   }
}

typedef struct {
   node_t *root;
   u64 value;
} conc_delete_result_t;

static conc_delete_result_t conc_delete(node_t *root, u64 i) {
   invariant(root != NULL);
   invariant(i < root->s);
   if (isLeaf(root)) {
      invariant(i == 0);
      u64 value = root->x;
      tree_release(root);
      return (conc_delete_result_t){.root = NULL, .value = value};
   }
   invariant(root->l != NULL);
   invariant(root->r != NULL);
   u64 left_size = root->l->s;
   conc_child_pair_t children = conc_unpack(root);
   node_t *left = children.left;
   node_t *right = children.right;
   if (i < left_size) {
      conc_delete_result_t result = conc_delete(left, i);
      result.root = conc_balanced(result.root, right);
      return result;
   }
   u64 right_index = conc_right_index(i, left_size);
   conc_delete_result_t result = conc_delete(right, right_index);
   result.root = conc_balanced(left, result.root);
   return result;
}

typedef struct {
   node_t *left;
   node_t *right;
} conc_split_result_t;

static conc_split_result_t conc_split(node_t *root, u64 split_size) {
   invariant(root != NULL);
   invariant(split_size <= root->s);
   if (isLeaf(root)) {
      invariant(root->s == 1);
      invariant(split_size <= 1);
      if (split_size == 0) {
         return (conc_split_result_t){.left = NULL, .right = root};
      } else {
         return (conc_split_result_t){.left = root, .right = NULL};
      }
   }
   invariant(root->l != NULL);
   invariant(root->r != NULL);
   u64 root_left_size = root->l->s;
   conc_child_pair_t root_children = conc_unpack(root);
   node_t *root_left = root_children.left;
   node_t *root_right = root_children.right;
   if (split_size < root_left_size) {
      conc_split_result_t split = conc_split(root_left, split_size);
      return (conc_split_result_t){
          .left = split.left,
          .right = conc_balanced(split.right, root_right),
      };
   } else {
      u64 right_split_size = conc_right_index(split_size, root_left_size);
      conc_split_result_t split = conc_split(root_right, right_split_size);
      return (conc_split_result_t){
          .left = conc_balanced(root_left, split.left),
          .right = split.right,
      };
   }
}

// Conc inorder: data is only in leaves.
static void conc_inorder(node_t *root, void (*visit)(u64, void *), void *ctx) {
   invariant(visit != NULL);

   node_t *inline_data[NODE_STACK_INLINE_CAP];
   node_stack_t stack = {
       .data = inline_data,
       .len = 0,
       .cap = NODE_STACK_INLINE_CAP,
   };

   node_stack_push(&stack, inline_data, root);
   while (stack.len > 0) {
      node_t *node = node_stack_pop(&stack);
      if (isLeaf(node)) {
         visit(node->x, ctx);
      } else {
         node_stack_push(&stack, inline_data, node->r);
         node_stack_push(&stack, inline_data, node->l);
      }
   }

   node_stack_free(&stack, inline_data);
}

typedef struct {
   u64 leaves;
   u64 height;
} conc_measure_t;

typedef struct {
   node_t *node;
   int state;
   bool is_right_child;
   conc_measure_t left;
   conc_measure_t right;
} conc_verify_frame_t;

enum {
   CONC_VERIFY_STACK_INLINE_CAP = 128,
};

static void conc_verify_stack_push(conc_verify_frame_t **stack,
                                   conc_verify_frame_t *inline_stack, u64 *len,
                                   u64 *cap, node_t *node,
                                   bool is_right_child) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(len != NULL);
   invariant(cap != NULL);
   invariant(*stack != NULL);
   invariant(*cap > 0);
   invariant(*len <= *cap);
   invariant(node != NULL);

   if (*len == *cap) {
      *stack = (conc_verify_frame_t *)stack_array_grow(
          *stack, inline_stack, *len, cap, sizeof(**stack));
   }

   invariant(*len < *cap);
   u64 index = *len;
   *len = size_after_inserting_one(*len);
   (*stack)[index] = (conc_verify_frame_t){
       .node = node,
       .state = 0,
       .is_right_child = is_right_child,
   };
}

static void conc_verify_stack_dispose(conc_verify_frame_t *stack,
                                      conc_verify_frame_t *inline_stack,
                                      u64 len, u64 cap) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(cap > 0);
   invariant(len <= cap);
   invariant(stack == inline_stack || cap > CONC_VERIFY_STACK_INLINE_CAP);
   if (stack != inline_stack) {
      free(stack);
   }
}

static conc_measure_t conc_verify_measure_node(node_t *p, conc_measure_t l,
                                               conc_measure_t r) {
   invariant(p != NULL);
   invariant(p->l != NULL);
   invariant(p->r != NULL);
   u64 leaves = size_sum(l.leaves, r.leaves);

   u64 height = MAX(l.height, r.height);
   height = size_after_inserting_one(height);

   u64 diff = l.height > r.height ? l.height - r.height : r.height - l.height;
   invariant(diff <= 1);
   invariant(p->s == leaves);
   invariant(p->y == height);

   return (conc_measure_t){.leaves = leaves, .height = height};
}

static void conc_verify_set_child_measure(conc_verify_frame_t *parent,
                                          bool is_right_child,
                                          conc_measure_t measure) {
   invariant(parent != NULL);
   invariant(measure.leaves > 0);
   if (is_right_child) {
      invariant(parent->right.leaves == 0);
      parent->right = measure;
   } else {
      invariant(parent->left.leaves == 0);
      parent->left = measure;
   }
}

static conc_measure_t conc_verify_node(node_t *p) {
   invariant(p != NULL);

   conc_verify_frame_t inline_stack[CONC_VERIFY_STACK_INLINE_CAP];
   conc_verify_frame_t *stack = inline_stack;
   u64 len = 0;
   u64 cap = CONC_VERIFY_STACK_INLINE_CAP;
   conc_measure_t root = {0};
   node_seen_t seen = {0};

   conc_verify_stack_push(&stack, inline_stack, &len, &cap, p, false);
   while (len > 0) {
      conc_verify_frame_t *frame = &stack[rank_of_last(len)];
      node_t *node = frame->node;

      if (frame->state == 0) {
         node_seen_insert(&seen, node);
         invariant((node->l == NULL) == (node->r == NULL));
         if (isLeaf(node)) {
            invariant(node->s == 1);
            invariant(node->y == 0);
            conc_measure_t measure = {.leaves = 1, .height = 0};
            bool is_right_child = frame->is_right_child;
            len = size_after_removing_one(len);
            if (len == 0) {
               root = measure;
            } else {
               conc_verify_set_child_measure(&stack[rank_of_last(len)],
                                             is_right_child, measure);
            }
            continue;
         }

         frame->state = 1;
         conc_verify_stack_push(&stack, inline_stack, &len, &cap, node->l,
                                false);
         continue;
      }

      if (frame->state == 1) {
         frame->state = 2;
         conc_verify_stack_push(&stack, inline_stack, &len, &cap, node->r,
                                true);
         continue;
      }

      invariant(frame->left.leaves > 0);
      invariant(frame->right.leaves > 0);
      conc_measure_t measure =
          conc_verify_measure_node(node, frame->left, frame->right);
      bool is_right_child = frame->is_right_child;
      len = size_after_removing_one(len);
      if (len == 0) {
         root = measure;
      } else {
         conc_verify_set_child_measure(&stack[rank_of_last(len)],
                                       is_right_child, measure);
      }
   }

   conc_verify_stack_dispose(stack, inline_stack, len, cap);
   node_seen_dispose(&seen);
   return root;
}

static tree_t *conc_require_handle(tree_t *tree) {
   invariant(tree != NULL);
   tree_handle_require(*tree);
   if (tree->root != NULL) {
      invariant((tree->root->l == NULL) == (tree->root->r == NULL));
      invariant(tree->root->s == tree->size);
   }
   return tree;
}

static void conc_verify_tree(tree_t *tree) {
   conc_require_handle(tree);
   if (tree->root == NULL) {
      return;
   }
   conc_measure_t measure = conc_verify_node(tree->root);
   invariant(measure.leaves == tree->size);
}

#ifdef DEBUG
static void conc_debug_require_join_inputs(node_t *left_root,
                                           node_t *right_root, u64 left_size,
                                           u64 right_size) {
   tree_t left = tree_list_make_tree(left_root, left_size);
   tree_t right = tree_list_make_tree(right_root, right_size);
   conc_verify_tree(&left);
   conc_verify_tree(&right);
   assert_bst(!tree_roots_overlap(&left, &right));
}
#endif

static void conc_require_join_roots(node_t *left_root, node_t *right_root,
                                    u64 left_size, u64 right_size) {
   tree_root_size_require(left_root, left_size);
   tree_root_size_require(right_root, right_size);
   invariant(left_size <= UINT64_MAX - right_size);
#ifdef DEBUG
   conc_debug_require_join_inputs(left_root, right_root, left_size, right_size);
#endif
}

typedef struct {
   node_t *src;
   node_t **out;
   node_t *clone;
   int state;
   bool is_right_child;
   conc_measure_t left;
   conc_measure_t right;
} conc_clone_frame_t;

enum {
   CONC_CLONE_STACK_INLINE_CAP = 128,
};

static void conc_clone_stack_push(conc_clone_frame_t **stack,
                                  conc_clone_frame_t *inline_stack, u64 *len,
                                  u64 *cap, node_t *src, bool is_right_child,
                                  node_t **out) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(len != NULL);
   invariant(cap != NULL);
   invariant(out != NULL);
   invariant(*stack != NULL);
   invariant(*cap > 0);
   invariant(*len <= *cap);
   if (src == NULL) {
      *out = NULL;
      return;
   }

   if (*len == *cap) {
      *stack = (conc_clone_frame_t *)stack_array_grow(
          *stack, inline_stack, *len, cap, sizeof(**stack));
   }

   invariant(*len < *cap);
   u64 index = *len;
   *len = size_after_inserting_one(*len);
   (*stack)[index] = (conc_clone_frame_t){
       .src = src,
       .out = out,
       .clone = NULL,
       .state = 0,
       .is_right_child = is_right_child,
       .left = {0},
       .right = {0},
   };
}

static void conc_clone_stack_dispose(conc_clone_frame_t *stack,
                                     conc_clone_frame_t *inline_stack, u64 len,
                                     u64 cap) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(cap > 0);
   invariant(len <= cap);
   invariant(stack == inline_stack || cap > CONC_CLONE_STACK_INLINE_CAP);
   if (stack != inline_stack) {
      free(stack);
   }
}

static void conc_clone_finish_frame(conc_clone_frame_t *stack, u64 *len,
                                    conc_measure_t measure, bool is_right_child,
                                    conc_measure_t *root) {
   invariant(stack != NULL);
   invariant(len != NULL);
   invariant(*len > 0);
   invariant(measure.leaves > 0);
   invariant(root != NULL);

   *len = size_after_removing_one(*len);
   if (*len == 0) {
      *root = measure;
   } else if (is_right_child) {
      conc_clone_frame_t *parent = &stack[rank_of_last(*len)];
      invariant(parent->right.leaves == 0);
      parent->right = measure;
   } else {
      conc_clone_frame_t *parent = &stack[rank_of_last(*len)];
      invariant(parent->left.leaves == 0);
      parent->left = measure;
   }
}

static node_t conc_clone_node_value(node_t *src) {
   invariant(src != NULL);
   node_t value = *src;
   value.l = NULL;
   value.r = NULL;
#ifdef PERSISTENT
   value.refs = 0;
#endif
   return value;
}

static node_t *conc_clone_nodes(node_t *root, u64 size) {
   tree_root_size_require(root, size);
   node_t *copy_root = NULL;
   conc_clone_frame_t inline_stack[CONC_CLONE_STACK_INLINE_CAP];
   conc_clone_frame_t *stack = inline_stack;
   u64 len = 0;
   u64 cap = CONC_CLONE_STACK_INLINE_CAP;
   conc_measure_t measure = {0};
   node_seen_t seen = {0};

   conc_clone_stack_push(&stack, inline_stack, &len, &cap, root, false,
                         &copy_root);
   while (len > 0) {
      conc_clone_frame_t *frame = &stack[rank_of_last(len)];
      node_t *node = frame->src;

      if (frame->state == 0) {
         frame->state = 1;
         node_seen_insert(&seen, node);
         invariant((node->l == NULL) == (node->r == NULL));
         frame->clone = tree_allocate(conc_clone_node_value(node));
         *frame->out = frame->clone;

         if (isLeaf(node)) {
            invariant(node->s == 1);
            invariant(node->y == 0);
            conc_clone_finish_frame(stack, &len,
                                    (conc_measure_t){.leaves = 1, .height = 0},
                                    frame->is_right_child, &measure);
            continue;
         }

         conc_clone_stack_push(&stack, inline_stack, &len, &cap, node->l, false,
                               &frame->clone->l);
         continue;
      }

      if (frame->state == 1) {
         frame->state = 2;
         conc_clone_stack_push(&stack, inline_stack, &len, &cap, node->r, true,
                               &frame->clone->r);
         continue;
      }

      invariant(frame->clone != NULL);
      invariant(frame->left.leaves > 0);
      invariant(frame->right.leaves > 0);
      conc_measure_t subtree =
          conc_verify_measure_node(node, frame->left, frame->right);
      conc_clone_finish_frame(stack, &len, subtree, frame->is_right_child,
                              &measure);
   }

   conc_clone_stack_dispose(stack, inline_stack, len, cap);
   invariant(measure.leaves == size);
   node_seen_dispose(&seen);
   return copy_root;
}

static conc_t *conc_clone_physical(conc_t *orig) {
   invariant(orig != NULL);
   conc_require_handle(&orig->tree);
   conc_t *copy = (conc_t *)bst_malloc(sizeof(*copy));
   *copy = *orig;
   copy->tree.root = conc_clone_nodes(orig->tree.root, orig->tree.size);
   return copy;
}

// ─────────────────────────────────────────────────────────────────────────────
// List vtable
// ─────────────────────────────────────────────────────────────────────────────

static list_t conc_vt_new(const list_vtable_t *vt) {
   list_vtable_require(vt);
   conc_t *t = (conc_t *)bst_calloc(1, sizeof(conc_t));
   return tree_list_make(t, vt);
}

static list_t conc_vt_clone(list_t *self) {
   list_require(self);
   conc_t *orig = (conc_t *)self->impl;
   conc_t *copy;
#ifdef PERSISTENT
   conc_verify_tree(&orig->tree);
   copy = (conc_t *)bst_malloc(sizeof(*copy));
   *copy = *orig;
   tree_share(copy->tree.root);
#else
   copy = conc_clone_physical(orig);
#endif
   return tree_list_make(copy, self->vt);
}

static void conc_vt_free(list_t *self) {
   list_require(self);
   conc_t *t = (conc_t *)self->impl;
   conc_require_handle(&t->tree);
   tree_list_free_impl(self, t, sizeof(*t), &t->tree);
}

static u64 conc_vt_size(list_t *self) {
   list_require(self);
   conc_t *t = (conc_t *)self->impl;
   return conc_require_handle(&t->tree)->size;
}

static void conc_vt_each(list_t *self, void (*visit)(u64, void *), void *ctx) {
   list_require(self);
   conc_t *t = (conc_t *)self->impl;
   conc_inorder(conc_require_handle(&t->tree)->root, visit, ctx);
}

static void conc_vt_verify(list_t *self) {
   list_require(self);
   conc_t *t = (conc_t *)self->impl;
   conc_verify_tree(&t->tree);
}

static list_t conc_make_result(const list_vtable_t *vt, node_t *root,
                               u64 size) {
   conc_t *res = TREE_LIST_ALLOC_UNCHECKED(conc_t, root, size);
   conc_require_handle(&res->tree);
#ifdef DEBUG
   conc_verify_tree(&res->tree);
#endif
   return tree_list_make(res, vt);
}

static u64 conc_vt_select(list_t *self, u64 i) {
   list_require(self);
   conc_t *t = (conc_t *)self->impl;
   tree_t *tree = conc_require_handle(&t->tree);
   sequence_rank_inputs_require(i, tree->size);
   return conc_lookup(tree->root, i)->x;
}

static void conc_vt_update(list_t *self, u64 i, u64 x) {
   list_require(self);
   conc_t *t = (conc_t *)self->impl;
   tree_t *tree = conc_require_handle(&t->tree);
   sequence_rank_inputs_require(i, tree->size);
   u64 size = tree->size;
   t->tree.root = conc_update(t->tree.root, i, x);
   invariant(t->tree.size == size);
   conc_require_handle(&t->tree);
}

static void conc_vt_insert(list_t *self, u64 i, u64 x) {
   list_require(self);
   conc_t *t = (conc_t *)self->impl;
   tree_t *tree = conc_require_handle(&t->tree);
   u64 size = tree->size;
   u64 inserted_size = sequence_insert_inputs_require(i, size);
   if (tree->root == NULL) {
      t->tree.root = conc_asLeaf(x);
   } else {
      t->tree.root = conc_insert(tree->root, i, x);
   }
   tree_list_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
   conc_require_handle(&t->tree);
}

static u64 conc_vt_delete(list_t *self, u64 i) {
   list_require(self);
   conc_t *t = (conc_t *)self->impl;
   tree_t *tree = conc_require_handle(&t->tree);
   u64 size = tree->size;
   u64 deleted_size = sequence_delete_inputs_require(i, size);
   conc_delete_result_t result = conc_delete(tree->root, i);
   t->tree.root = result.root;
   tree_list_deleted(&t->tree);
   invariant(t->tree.size == deleted_size);
   conc_require_handle(&t->tree);
   return result.value;
}

TREE_LIST_DEFINE_EVENT_APPLIERS(conc)

static void conc_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   tree_list_require_split(self, i, out_l, out_r);
   conc_t *t = (conc_t *)self->impl;
   tree_t *tree = conc_require_handle(&t->tree);
   u64 right_size = sequence_split_inputs_require(i, tree->size);
   tree_list_require_split_result(self, i, i, right_size, out_l, out_r);
   conc_split_result_t split = {0};
   if (tree->root != NULL) {
      tree_share(tree->root);
      split = conc_split(tree->root, i);
   }
   *out_l = conc_make_result(self->vt, split.left, i);
   *out_r = conc_make_result(self->vt, split.right, right_size);
#ifndef PERSISTENT
   t->tree = (tree_t){0};
#endif
}

#ifdef PERSISTENT
static tree_join_roots_t conc_prepare_join_roots(conc_t *left, conc_t *right) {
   invariant(left != NULL);
   invariant(right != NULL);
   tree_handle_require(left->tree);
   tree_handle_require(right->tree);

   if (tree_roots_overlap(&left->tree, &right->tree)) {
      if (left->tree.size < right->tree.size) {
         conc_t *left_copy = conc_clone_physical(left);
         tree_share(right->tree.root);
         tree_join_roots_t roots = {
             .left = left_copy->tree.root,
             .right = right->tree.root,
         };
         free(left_copy);
         return roots;
      }

      conc_t *right_copy = conc_clone_physical(right);
      tree_share(left->tree.root);
      tree_join_roots_t roots = {
          .left = left->tree.root,
          .right = right_copy->tree.root,
      };
      free(right_copy);
      return roots;
   }

   tree_share(left->tree.root);
   tree_share(right->tree.root);
   return (tree_join_roots_t){
       .left = left->tree.root,
       .right = right->tree.root,
   };
}
#endif

static list_t conc_vt_join(list_t *self, list_t *other) {
   tree_list_require_join(self, other);
   conc_t *t = (conc_t *)self->impl;
   conc_t *that = (conc_t *)other->impl;
   tree_t *left = conc_require_handle(&t->tree);
   tree_t *right = conc_require_handle(&that->tree);
   u64 size = tree_list_join_size(left->size, right->size);
   tree_list_require_join_result(self, other, size);
#ifdef PERSISTENT
   tree_join_roots_t roots = conc_prepare_join_roots(t, that);
   node_t *left_root = roots.left;
   node_t *right_root = roots.right;
#else
   node_t *left_root = left->root;
   node_t *right_root = right->root;
   tree_share(left_root);
   tree_share(right_root);
#endif
   conc_require_join_roots(left_root, right_root, left->size, right->size);
   node_t *root = conc_balanced(left_root, right_root);
   list_t joined = conc_make_result(self->vt, root, size);
#ifndef PERSISTENT
   t->tree = (tree_t){0};
   that->tree = (tree_t){0};
#endif
   return joined;
}

TREE_LIST_DEFINE_TREE_ACCESSOR(conc_t, conc)
const list_vtable_t conc_vtable = TREE_LIST_VTABLE("Conc", conc);
