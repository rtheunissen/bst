#include "bst.h"

#include <limits.h>

#ifdef DEBUG
static u64 *tree_debug_capture_sequence(tree_t tree);
static void tree_debug_require_values_slice(const u64 *actual, u64 actual_count,
                                            u64 actual_offset,
                                            const u64 *expected,
                                            u64 expected_count,
                                            u64 expected_offset, u64 count);
#endif

static void tree_node_local_shape_require(node_t *node, u64 size);

// ─────────────────────────────────────────────────────────────────────────────
// Clone
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   node_t *src;
   node_t **out;
   node_t *clone;
   u64 size;
   u64 left_size;
   u64 right_size;
   int state;
   bool is_right_child;
} tree_clone_frame_t;

enum {
   TREE_CLONE_STACK_INLINE_CAP = 128,
};

static void tree_clone_stack_push(tree_clone_frame_t **stack,
                                  tree_clone_frame_t *inline_stack, u64 *len,
                                  u64 *cap, node_t *src, u64 size,
                                  bool is_right_child, node_t **out) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(len != NULL);
   invariant(cap != NULL);
   invariant(out != NULL);
   invariant(*stack != NULL);
   invariant(*cap > 0);
   invariant(*len <= *cap);
   if (src == NULL) {
      invariant(size == 0);
      *out = NULL;
      return;
   }
   invariant(size > 0);

   if (*len == *cap) {
      *stack = (tree_clone_frame_t *)stack_array_grow(
          *stack, inline_stack, *len, cap, sizeof(**stack));
   }

   invariant(*len < *cap);
   u64 index = *len;
   *len = size_after_inserting_one(*len);
   (*stack)[index] = (tree_clone_frame_t){
       .src = src,
       .out = out,
       .clone = NULL,
       .size = size,
       .left_size = 0,
       .right_size = 0,
       .state = 0,
       .is_right_child = is_right_child,
   };
}

static node_t tree_clone_node_value(node_t *src) {
   invariant(src != NULL);
   node_t value = *src;
   value.l = NULL;
   value.r = NULL;
#ifdef PERSISTENT
   value.refs = 0;
#endif
   return value;
}

static void tree_clone_set_child_size(tree_clone_frame_t *parent,
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

static void tree_clone_finish_frame(tree_clone_frame_t *stack, u64 *len,
                                    bool is_right_child, u64 size) {
   invariant(stack != NULL);
   invariant(len != NULL);
   invariant(*len > 0);
   invariant(size > 0);

   *len = size_after_removing_one(*len);
   if (*len > 0) {
      tree_clone_frame_t *parent = &stack[rank_of_last(*len)];
      tree_clone_set_child_size(parent, is_right_child, size);
   }
}

static void tree_clone_stack_dispose(tree_clone_frame_t *stack,
                                     tree_clone_frame_t *inline_stack, u64 len,
                                     u64 cap) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(cap > 0);
   invariant(len <= cap);
   invariant(stack == inline_stack || cap > TREE_CLONE_STACK_INLINE_CAP);
   if (stack != inline_stack) {
      free(stack);
   }
}

static node_t *tree_clone_nodes(node_t *p, u64 size) {
   tree_sized_root_require(p, size);
   node_t *root = NULL;
   tree_clone_frame_t inline_stack[TREE_CLONE_STACK_INLINE_CAP];
   tree_clone_frame_t *stack = inline_stack;
   u64 len = 0;
   u64 cap = TREE_CLONE_STACK_INLINE_CAP;
   u64 cloned = 0;
   node_seen_t seen = {0};

   tree_clone_stack_push(&stack, inline_stack, &len, &cap, p, size, false,
                         &root);
   while (len > 0) {
      tree_clone_frame_t *frame = &stack[rank_of_last(len)];

      if (frame->state == 0) {
         frame->state = 1;
         node_seen_insert(&seen, frame->src);
         tree_node_local_shape_require(frame->src, frame->size);
         frame->clone = tree_allocate(tree_clone_node_value(frame->src));
         *frame->out = frame->clone;
         cloned = size_after_inserting_one(cloned);
         tree_clone_stack_push(&stack, inline_stack, &len, &cap, frame->src->l,
                               sizeL(frame->src), false, &frame->clone->l);
         continue;
      }

      if (frame->state == 1) {
         frame->state = 2;
         tree_clone_stack_push(&stack, inline_stack, &len, &cap, frame->src->r,
                               sizeR(frame->src, frame->size), true,
                               &frame->clone->r);
         continue;
      }

      invariant(frame->clone != NULL);
      invariant((frame->src->l == NULL) == (frame->left_size == 0));
      invariant((frame->src->r == NULL) == (frame->right_size == 0));
      invariant(frame->left_size == sizeL(frame->src));
      invariant(frame->right_size == sizeR(frame->src, frame->size));
      invariant(frame->size == tree_size_from_child_sizes(frame->left_size,
                                                          frame->right_size));

      bool is_right_child = frame->is_right_child;
      u64 frame_size = frame->size;
      tree_clone_finish_frame(stack, &len, is_right_child, frame_size);
   }

   tree_clone_stack_dispose(stack, inline_stack, len, cap);
   invariant(cloned == size);
   node_seen_dispose(&seen);
   return root;
}

tree_t tree_clone_physical(tree_t *tree) {
   invariant(tree != NULL);
   tree_sized_handle_require(*tree);
#ifdef DEBUG
   u64 *original = tree_debug_capture_sequence(*tree);
#endif
   tree_t clone = {
       .root = tree_clone_nodes(tree->root, tree->size),
       .size = tree->size,
   };
   tree_sized_handle_require(clone);
#ifdef DEBUG
   tree_debug_require_size(clone.root, clone.size);
   u64 *copy = tree_debug_capture_sequence(clone);
   tree_debug_require_values_slice(copy, clone.size, 0, original, clone.size, 0,
                                   clone.size);
   assert_bst(!tree_contains_shared_node(clone.root));
   free(copy);
   free(original);
#endif
   return clone;
}

tree_t tree_clone(tree_t *tree) {
   invariant(tree != NULL);
   tree_sized_handle_require(*tree);
#ifndef PERSISTENT
   return tree_clone_physical(tree);
#else
#ifdef DEBUG
   tree_debug_require_size(tree->root, tree->size);
#endif
   tree_share(tree->root);
   tree_t clone = *tree;
   tree_sized_handle_require(clone);
   return clone;
#endif
}

bool tree_contains_shared_node(node_t *root) {
#ifndef PERSISTENT
   (void)root;
   return false;
#else
   if (root == NULL) {
      return false;
   }

   node_t *inline_data[NODE_STACK_INLINE_CAP];
   node_stack_t stack = {
       .data = inline_data,
       .len = 0,
       .cap = NODE_STACK_INLINE_CAP,
   };
   node_seen_t seen = {0};
   bool contains_shared = false;

   node_stack_push(&stack, inline_data, root);
   while (stack.len > 0) {
      node_t *node = node_stack_pop(&stack);
      node_seen_insert(&seen, node);
      if (tree_shared(node)) {
         contains_shared = true;
      }
      node_stack_push(&stack, inline_data, node->r);
      node_stack_push(&stack, inline_data, node->l);
   }

   node_stack_free(&stack, inline_data);
   node_seen_dispose(&seen);
   return contains_shared;
#endif
}

#if defined(PERSISTENT) || defined(DEBUG)
static void tree_collect_reachable_nodes(node_seen_t *seen, node_t *root) {
   invariant(seen != NULL);

   node_t *inline_data[NODE_STACK_INLINE_CAP];
   node_stack_t stack = {
       .data = inline_data,
       .len = 0,
       .cap = NODE_STACK_INLINE_CAP,
   };

   node_stack_push(&stack, inline_data, root);
   while (stack.len > 0) {
      node_t *node = node_stack_pop(&stack);
      node_seen_insert(seen, node);
      node_stack_push(&stack, inline_data, node->r);
      node_stack_push(&stack, inline_data, node->l);
   }

   node_stack_free(&stack, inline_data);
}

static bool tree_reachable_nodes_overlap(node_seen_t *indexed, node_t *root) {
   invariant(indexed != NULL);

   node_t *inline_data[NODE_STACK_INLINE_CAP];
   node_stack_t stack = {
       .data = inline_data,
       .len = 0,
       .cap = NODE_STACK_INLINE_CAP,
   };
   node_seen_t scanned = {0};

   node_stack_push(&stack, inline_data, root);
   while (stack.len > 0) {
      node_t *node = node_stack_pop(&stack);
      node_seen_insert(&scanned, node);
      if (node_seen_contains(indexed, node)) {
         node_stack_free(&stack, inline_data);
         node_seen_dispose(&scanned);
         return true;
      }
      node_stack_push(&stack, inline_data, node->r);
      node_stack_push(&stack, inline_data, node->l);
   }

   node_stack_free(&stack, inline_data);
   node_seen_dispose(&scanned);
   return false;
}

bool tree_roots_overlap(tree_t *left, tree_t *right) {
   invariant(left != NULL);
   invariant(right != NULL);
   tree_handle_require(*left);
   tree_handle_require(*right);

   if (left->root == NULL || right->root == NULL) {
      return false;
   }
   if (left->root == right->root) {
      return true;
   }

   node_t *indexed = left->root;
   node_t *scanned = right->root;
   if (right->size < left->size) {
      indexed = right->root;
      scanned = left->root;
   }

   node_seen_t seen = {0};
   tree_collect_reachable_nodes(&seen, indexed);
   bool overlap = tree_reachable_nodes_overlap(&seen, scanned);
   node_seen_dispose(&seen);
   return overlap;
}

#ifdef DEBUG
void tree_debug_require_join_inputs(node_t *left_root, node_t *right_root,
                                    u64 left_size, u64 right_size) {
   tree_debug_require_size(left_root, left_size);
   tree_debug_require_size(right_root, right_size);
   tree_t left = {.root = left_root, .size = left_size};
   tree_t right = {.root = right_root, .size = right_size};
   assert_bst(!tree_roots_overlap(&left, &right));
}
#endif
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Inorder traversal
// ─────────────────────────────────────────────────────────────────────────────

void node_stack_grow(node_stack_t *stack, node_t **inline_data) {
   invariant(stack != NULL);
   invariant(inline_data != NULL);
   invariant(stack->len == stack->cap);
   stack->data = (node_t **)stack_array_grow(
       stack->data, inline_data, stack->len, &stack->cap, sizeof(*stack->data));
}

void inorder(node_t *p, void (*visit)(u64, void *), void *ctx) {
   invariant(visit != NULL);

   node_t *inline_data[NODE_STACK_INLINE_CAP];
   node_stack_t stack = {
       .data = inline_data,
       .len = 0,
       .cap = NODE_STACK_INLINE_CAP,
   };
#ifdef DEBUG
   node_seen_t seen = {0};
#endif

   while (p != NULL || stack.len > 0) {
      while (p != NULL) {
#ifdef DEBUG
         node_seen_insert(&seen, p);
#endif
         node_stack_push(&stack, inline_data, p);
         p = p->l;
      }

      p = node_stack_pop(&stack);
      visit(p->x, ctx);
      p = p->r;
   }

   node_stack_free(&stack, inline_data);
#ifdef DEBUG
   node_seen_dispose(&seen);
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Path metrics, height, and size (O(n), for verification and measurements)
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   node_t *node;
   u64 depth;
} node_path_frame_t;

enum {
   NODE_PATH_STACK_INLINE_CAP = 128,
};

static void node_path_stack_push(node_path_frame_t **stack,
                                 node_path_frame_t *inline_stack, u64 *len,
                                 u64 *cap, node_t *node, u64 depth) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(len != NULL);
   invariant(cap != NULL);
   invariant(*stack != NULL);
   invariant(*cap > 0);
   invariant(*len <= *cap);
   if (node == NULL) {
      return;
   }

   if (*len == *cap) {
      *stack = (node_path_frame_t *)stack_array_grow(*stack, inline_stack, *len,
                                                     cap, sizeof(**stack));
   }

   invariant(*len < *cap);
   u64 index = *len;
   *len = size_after_inserting_one(*len);
   (*stack)[index] = (node_path_frame_t){.node = node, .depth = depth};
}

static node_path_frame_t node_path_stack_pop(node_path_frame_t *stack,
                                             u64 *len) {
   invariant(stack != NULL);
   invariant(len != NULL);
   invariant(*len > 0);
   *len = size_after_removing_one(*len);
   return stack[*len];
}

static void node_path_stack_dispose(node_path_frame_t *stack,
                                    node_path_frame_t *inline_stack, u64 len,
                                    u64 cap) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(cap > 0);
   invariant(len <= cap);
   invariant(stack == inline_stack || cap > NODE_PATH_STACK_INLINE_CAP);
   if (stack != inline_stack) {
      free(stack);
   }
}

static void node_path_metrics_include(node_path_metrics_t *metrics, u64 depth) {
   invariant(metrics != NULL);
   metrics->nodes = size_after_inserting_one(metrics->nodes);
   metrics->total_depth = size_sum(metrics->total_depth, depth);
   metrics->maximum_depth = MAX(metrics->maximum_depth, depth);
}

static void node_path_stack_push_children(node_path_frame_t **stack,
                                          node_path_frame_t *inline_stack,
                                          u64 *len, u64 *cap, node_t *node,
                                          u64 depth) {
   invariant(node != NULL);
   if (node->l == NULL && node->r == NULL) {
      return;
   }

   u64 child_depth = size_after_inserting_one(depth);
   node_path_stack_push(stack, inline_stack, len, cap, node->r, child_depth);
   node_path_stack_push(stack, inline_stack, len, cap, node->l, child_depth);
}

node_path_metrics_t nodePathMetrics(node_t *p) {
   node_path_metrics_t metrics = {0};
   if (p == NULL) {
      nodePathMetricsRequire(metrics);
      return metrics;
   }

   node_path_frame_t inline_stack[NODE_PATH_STACK_INLINE_CAP];
   node_path_frame_t *stack = inline_stack;
   u64 len = 0;
   u64 cap = NODE_PATH_STACK_INLINE_CAP;
   node_seen_t seen = {0};

   node_path_stack_push(&stack, inline_stack, &len, &cap, p, 0);
   while (len > 0) {
      node_path_frame_t frame = node_path_stack_pop(stack, &len);
      node_t *node = frame.node;
      u64 depth = frame.depth;

      node_seen_insert(&seen, node);
      node_path_metrics_include(&metrics, depth);

      node_path_stack_push_children(&stack, inline_stack, &len, &cap, node,
                                    depth);
   }

   node_path_stack_dispose(stack, inline_stack, len, cap);
   node_seen_dispose(&seen);
   nodePathMetricsRequire(metrics);
   return metrics;
}

bool nodeHeightExceeds(node_t *p, int maximum_height) {
   invariant(maximum_height >= -1);
   if (p == NULL) {
      return false;
   }
   if (maximum_height < 0) {
      return true;
   }

   node_path_frame_t inline_stack[NODE_PATH_STACK_INLINE_CAP];
   node_path_frame_t *stack = inline_stack;
   u64 len = 0;
   u64 cap = NODE_PATH_STACK_INLINE_CAP;
   node_seen_t seen = {0};
   bool exceeds = false;

   node_path_stack_push(&stack, inline_stack, &len, &cap, p, 0);
   while (len > 0) {
      node_path_frame_t frame = node_path_stack_pop(stack, &len);
      node_t *node = frame.node;
      u64 depth = frame.depth;

      node_seen_insert(&seen, node);
      if (depth > (u64)maximum_height) {
         exceeds = true;
#ifndef DEBUG
         break;
#endif
      }

      node_path_stack_push_children(&stack, inline_stack, &len, &cap, node,
                                    depth);
   }

   node_path_stack_dispose(stack, inline_stack, len, cap);
   node_seen_dispose(&seen);
   return exceeds;
}

int height(node_t *p) {
   return nodeMaximumPathLengthFromMetrics(nodePathMetrics(p));
}

double nodeAveragePathLength(node_t *p) {
   return nodeAveragePathLengthFromMetrics(nodePathMetrics(p));
}

// Counts the number of nodes reachable from p*, including itself.
u64 node_size(node_t *p) {
   node_t *inline_data[NODE_STACK_INLINE_CAP];
   node_stack_t stack = {
       .data = inline_data,
       .len = 0,
       .cap = NODE_STACK_INLINE_CAP,
   };
   u64 count = 0;
   node_seen_t seen = {0};

   node_stack_push(&stack, inline_data, p);
   while (stack.len > 0) {
      node_t *node = node_stack_pop(&stack);

      node_seen_insert(&seen, node);
      count = size_after_inserting_one(count);
      node_stack_push(&stack, inline_data, node->r);
      node_stack_push(&stack, inline_data, node->l);
   }

   node_stack_free(&stack, inline_data);
   node_seen_dispose(&seen);
   return count;
}

// ─────────────────────────────────────────────────────────────────────────────
// Verify sizes
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   node_t *node;
   u64 size;
   u64 depth;
} tree_verify_size_frame_t;

typedef struct {
   node_t *node;
   int state;
   bool is_right_child;
   u64 left_size;
   u64 right_size;
} tree_verify_reachable_size_frame_t;

enum {
   TREE_VERIFY_SIZE_STACK_INLINE_CAP = 128,
   TREE_VERIFY_REACHABLE_SIZE_STACK_INLINE_CAP = 128,
};

static void tree_verify_node_unchanged(node_t *node, node_t before) {
   invariant(node != NULL);
   invariant(node->l == before.l);
   invariant(node->r == before.r);
   invariant(node->s == before.s);
   invariant(node->x == before.x);
   invariant(node->y == before.y);
#ifdef PERSISTENT
   invariant(node->refs == before.refs);
#endif
}

static void tree_verify_size_stack_push(tree_verify_size_frame_t **stack,
                                        tree_verify_size_frame_t *inline_stack,
                                        u64 *len, u64 *cap, node_t *node,
                                        u64 size, u64 depth) {
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
      *stack = (tree_verify_size_frame_t *)stack_array_grow(
          *stack, inline_stack, *len, cap, sizeof(**stack));
   }

   invariant(*len < *cap);
   u64 index = *len;
   *len = size_after_inserting_one(*len);
   (*stack)[index] = (tree_verify_size_frame_t){
       .node = node,
       .size = size,
       .depth = depth,
   };
}

static tree_verify_size_frame_t
tree_verify_size_stack_pop(tree_verify_size_frame_t *stack, u64 *len) {
   invariant(stack != NULL);
   invariant(len != NULL);
   invariant(*len > 0);
   *len = size_after_removing_one(*len);
   return stack[*len];
}

static void
tree_verify_size_stack_dispose(tree_verify_size_frame_t *stack,
                               tree_verify_size_frame_t *inline_stack, u64 len,
                               u64 cap) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(cap > 0);
   invariant(len <= cap);
   invariant(stack == inline_stack || cap > TREE_VERIFY_SIZE_STACK_INLINE_CAP);
   if (stack != inline_stack) {
      free(stack);
   }
}

static void
tree_verify_size_stack_push_children(tree_verify_size_frame_t **stack,
                                     tree_verify_size_frame_t *inline_stack,
                                     u64 *len, u64 *cap, node_t *node,
                                     u64 left_size, u64 right_size, u64 depth) {
   invariant(node != NULL);
   if (node->l == NULL && node->r == NULL && left_size == 0 &&
       right_size == 0) {
      return;
   }

   u64 child_depth = size_after_inserting_one(depth);
   tree_verify_size_stack_push(stack, inline_stack, len, cap, node->r,
                               right_size, child_depth);
   tree_verify_size_stack_push(stack, inline_stack, len, cap, node->l,
                               left_size, child_depth);
}

static node_path_metrics_t
tree_verify_size_metrics_each_node(node_t *p, u64 s, tree_verify_node_fn verify,
                                   void *ctx) {
   tree_sized_root_require(p, s);
   if (p == NULL) {
      node_path_metrics_t metrics = {0};
      nodePathMetricsRequire(metrics);
      return metrics;
   }

   tree_verify_size_frame_t inline_stack[TREE_VERIFY_SIZE_STACK_INLINE_CAP];
   tree_verify_size_frame_t *stack = inline_stack;
   u64 len = 0;
   u64 cap = TREE_VERIFY_SIZE_STACK_INLINE_CAP;
   node_path_metrics_t metrics = {0};
   node_seen_t seen = {0};

   tree_verify_size_stack_push(&stack, inline_stack, &len, &cap, p, s, 0);
   while (len > 0) {
      tree_verify_size_frame_t frame = tree_verify_size_stack_pop(stack, &len);
      node_t *node = frame.node;
      u64 size = frame.size;
      u64 depth = frame.depth;

      invariant(size > 0);
      tree_node_local_shape_require(node, size);
      invariant(metrics.nodes < s);
      node_seen_insert(&seen, node);
      node_path_metrics_include(&metrics, depth);

      u64 sl = sizeL(node);
      u64 sr = sizeR(node, size);
      invariant(size == tree_size_from_child_sizes(sl, sr));
      if (verify != NULL) {
         node_t before = *node;
         verify(node, ctx);
         tree_verify_node_unchanged(node, before);
      }

      tree_verify_size_stack_push_children(&stack, inline_stack, &len, &cap,
                                           node, sl, sr, depth);
   }

   tree_verify_size_stack_dispose(stack, inline_stack, len, cap);
   invariant(metrics.nodes == s);
   node_seen_dispose(&seen);
   nodePathMetricsRequire(metrics);
   return metrics;
}

u64 tree_verify_size_each_node(node_t *p, u64 s, tree_verify_node_fn verify,
                               void *ctx) {
   return tree_verify_size_metrics_each_node(p, s, verify, ctx).nodes;
}

u64 tree_verify_size(node_t *p, u64 s) {
   return tree_verify_size_each_node(p, s, NULL, NULL);
}

node_path_metrics_t tree_verify_size_path_metrics(node_t *p, u64 s) {
   return tree_verify_size_metrics_each_node(p, s, NULL, NULL);
}

static void tree_verify_reachable_size_stack_push(
    tree_verify_reachable_size_frame_t **stack,
    tree_verify_reachable_size_frame_t *inline_stack, u64 *len, u64 *cap,
    node_t *node, bool is_right_child) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(len != NULL);
   invariant(cap != NULL);
   invariant(*stack != NULL);
   invariant(*cap > 0);
   invariant(*len <= *cap);
   if (node == NULL) {
      return;
   }

   if (*len == *cap) {
      *stack = (tree_verify_reachable_size_frame_t *)stack_array_grow(
          *stack, inline_stack, *len, cap, sizeof(**stack));
   }

   invariant(*len < *cap);
   u64 index = *len;
   *len = size_after_inserting_one(*len);
   (*stack)[index] = (tree_verify_reachable_size_frame_t){
       .node = node,
       .state = 0,
       .is_right_child = is_right_child,
       .left_size = 0,
       .right_size = 0,
   };
}

static void tree_verify_reachable_size_set_child_size(
    tree_verify_reachable_size_frame_t *parent, bool is_right_child, u64 size) {
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

static void tree_verify_reachable_size_finish_frame(
    tree_verify_reachable_size_frame_t *stack, u64 *len, bool is_right_child,
    u64 size, u64 *root_size) {
   invariant(stack != NULL);
   invariant(len != NULL);
   invariant(*len > 0);
   invariant(size > 0);
   invariant(root_size != NULL);

   *len = size_after_removing_one(*len);
   if (*len == 0) {
      *root_size = size;
   } else {
      tree_verify_reachable_size_frame_t *parent = &stack[rank_of_last(*len)];
      tree_verify_reachable_size_set_child_size(parent, is_right_child, size);
   }
}

static void tree_verify_reachable_size_stack_dispose(
    tree_verify_reachable_size_frame_t *stack,
    tree_verify_reachable_size_frame_t *inline_stack, u64 len, u64 cap) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(cap > 0);
   invariant(len <= cap);
   invariant(stack == inline_stack ||
             cap > TREE_VERIFY_REACHABLE_SIZE_STACK_INLINE_CAP);
   if (stack != inline_stack) {
      free(stack);
   }
}

u64 tree_verify_reachable_size_each_node(node_t *p, tree_verify_node_fn verify,
                                         void *ctx) {
   if (p == NULL) {
      return 0;
   }

   tree_verify_reachable_size_frame_t
       inline_stack[TREE_VERIFY_REACHABLE_SIZE_STACK_INLINE_CAP];
   tree_verify_reachable_size_frame_t *stack = inline_stack;
   u64 len = 0;
   u64 cap = TREE_VERIFY_REACHABLE_SIZE_STACK_INLINE_CAP;
   u64 visited = 0;
   u64 root_size = 0;
   node_seen_t seen = {0};

   tree_verify_reachable_size_stack_push(&stack, inline_stack, &len, &cap, p,
                                         false);
   while (len > 0) {
      tree_verify_reachable_size_frame_t *frame = &stack[rank_of_last(len)];

      if (frame->state == 0) {
         frame->state = 1;
         node_seen_insert(&seen, frame->node);
         visited = size_after_inserting_one(visited);
         tree_verify_reachable_size_stack_push(&stack, inline_stack, &len, &cap,
                                               frame->node->l, false);
         continue;
      }

      if (frame->state == 1) {
         frame->state = 2;
         tree_verify_reachable_size_stack_push(&stack, inline_stack, &len, &cap,
                                               frame->node->r, true);
         continue;
      }

      node_t *node = frame->node;
      invariant((node->l == NULL) == (frame->left_size == 0));
      invariant((node->r == NULL) == (frame->right_size == 0));
      invariant(sizeL(node) == frame->left_size);
      u64 size =
          tree_size_from_child_sizes(frame->left_size, frame->right_size);
      if (verify != NULL) {
         node_t before = *node;
         verify(node, ctx);
         tree_verify_node_unchanged(node, before);
      }

      bool is_right_child = frame->is_right_child;
      tree_verify_reachable_size_finish_frame(stack, &len, is_right_child, size,
                                              &root_size);
   }

   tree_verify_reachable_size_stack_dispose(stack, inline_stack, len, cap);
   invariant(root_size == visited);
   node_seen_dispose(&seen);
   return root_size;
}

u64 tree_verify_reachable_size(node_t *p) {
   return tree_verify_reachable_size_each_node(p, NULL, NULL);
}

// ─────────────────────────────────────────────────────────────────────────────
// Delete min / max
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   node_t *root;
   node_t *node;
} tree_delete_extreme_result_t;

static void tree_node_local_shape_require(node_t *node, u64 size) {
   invariant(node != NULL);
   if (size > 0) {
      tree_sized_root_shape_require(node, size);
      return;
   }

   invariant((node->l == NULL) == (node->s == 0));
}

static tree_delete_extreme_result_t tree_delete_min_root(node_t *root,
                                                         u64 size) {
   invariant(root != NULL);
   node_t n = {0};
   node_t *l = &n;
   for (;;) {
      tree_persist_unshared(&root);
      tree_node_local_shape_require(root, size);
      if (root->l == NULL) {
         l->l = root->r;
         root->r = NULL;
         return (tree_delete_extreme_result_t){
             .root = n.l,
             .node = root,
         };
      }
      invariant(root->l != NULL);
      u64 left_size = root->s;
      node_t **left = deleteL(root);
      l->l = root;
      l = l->l;
      root = *left;
      if (size > 0) {
         size = left_size;
      }
   }
}

static node_t *tree_deleteMin_known_size(node_t **p, u64 before) {
   invariant(p != NULL);
   invariant(*p != NULL);
#ifdef DEBUG
   u64 *original =
       tree_debug_capture_sequence((tree_t){.root = *p, .size = before});
#endif
   tree_delete_extreme_result_t result = tree_delete_min_root(*p, before);
   *p = result.root;
   node_t *min = result.node;
   invariant(min != NULL);
   invariant(min->l == NULL);
   invariant(min->r == NULL);
   invariant(!tree_shared(min));
#ifdef DEBUG
   u64 remaining_size = size_after_removing_one(before);
   tree_debug_require_size(*p, remaining_size);
   assert_bst(min->x == original[0]);
   u64 *remaining = tree_debug_capture_sequence(
       (tree_t){.root = *p, .size = remaining_size});
   tree_debug_require_values_slice(remaining, remaining_size, 0, original,
                                   before, 1, remaining_size);
   free(remaining);
   free(original);
#endif
   return min;
}

node_t *tree_deleteMin(node_t **p) {
   invariant(p != NULL);
   invariant(*p != NULL);
#ifdef DEBUG
   return tree_deleteMin_known_size(p, tree_verify_reachable_size(*p));
#else
   return tree_deleteMin_known_size(p, 0);
#endif
}

node_t *tree_deleteMin_sized(node_t **p, u64 size) {
   invariant(p != NULL);
   tree_nonempty_root_shape_require(*p, size);
   return tree_deleteMin_known_size(p, size);
}

static tree_delete_extreme_result_t tree_delete_max_root(node_t *root,
                                                         u64 size) {
   invariant(root != NULL);
   node_t n = {0};
   node_t *r = &n;
   for (;;) {
      tree_persist_unshared(&root);
      tree_node_local_shape_require(root, size);
      if (root->r == NULL) {
         r->r = root->l;
         root->l = NULL;
         root->s = 0;
         return (tree_delete_extreme_result_t){
             .root = n.r,
             .node = root,
         };
      }
      invariant(root->r != NULL);
      u64 right_size = size > 0 ? sizeR(root, size) : 0;
      r->r = root;
      r = r->r;
      root = root->r;
      if (size > 0) {
         size = right_size;
      }
   }
}

static node_t *tree_deleteMax_known_size(node_t **p, u64 before) {
   invariant(p != NULL);
   invariant(*p != NULL);
#ifdef DEBUG
   u64 *original =
       tree_debug_capture_sequence((tree_t){.root = *p, .size = before});
#endif
   tree_delete_extreme_result_t result = tree_delete_max_root(*p, before);
   *p = result.root;
   node_t *max = result.node;
   invariant(max != NULL);
   invariant(max->l == NULL);
   invariant(max->r == NULL);
   invariant(!tree_shared(max));
#ifdef DEBUG
   u64 remaining_size = size_after_removing_one(before);
   u64 max_rank = rank_of_last(before);
   tree_debug_require_size(*p, remaining_size);
   assert_bst(max->x == original[max_rank]);
   u64 *remaining = tree_debug_capture_sequence(
       (tree_t){.root = *p, .size = remaining_size});
   tree_debug_require_values_slice(remaining, remaining_size, 0, original,
                                   before, 0, remaining_size);
   free(remaining);
   free(original);
#endif
   return max;
}

node_t *tree_deleteMax(node_t **p) {
   invariant(p != NULL);
   invariant(*p != NULL);
#ifdef DEBUG
   return tree_deleteMax_known_size(p, tree_verify_reachable_size(*p));
#else
   return tree_deleteMax_known_size(p, 0);
#endif
}

node_t *tree_deleteMax_sized(node_t **p, u64 size) {
   invariant(p != NULL);
   tree_nonempty_root_shape_require(*p, size);
   return tree_deleteMax_known_size(p, size);
}

// ─────────────────────────────────────────────────────────────────────────────
// Dissolve
// ─────────────────────────────────────────────────────────────────────────────

node_t *tree_dissolve(node_t *p, u64 s) {
   tree_nonempty_root_shape_require(p, s);
   tree_node_local_shape_require(p, s);
   invariant(!tree_shared(p));
#ifdef DEBUG
   u64 removed_rank = p->s;
   u64 *original = tree_debug_capture_sequence((tree_t){.root = p, .size = s});
#endif
   node_t *root;
   // dissolvePreferLargerSubtree
   if (p->l == NULL) {
      root = p->r;
   } else if (p->r == NULL) {
      root = p->l;
   } else if (sizeL(p) < sizeR(p, s)) {
      node_t *r = tree_deleteMin_sized(&p->r, sizeR(p, s));
      invariant(!tree_shared(r));
      r->l = p->l;
      r->r = p->r;
      r->s = p->s;
      r->y = p->y;
      root = r;
   } else {
      invariant(p->s > 0);
      node_t *l = tree_deleteMax_sized(&p->l, sizeL(p));
      invariant(!tree_shared(l));
      l->r = p->r;
      l->l = p->l;
      l->s = rank_of_last(p->s);
      l->y = p->y;
      root = l;
   }
#ifdef DEBUG
   u64 remaining_size = size_after_removing_one(s);
   u64 *remaining = tree_debug_capture_sequence(
       (tree_t){.root = root, .size = remaining_size});
   tree_debug_require_values_slice(remaining, remaining_size, 0, original, s, 0,
                                   removed_rank);
   u64 suffix_size = suffix_size_after_rank(s, removed_rank);
   u64 suffix_offset = size_after_inserting_one(removed_rank);
   tree_debug_require_values_slice(remaining, remaining_size, removed_rank,
                                   original, s, suffix_offset, suffix_size);
   free(remaining);
   free(original);
#endif
   return root;
}

// ─────────────────────────────────────────────────────────────────────────────
// Lookup
// ─────────────────────────────────────────────────────────────────────────────

static void tree_require_shape(tree_t tree) {
   tree_sized_handle_require(tree);
   tree_debug_require_size(tree.root, tree.size);
}

#ifdef DEBUG
typedef struct {
   u64 *values;
   u64 len;
   u64 cap;
} tree_debug_sequence_t;

static void tree_debug_sequence_visit(u64 x, void *ctx) {
   tree_debug_sequence_t *seq = (tree_debug_sequence_t *)ctx;
   invariant(seq != NULL);
   invariant(seq->values != NULL);
   invariant(seq->len < seq->cap);
   u64 index = seq->len;
   seq->len = size_after_inserting_one(seq->len);
   seq->values[index] = x;
}

static u64 *tree_debug_capture_sequence(tree_t tree) {
   tree_require_shape(tree);
   if (tree.size == 0) {
      return NULL;
   }

   u64 *values = (u64 *)bst_malloc_array(tree.size, sizeof(values[0]));
   tree_debug_sequence_t seq = {
       .values = values,
       .len = 0,
       .cap = tree.size,
   };
   inorder(tree.root, tree_debug_sequence_visit, &seq);
   invariant(seq.len == tree.size);
   return values;
}

static void tree_debug_require_values_slice(const u64 *actual, u64 actual_count,
                                            u64 actual_offset,
                                            const u64 *expected,
                                            u64 expected_count,
                                            u64 expected_offset, u64 count) {
   invariant(actual_offset <= actual_count);
   invariant(count <= actual_count - actual_offset);
   invariant(expected_offset <= expected_count);
   invariant(count <= expected_count - expected_offset);
   if (count == 0) {
      return;
   }

   invariant(actual != NULL);
   invariant(expected != NULL);
   for (u64 i = 0; i < count; i++) {
      assert_bst(actual[actual_offset + i] == expected[expected_offset + i]);
   }
}
#endif

static void tree_clear_moved(tree_t *tree) {
   invariant(tree != NULL);
#ifndef PERSISTENT
   *tree = (tree_t){0};
#endif
}

u64 tree_lookup(tree_t *tree, u64 i) {
   invariant(tree != NULL);
   tree_require_shape(*tree);
   tree_rank_inputs_require(tree->root, i, tree->size);

   node_t *p = tree->root;
   u64 s = tree->size;
   for (;;) {
      invariant(p != NULL);
      invariant(p->s < s);
      tree_node_local_shape_require(p, s);
      if (i == p->s) {
         return p->x;
      }
      if (i < p->s) {
         invariant(p->l != NULL);
         s = p->s;
         p = p->l;
      } else {
         invariant(p->r != NULL);
         u64 sr = sizeR(p, s);
         i = rank_in_right_subtree(i, p->s);
         s = sr;
         invariant(i < s);
         p = p->r;
      }
   }
}

// ─────────────────────────────────────────────────────────────────────────────
// Update
// ─────────────────────────────────────────────────────────────────────────────

static void tree_update_internal(node_t *p, u64 s, u64 i, u64 x) {
   for (;;) {
      invariant(p != NULL);
      invariant(i < s);
      invariant(p->s < s);
      tree_node_local_shape_require(p, s);
      if (i == p->s) {
         p->x = x;
         return;
      }
      if (i < p->s) {
         invariant(p->l != NULL);
         tree_persist_unshared(&p->l);
         s = p->s;
         p = p->l;
      } else {
         invariant(p->r != NULL);
         tree_persist_unshared(&p->r);
         u64 sr = sizeR(p, s);
         i = rank_in_right_subtree(i, p->s);
         s = sr;
         p = p->r;
      }
   }
}

void tree_update_base(tree_t *tree, u64 i, u64 x) {
   invariant(tree != NULL);
   tree_require_shape(*tree);
   tree_rank_inputs_require(tree->root, i, tree->size);
   u64 size = tree->size;
#ifdef DEBUG
   u64 *original = tree_debug_capture_sequence(*tree);
#endif
   tree_persist_unshared(&tree->root);
   tree_update_internal(tree->root, tree->size, i, x);
   invariant(tree->size == size);
   invariant(tree_lookup(tree, i) == x);
   tree_require_shape(*tree);
#ifdef DEBUG
   u64 *updated = tree_debug_capture_sequence(*tree);
   tree_debug_require_values_slice(updated, size, 0, original, size, 0, i);
   assert_bst(updated[i] == x);
   u64 suffix_size = suffix_size_after_rank(size, i);
   u64 suffix_offset = size_after_inserting_one(i);
   tree_debug_require_values_slice(updated, size, suffix_offset, original, size,
                                   suffix_offset, suffix_size);
   free(updated);
   free(original);
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Base insert (unbalanced)
// ─────────────────────────────────────────────────────────────────────────────

void tree_insert_internal(node_t **p, u64 s, u64 i, u64 x) {
   invariant(p != NULL);
   (void)tree_insert_inputs_require(*p, i, s);
   for (;;) {
      invariant(i <= s);
      invariant(s < UINT64_MAX);
      if (*p == NULL) {
         invariant(s == 0);
         invariant(i == 0);
         *p = tree_allocate((node_t){.x = x});
         return;
      }
      tree_persist_unshared(p);
      node_t *node = *p;
      invariant(node != NULL);
      tree_node_local_shape_require(node, s);
      if (i > node->s) {
         u64 sr = sizeR(node, s);
         p = insertR(node, &i);
         s = sr;
      } else {
         u64 sl = sizeL(node);
         p = insertL(node);
         s = sl;
      }
   }
}

void tree_insert_base(tree_t *tree, u64 i, u64 x) {
   invariant(tree != NULL);
   tree_require_shape(*tree);
   u64 size = tree->size;
   u64 inserted_size = tree_insert_inputs_require(tree->root, i, size);
#ifdef DEBUG
   u64 *original = tree_debug_capture_sequence(*tree);
#endif
   tree_insert_internal(&tree->root, tree->size, i, x);
   tree->size = inserted_size;
   invariant(tree->size == inserted_size);
   invariant(tree_lookup(tree, i) == x);
   tree_require_shape(*tree);
#ifdef DEBUG
   u64 *inserted = tree_debug_capture_sequence(*tree);
   u64 suffix_size = sequence_split_inputs_require(i, size);
   tree_debug_require_values_slice(inserted, inserted_size, 0, original, size,
                                   0, i);
   assert_bst(inserted[i] == x);
   u64 inserted_suffix_offset = size_after_inserting_one(i);
   tree_debug_require_values_slice(inserted, inserted_size,
                                   inserted_suffix_offset, original, size, i,
                                   suffix_size);
   free(inserted);
   free(original);
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Base delete (unbalanced, uses dissolve)
// ─────────────────────────────────────────────────────────────────────────────

u64 tree_delete_internal(node_t **p, u64 s, u64 i) {
   invariant(p != NULL);
   (void)tree_delete_inputs_require(*p, i, s);
   for (;;) {
      invariant(*p != NULL);
      invariant(i < s);
      invariant((*p)->s < s);
      tree_persist_unshared(p);
      node_t *node = *p;
      invariant(node != NULL);
      tree_node_local_shape_require(node, s);
      if (i == node->s) {
         node_t *old = node;
         u64 x = old->x;
         node_t *root = tree_dissolve(old, s);
         tree_share(old->l);
         tree_share(old->r);
         tree_release(old);
         *p = root;
         return x;
      }
      if (i < node->s) {
         u64 sl = sizeL(node);
         invariant(node->l != NULL);
         p = deleteL(node);
         s = sl;
      } else {
         u64 sr = sizeR(node, s);
         invariant(node->r != NULL);
         p = deleteR(node, &i);
         s = sr;
      }
   }
}

u64 tree_delete_base(tree_t *tree, u64 i) {
   invariant(tree != NULL);
   tree_require_shape(*tree);
   u64 size = tree->size;
   u64 deleted_size = tree_delete_inputs_require(tree->root, i, size);
#ifdef DEBUG
   u64 *original = tree_debug_capture_sequence(*tree);
   u64 expected = original[i];
#endif
   u64 x = tree_delete_internal(&tree->root, tree->size, i);
   tree->size = deleted_size;
   invariant(tree->size == deleted_size);
   tree_require_shape(*tree);
#ifdef DEBUG
   assert_bst(x == expected);
   u64 *remaining = tree_debug_capture_sequence(*tree);
   tree_debug_require_values_slice(remaining, deleted_size, 0, original, size,
                                   0, i);
   u64 suffix_size = suffix_size_after_rank(size, i);
   u64 original_suffix_offset = size_after_inserting_one(i);
   tree_debug_require_values_slice(remaining, deleted_size, i, original, size,
                                   original_suffix_offset, suffix_size);
   free(remaining);
   free(original);
#endif
   return x;
}

// ─────────────────────────────────────────────────────────────────────────────
// Split / Join (base implementations)
// ─────────────────────────────────────────────────────────────────────────────

static tree_split_result_t tree_split_roots_impl(node_t *root, u64 split_size) {
   invariant(root != NULL || split_size == 0);
   node_t sentinel = {0};
   node_t *left_spine = &sentinel;
   node_t *right_spine = &sentinel;
   u64 left_remaining = split_size;
   node_t *node = root;
   while (node != NULL) {
      tree_persist_unshared(&node);
      invariant((node->l == NULL) == (node->s == 0));
      if (left_remaining <= node->s) {
         invariant(left_remaining <= node->s);
         invariant(left_remaining == 0 || node->l != NULL);
         node->s = suffix_size_after_prefix(node->s, left_remaining);
         right_spine->l = node;
         right_spine = right_spine->l;
         node = node->l;
      } else {
         invariant(left_remaining > node->s);
         left_remaining = rank_in_right_subtree(left_remaining, node->s);
         left_spine->r = node;
         left_spine = left_spine->r;
         node = node->r;
      }
   }
   left_spine->r = NULL;
   right_spine->l = NULL;
   invariant(left_remaining == 0);
   return (tree_split_result_t){
       .left = sentinel.r,
       .right = sentinel.l,
   };
}

tree_split_result_t tree_split_roots_sized(node_t *root, u64 split_size,
                                           u64 original_size) {
   u64 right_size = tree_split_inputs_require(root, split_size, original_size);
#ifndef DEBUG
   (void)right_size;
#endif
#ifdef DEBUG
   u64 *original = tree_debug_capture_sequence((tree_t){
       .root = root,
       .size = original_size,
   });
#endif
   tree_split_result_t result = tree_split_roots_impl(root, split_size);
#ifdef DEBUG
   u64 *left_values = tree_debug_capture_sequence((tree_t){
       .root = result.left,
       .size = split_size,
   });
   u64 *right_values = tree_debug_capture_sequence((tree_t){
       .root = result.right,
       .size = right_size,
   });
   tree_t left_tree = {.root = result.left, .size = split_size};
   tree_t right_tree = {.root = result.right, .size = right_size};
   assert_bst(!tree_roots_overlap(&left_tree, &right_tree));
   tree_debug_require_values_slice(left_values, split_size, 0, original,
                                   original_size, 0, split_size);
   tree_debug_require_values_slice(right_values, right_size, 0, original,
                                   original_size, split_size, right_size);
   free(left_values);
   free(right_values);
   free(original);
#endif
   return result;
}

tree_split_result_t tree_split_roots(node_t *root, u64 split_size) {
   invariant(root != NULL || split_size == 0);
#ifdef DEBUG
   u64 original_size = tree_verify_reachable_size(root);
   return tree_split_roots_sized(root, split_size, original_size);
#else
   return tree_split_roots_impl(root, split_size);
#endif
}

tree_split_t tree_split_base(tree_t *tree, u64 i) {
   invariant(tree != NULL);
   tree_require_shape(*tree);
   u64 size = tree->size;
   u64 right_size = tree_split_inputs_require(tree->root, i, size);
#ifdef DEBUG
   u64 *original = tree_debug_capture_sequence(*tree);
#endif
   tree_share(tree->root);
   tree_split_result_t split = tree_split_roots_sized(tree->root, i, size);
   tree_split_t result = {
       .left = {.root = split.left, .size = i},
       .right = {.root = split.right, .size = right_size},
   };
   tree_clear_moved(tree);
   tree_require_shape(result.left);
   tree_require_shape(result.right);
#ifdef DEBUG
   u64 *left_values = tree_debug_capture_sequence(result.left);
   u64 *right_values = tree_debug_capture_sequence(result.right);
   tree_debug_require_values_slice(left_values, i, 0, original, size, 0, i);
   tree_debug_require_values_slice(right_values, right_size, 0, original, size,
                                   i, right_size);
   free(left_values);
   free(right_values);
   free(original);
#endif
   return result;
}

node_t *tree_join_internal(node_t *l, node_t *r, u64 sl, u64 sr) {
   u64 total_size = tree_join_inputs_require(l, r, sl, sr);
#ifndef DEBUG
   (void)total_size;
#endif
#ifndef PERSISTENT
   invariant(l == NULL || r == NULL || l != r);
#endif
#ifdef DEBUG
   tree_debug_require_join_inputs(l, r, sl, sr);
#endif
   node_t *root;
   if (l == NULL) {
      root = r;
   } else if (r == NULL) {
      root = l;
   } else if (sl <= sr) {
      // tree_deleteMin copies the mutated right path; l is only linked.
      node_t *p = tree_deleteMin_sized(&r, sr);
      p->l = l;
      p->r = r;
      p->s = sl;
      root = p;
   } else {
      invariant(sl > 0);
      // tree_deleteMax copies the mutated left path; r is only linked.
      node_t *p = tree_deleteMax_sized(&l, sl);
      p->r = r;
      p->l = l;
      p->s = rank_of_last(sl);
      root = p;
   }
#ifdef DEBUG
   tree_debug_require_size(root, total_size);
#endif
   return root;
}

static tree_join_roots_t tree_join_roots_make(node_t *left_root,
                                              node_t *right_root, u64 left_size,
                                              u64 right_size) {
   tree_join_roots_t roots = {
       .left = left_root,
       .right = right_root,
   };
   tree_join_inputs_require(roots.left, roots.right, left_size, right_size);
#ifdef DEBUG
   tree_debug_require_join_inputs(roots.left, roots.right, left_size,
                                  right_size);
#endif
   return roots;
}

tree_join_roots_t tree_prepare_join_roots(tree_t *left, tree_t *right) {
   invariant(left != NULL);
   invariant(right != NULL);
   tree_sized_handle_require(*left);
   tree_sized_handle_require(*right);
#ifdef PERSISTENT
   if (tree_roots_overlap(left, right)) {
      // Avoid building a joined tree whose left and right sides can still reach
      // the same persistent node. Clone the smaller side to keep the physical
      // copy proportional to the minimum overlap-isolation cost.
      if (left->size < right->size) {
         tree_t left_copy = tree_clone_physical(left);
         tree_share(right->root);
         return tree_join_roots_make(left_copy.root, right->root, left->size,
                                     right->size);
      }
      tree_t right_copy = tree_clone_physical(right);
      tree_share(left->root);
      return tree_join_roots_make(left->root, right_copy.root, left->size,
                                  right->size);
   }
#else
   invariant(left->root == NULL || right->root == NULL ||
             left->root != right->root);
#ifdef DEBUG
   assert_bst(!tree_roots_overlap(left, right));
#endif
#endif
   tree_share(left->root);
   tree_share(right->root);
   return tree_join_roots_make(left->root, right->root, left->size,
                               right->size);
}

tree_t tree_join_base(tree_t *tree, tree_t *other) {
   invariant(tree != NULL);
   invariant(other != NULL);
   invariant(other != tree);
   tree_require_shape(*tree);
   tree_require_shape(*other);
#ifndef PERSISTENT
   invariant(tree->root == NULL || other->root == NULL ||
             tree->root != other->root);
#ifdef DEBUG
   assert_bst(!tree_roots_overlap(tree, other));
#endif
#endif
   u64 left_size = tree->size;
   u64 right_size = other->size;
   u64 joined_size = size_sum(left_size, right_size);
#ifdef DEBUG
   u64 *left_values = tree_debug_capture_sequence(*tree);
   u64 *right_values = tree_debug_capture_sequence(*other);
#endif
   tree_join_roots_t roots = tree_prepare_join_roots(tree, other);
   tree_t joined = {
       .root =
           tree_join_internal(roots.left, roots.right, left_size, right_size),
       .size = joined_size,
   };
   tree_clear_moved(tree);
   tree_clear_moved(other);
   tree_require_shape(joined);
#ifdef DEBUG
   u64 *joined_values = tree_debug_capture_sequence(joined);
   tree_debug_require_values_slice(joined_values, joined.size, 0, left_values,
                                   left_size, 0, left_size);
   tree_debug_require_values_slice(joined_values, joined.size, left_size,
                                   right_values, right_size, 0, right_size);
   free(joined_values);
   free(left_values);
   free(right_values);
#endif
   return joined;
}

// ─────────────────────────────────────────────────────────────────────────────
// Partition
// ─────────────────────────────────────────────────────────────────────────────

node_t *tree_partition_sized(node_t *p, u64 s, u64 i) {
   tree_rank_inputs_require(p, i, s);
#ifdef DEBUG
   u64 original_size = s;
   u64 target = i;
   u64 *original = tree_debug_capture_sequence((tree_t){.root = p, .size = s});
#endif
   node_t n = {.s = i};
   node_t *l = &n;
   node_t *r = &n;
   for (;;) {
      invariant(p != NULL);
      tree_node_local_shape_require(p, s);
      if (i == p->s) {
         break;
      }
      tree_persist_unshared(&p);
      invariant(sizeL(p) < s);
      if (i < p->s) {
         u64 sl = sizeL(p);
         invariant(p->l != NULL);
         p->s = rank_in_right_subtree(p->s, i);
         r->l = p;
         r = r->l;
         p = p->l;
         s = sl;
      } else {
         u64 sr = sizeR(p, s);
         i = rank_in_right_subtree(i, p->s);
         invariant(p->r != NULL);
         l->r = p;
         l = l->r;
         p = p->r;
         s = sr;
      }
   }
   tree_persist_unshared(&p);
   l->r = p->l;
   r->l = p->r;
   p->l = n.r;
   p->r = n.l;
   p->s = n.s;
#ifdef DEBUG
   invariant(sizeL(p) == target);
   u64 *partitioned = tree_debug_capture_sequence((tree_t){
       .root = p,
       .size = original_size,
   });
   tree_debug_require_values_slice(partitioned, original_size, 0, original,
                                   original_size, 0, original_size);
   free(partitioned);
   free(original);
#endif
   return p;
}

node_t *tree_partition(node_t *p, u64 i) {
   invariant(p != NULL);
   return tree_partition_sized(p, tree_verify_reachable_size(p), i);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tree rotations with persistence
// ─────────────────────────────────────────────────────────────────────────────

static void tree_rotate_l_input_require(node_t *root, u64 size) {
   tree_nonempty_root_require(root, size);
   tree_node_local_shape_require(root, size);
   invariant(root->r != NULL);
   u64 right_size = sizeR(root, size);
   tree_node_local_shape_require(root->r, right_size);
}

static void tree_rotate_r_input_require(node_t *root, u64 size) {
   tree_nonempty_root_require(root, size);
   tree_node_local_shape_require(root, size);
   invariant(root->l != NULL);
   tree_node_local_shape_require(root->l, root->s);
}

static void tree_rotate_rl_input_require(node_t *root, u64 size) {
   tree_rotate_l_input_require(root, size);
   node_t *right = root->r;
   invariant(right->l != NULL);
   tree_node_local_shape_require(right->l, right->s);
}

static void tree_rotate_lr_input_require(node_t *root, u64 size) {
   tree_rotate_r_input_require(root, size);
   node_t *left = root->l;
   invariant(left->r != NULL);
   u64 middle_size = sizeR(left, root->s);
   tree_node_local_shape_require(left->r, middle_size);
}

// Provisional rotations preserve persistence but skip whole-subtree size
// verification; top-down algorithms can use them while cached sizes describe
// the post-operation state before the physical deletion reaches the leaf.
void treeRotateLProvisional(node_t **p) {
   invariant(p != NULL);
   invariant(*p != NULL);
   invariant((*p)->r != NULL);
   invariant(!tree_shared(*p));
   tree_persist_unshared(&(*p)->r);
   *p = rotateL(*p);
   invariant(!tree_shared(*p));
}

void treeRotateL(node_t **p) {
   invariant(p != NULL);
   invariant(*p != NULL);
#ifdef DEBUG
   treeRotateLSized(p, tree_verify_reachable_size(*p));
#else
   treeRotateLProvisional(p);
#endif
}

void treeRotateLSized(node_t **p, u64 size) {
   invariant(p != NULL);
   tree_rotate_l_input_require(*p, size);
   treeRotateLProvisional(p);
#ifdef DEBUG
   tree_debug_require_size(*p, size);
#endif
}

void treeRotateRProvisional(node_t **p) {
   invariant(p != NULL);
   invariant(*p != NULL);
   invariant((*p)->l != NULL);
   invariant(!tree_shared(*p));
   tree_persist_unshared(&(*p)->l);
   *p = rotateR(*p);
   invariant(!tree_shared(*p));
}

void treeRotateR(node_t **p) {
   invariant(p != NULL);
   invariant(*p != NULL);
#ifdef DEBUG
   treeRotateRSized(p, tree_verify_reachable_size(*p));
#else
   treeRotateRProvisional(p);
#endif
}

void treeRotateRSized(node_t **p, u64 size) {
   invariant(p != NULL);
   tree_rotate_r_input_require(*p, size);
   treeRotateRProvisional(p);
#ifdef DEBUG
   tree_debug_require_size(*p, size);
#endif
}

void treeRotateRLProvisional(node_t **p) {
   invariant(p != NULL);
   invariant(*p != NULL);
   invariant((*p)->r != NULL);
   invariant((*p)->r->l != NULL);
   invariant(!tree_shared(*p));
   tree_persist_unshared(&(*p)->r);
   tree_persist_unshared(&(*p)->r->l);
   *p = rotateRL(*p);
   invariant(!tree_shared(*p));
}

void treeRotateRL(node_t **p) {
   invariant(p != NULL);
   invariant(*p != NULL);
#ifdef DEBUG
   treeRotateRLSized(p, tree_verify_reachable_size(*p));
#else
   treeRotateRLProvisional(p);
#endif
}

void treeRotateRLSized(node_t **p, u64 size) {
   invariant(p != NULL);
   tree_rotate_rl_input_require(*p, size);
   treeRotateRLProvisional(p);
#ifdef DEBUG
   tree_debug_require_size(*p, size);
#endif
}

void treeRotateLRProvisional(node_t **p) {
   invariant(p != NULL);
   invariant(*p != NULL);
   invariant((*p)->l != NULL);
   invariant((*p)->l->r != NULL);
   invariant(!tree_shared(*p));
   tree_persist_unshared(&(*p)->l);
   tree_persist_unshared(&(*p)->l->r);
   *p = rotateLR(*p);
   invariant(!tree_shared(*p));
}

void treeRotateLR(node_t **p) {
   invariant(p != NULL);
   invariant(*p != NULL);
#ifdef DEBUG
   treeRotateLRSized(p, tree_verify_reachable_size(*p));
#else
   treeRotateLRProvisional(p);
#endif
}

void treeRotateLRSized(node_t **p, u64 size) {
   invariant(p != NULL);
   tree_rotate_lr_input_require(*p, size);
   treeRotateLRProvisional(p);
#ifdef DEBUG
   tree_debug_require_size(*p, size);
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Append to rightmost / leftmost
// ─────────────────────────────────────────────────────────────────────────────

#ifdef DEBUG
static bool tree_debug_reaches_node(node_t *root, node_t *target) {
   invariant(target != NULL);

   node_t *inline_data[NODE_STACK_INLINE_CAP];
   node_stack_t stack = {
       .data = inline_data,
       .len = 0,
       .cap = NODE_STACK_INLINE_CAP,
   };
   node_seen_t seen = {0};

   node_stack_push(&stack, inline_data, root);
   while (stack.len > 0) {
      node_t *node = node_stack_pop(&stack);
      if (node == target) {
         node_stack_free(&stack, inline_data);
         node_seen_dispose(&seen);
         return true;
      }
      node_seen_insert(&seen, node);
      node_stack_push(&stack, inline_data, node->r);
      node_stack_push(&stack, inline_data, node->l);
   }

   node_stack_free(&stack, inline_data);
   node_seen_dispose(&seen);
   return false;
}

static void tree_debug_require_append_root_not_reachable(node_t *root,
                                                         node_t *appended) {
   if (appended == NULL) {
      return;
   }
   assert_bst(!tree_debug_reaches_node(root, appended));
}
#endif

void tree_appendR_raw(node_t **p, node_t *n) {
   invariant(p != NULL);
   if (n == NULL) {
      return;
   }
   invariant(!tree_shared(n));
#ifdef DEBUG
   tree_debug_require_append_root_not_reachable(*p, n);
#endif
   while (*p != NULL) {
      tree_persist_unshared(p);
      p = &(*p)->r;
   }
   *p = n;
}

void tree_appendL_raw(node_t **p, node_t *n) {
   invariant(p != NULL);
   if (n == NULL) {
      return;
   }
   invariant(!tree_shared(n));
#ifdef DEBUG
   tree_debug_require_append_root_not_reachable(*p, n);
#endif
   while (*p != NULL) {
      tree_persist_unshared(p);
      p = &(*p)->l;
   }
   *p = n;
}

// ─────────────────────────────────────────────────────────────────────────────
// replacedBy helpers
// ─────────────────────────────────────────────────────────────────────────────

static node_t *tree_replacedByRightSubtree_known_size(node_t **p, u64 before) {
   invariant(p != NULL);
   invariant(*p != NULL);
   tree_node_local_shape_require(*p, before);
#ifdef DEBUG
   u64 detached_size = size_after_inserting_one((*p)->s);
   u64 replacement_size = suffix_size_after_prefix(before, detached_size);
   u64 *original =
       tree_debug_capture_sequence((tree_t){.root = *p, .size = before});
#endif
   tree_persist_unshared(p);
   node_t *r = *p;
   *p = (*p)->r;
   r->r = NULL;
   invariant(r->r == NULL);
#ifdef DEBUG
   u64 *replacement = tree_debug_capture_sequence(
       (tree_t){.root = *p, .size = replacement_size});
   u64 *detached =
       tree_debug_capture_sequence((tree_t){.root = r, .size = detached_size});
   tree_t replacement_tree = {.root = *p, .size = replacement_size};
   tree_t detached_tree = {.root = r, .size = detached_size};
   assert_bst(!tree_roots_overlap(&replacement_tree, &detached_tree));
   tree_debug_require_values_slice(detached, detached_size, 0, original, before,
                                   0, detached_size);
   tree_debug_require_values_slice(replacement, replacement_size, 0, original,
                                   before, detached_size, replacement_size);
   free(replacement);
   free(detached);
   free(original);
#endif
   return r;
}

node_t *tree_replacedByRightSubtree(node_t **p) {
   invariant(p != NULL);
   invariant(*p != NULL);
#ifdef DEBUG
   return tree_replacedByRightSubtree_known_size(
       p, tree_verify_reachable_size(*p));
#else
   return tree_replacedByRightSubtree_known_size(p, 0);
#endif
}

node_t *tree_replacedByRightSubtree_sized(node_t **p, u64 size) {
   invariant(p != NULL);
   tree_nonempty_root_shape_require(*p, size);
   return tree_replacedByRightSubtree_known_size(p, size);
}

static node_t *tree_replacedByLeftSubtree_known_size(node_t **p, u64 before) {
   invariant(p != NULL);
   invariant(*p != NULL);
   tree_node_local_shape_require(*p, before);
#ifdef DEBUG
   u64 replacement_size = (*p)->s;
   u64 detached_size = suffix_size_after_prefix(before, replacement_size);
   u64 *original =
       tree_debug_capture_sequence((tree_t){.root = *p, .size = before});
#endif
   tree_persist_unshared(p);
   node_t *l = *p;
   *p = (*p)->l;
   l->l = NULL;
   l->s = 0;
   invariant(l->l == NULL);
#ifdef DEBUG
   u64 *replacement = tree_debug_capture_sequence(
       (tree_t){.root = *p, .size = replacement_size});
   u64 *detached =
       tree_debug_capture_sequence((tree_t){.root = l, .size = detached_size});
   tree_t replacement_tree = {.root = *p, .size = replacement_size};
   tree_t detached_tree = {.root = l, .size = detached_size};
   assert_bst(!tree_roots_overlap(&replacement_tree, &detached_tree));
   tree_debug_require_values_slice(replacement, replacement_size, 0, original,
                                   before, 0, replacement_size);
   tree_debug_require_values_slice(detached, detached_size, 0, original, before,
                                   replacement_size, detached_size);
   free(replacement);
   free(detached);
   free(original);
#endif
   return l;
}

node_t *tree_replacedByLeftSubtree(node_t **p) {
   invariant(p != NULL);
   invariant(*p != NULL);
#ifdef DEBUG
   return tree_replacedByLeftSubtree_known_size(p,
                                                tree_verify_reachable_size(*p));
#else
   return tree_replacedByLeftSubtree_known_size(p, 0);
#endif
}

node_t *tree_replacedByLeftSubtree_sized(node_t **p, u64 size) {
   invariant(p != NULL);
   tree_nonempty_root_shape_require(*p, size);
   return tree_replacedByLeftSubtree_known_size(p, size);
}

// ─────────────────────────────────────────────────────────────────────────────
// pathLeft / pathRight (triple-pointer traversal)
// ─────────────────────────────────────────────────────────────────────────────

void tree_pathLeft(node_t ***p) {
   invariant(p != NULL);
   invariant(*p != NULL);
   invariant(**p != NULL);
   tree_node_local_shape_require(**p, 0);
   invariant((**p)->l != NULL);
   invariant((**p)->s < UINT64_MAX);
   invariant(!tree_shared(**p));
   tree_persist_unshared(&(**p)->l);
   *p = insertL(**p);
}

void tree_pathRight(node_t ***p, u64 *i) {
   invariant(p != NULL);
   invariant(*p != NULL);
   invariant(**p != NULL);
   invariant(i != NULL);
   tree_node_local_shape_require(**p, 0);
   invariant(*i > (**p)->s);
   invariant((**p)->r != NULL);
   invariant(!tree_shared(**p));
   tree_persist_unshared(&(**p)->r);
   *p = insertR(**p, i);
}

void tree_pathLeftSized(node_t ***p, u64 *size) {
   invariant(p != NULL);
   invariant(*p != NULL);
   invariant(size != NULL);
   tree_nonempty_root_shape_require(**p, *size);
   tree_node_local_shape_require(**p, *size);
   u64 child_size = sizeL(**p);
   tree_pathLeft(p);
   *size = child_size;
   tree_sized_root_shape_require(**p, *size);
}

void tree_pathRightSized(node_t ***p, u64 *size, u64 *i) {
   invariant(p != NULL);
   invariant(*p != NULL);
   invariant(size != NULL);
   tree_nonempty_root_shape_require(**p, *size);
   tree_node_local_shape_require(**p, *size);
   u64 child_size = sizeR(**p, *size);
   tree_pathRight(p, i);
   *size = child_size;
   tree_sized_root_shape_require(**p, *size);
}

// ─────────────────────────────────────────────────────────────────────────────
// attach helpers
// ─────────────────────────────────────────────────────────────────────────────

void tree_attach(node_t **p, u64 x) {
   invariant(p != NULL);
   invariant(*p == NULL);
   *p = tree_allocate((node_t){.x = x});
}

void tree_attachL(node_t *p, u64 x) {
   invariant(p != NULL);
   invariant(p->l == NULL);
   invariant(p->s < UINT64_MAX);
   invariant(!tree_shared(p));
   p->s = size_after_inserting_one(p->s);
   p->l = tree_allocate((node_t){.x = x});
}

void tree_attachR(node_t *p, u64 x) {
   invariant(p != NULL);
   invariant(p->r == NULL);
   invariant(!tree_shared(p));
   p->r = tree_allocate((node_t){.x = x});
}

void tree_attachLL(node_t *p, u64 x) {
   invariant(p != NULL);
   invariant(p->l != NULL);
   invariant(p->l->l == NULL);
   invariant(p->s < UINT64_MAX);
   invariant(p->l->s < UINT64_MAX);
   invariant(!tree_shared(p));
   tree_persist_unshared(&p->l);
   p->s = size_after_inserting_one(p->s);
   p->l->s = size_after_inserting_one(p->l->s);
   p->l->l = tree_allocate((node_t){.x = x});
}

void tree_attachRR(node_t *p, u64 x) {
   invariant(p != NULL);
   invariant(p->r != NULL);
   invariant(p->r->r == NULL);
   invariant(!tree_shared(p));
   tree_persist_unshared(&p->r);
   p->r->r = tree_allocate((node_t){.x = x});
}

void tree_attachLR(node_t *p, u64 x) {
   invariant(p != NULL);
   invariant(p->l != NULL);
   invariant(p->l->r == NULL);
   invariant(p->s < UINT64_MAX);
   invariant(!tree_shared(p));
   tree_persist_unshared(&p->l);
   p->s = size_after_inserting_one(p->s);
   p->l->r = tree_allocate((node_t){.x = x});
}

void tree_attachRL(node_t *p, u64 x) {
   invariant(p != NULL);
   invariant(p->r != NULL);
   invariant(p->r->l == NULL);
   invariant(p->r->s < UINT64_MAX);
   invariant(!tree_shared(p));
   tree_persist_unshared(&p->r);
   p->r->s = size_after_inserting_one(p->r->s);
   p->r->l = tree_allocate((node_t){.x = x});
}
