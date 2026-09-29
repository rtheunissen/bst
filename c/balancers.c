#include "balancers.h"
#include "weight.h"

#ifdef DEBUG
typedef struct {
   node_t **nodes;
   u64 *values;
   u64 count;
} balancer_order_snapshot_t;

static balancer_order_snapshot_t balancer_order_snapshot(tree_t tree);
static void balancer_order_snapshot_free(balancer_order_snapshot_t *snapshot);
static void balancer_require_same_order(const balancer_order_snapshot_t *before,
                                        tree_t tree);
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Partition (balancer-specific, without persistence)
//
// Moves the node at position i to the root using split/reassemble.
// Unlike tree_partition in tree.c, this does NOT call tree_persist.
// ─────────────────────────────────────────────────────────────────────────────

static node_t *balancer_partition(node_t *p, u64 s, u64 i) {
   invariant(p != NULL);
   invariant(s > 0);
   invariant(i < s);
   invariant(sizeL(p) < s);
   measurement_record_partition();

   u64 root_size = s;
   u64 target = i;

   node_t n = {.s = i};
   node_t *l = &n;
   node_t *r = &n;
   for (;;) {
      invariant(p != NULL);
      invariant(!tree_shared(p));
      if (i == p->s) {
         break;
      }
      invariant(sizeL(p) < s);
      measurement_record_partition_descent();
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
   invariant(p != NULL);
   invariant(i == p->s);
   invariant(sizeL(p) < s);
   l->r = p->l;
   r->l = p->r;
   p->l = n.r;
   p->r = n.l;
   p->s = n.s;
   invariant(sizeL(p) == target);
   invariant((p->l == NULL) == (target == 0));
   invariant((p->r == NULL) == (sizeR(p, root_size) == 0));
   return p;
}

// ─────────────────────────────────────────────────────────────────────────────
// Randomize: shuffles tree shape using random partitions
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   node_t **root;
   u64 size;
} balancer_subtree_frame_t;

enum {
   BALANCER_SUBTREE_STACK_INLINE_CAP = 128,
};

static void balancer_subtree_stack_push(balancer_subtree_frame_t **stack,
                                        balancer_subtree_frame_t *inline_stack,
                                        u64 *len, u64 *cap, node_t **root,
                                        u64 size) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(len != NULL);
   invariant(cap != NULL);
   invariant(root != NULL);
   invariant(*stack != NULL);
   invariant(*cap > 0);
   invariant(*len <= *cap);
   if (*root == NULL) {
      invariant(size == 0);
      return;
   }
   invariant(size > 0);

   if (*len == *cap) {
      *stack = (balancer_subtree_frame_t *)stack_array_grow(
          *stack, inline_stack, *len, cap, sizeof(**stack));
   }

   invariant(*len < *cap);
   u64 index = *len;
   *len = size_after_inserting_one(*len);
   (*stack)[index] = (balancer_subtree_frame_t){.root = root, .size = size};
}

static void
balancer_subtree_stack_dispose(balancer_subtree_frame_t *stack,
                               balancer_subtree_frame_t *inline_stack, u64 len,
                               u64 cap) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(cap > 0);
   invariant(len <= cap);
   invariant(stack == inline_stack || cap > BALANCER_SUBTREE_STACK_INLINE_CAP);
   if (stack != inline_stack) {
      free(stack);
   }
}

#ifdef DEBUG
static balancer_order_snapshot_t balancer_order_snapshot(tree_t tree) {
   tree_sized_handle_require(tree);
   balancer_order_snapshot_t snapshot = {0};
   if (tree.size == 0) {
      return snapshot;
   }

   snapshot.nodes =
       (node_t **)bst_malloc_array(tree.size, sizeof(snapshot.nodes[0]));
   snapshot.values =
       (u64 *)bst_malloc_array(tree.size, sizeof(snapshot.values[0]));

   node_t *inline_data[NODE_STACK_INLINE_CAP];
   node_stack_t stack = {
       .data = inline_data,
       .len = 0,
       .cap = NODE_STACK_INLINE_CAP,
   };

   node_t *node = tree.root;
   while (node != NULL || stack.len > 0) {
      while (node != NULL) {
         node_stack_push(&stack, inline_data, node);
         node = node->l;
      }

      node = node_stack_pop(&stack);
      invariant(snapshot.count < tree.size);
      u64 index = snapshot.count;
      snapshot.count = size_after_inserting_one(snapshot.count);
      snapshot.nodes[index] = node;
      snapshot.values[index] = node->x;
      node = node->r;
   }

   node_stack_free(&stack, inline_data);
   invariant(snapshot.count == tree.size);
   return snapshot;
}

static void balancer_order_snapshot_free(balancer_order_snapshot_t *snapshot) {
   invariant(snapshot != NULL);
   free(snapshot->nodes);
   free(snapshot->values);
   *snapshot = (balancer_order_snapshot_t){0};
}

static void balancer_require_same_order(const balancer_order_snapshot_t *before,
                                        tree_t tree) {
   invariant(before != NULL);
   balancer_order_snapshot_t after = balancer_order_snapshot(tree);
   invariant(after.count == before->count);
   for (u64 i = 0; i < before->count; i++) {
      invariant(after.nodes[i] == before->nodes[i]);
      invariant(after.values[i] == before->values[i]);
   }
   balancer_order_snapshot_free(&after);
}
#endif

tree_t tree_randomize(tree_t tree, distribution_t *dist) {
   invariant(dist != NULL);
   tree_sized_handle_require(tree);
   tree_debug_require_size(tree.root, tree.size);
#ifdef DEBUG
   balancer_order_snapshot_t original_order = balancer_order_snapshot(tree);
#endif

   balancer_subtree_frame_t inline_stack[BALANCER_SUBTREE_STACK_INLINE_CAP];
   balancer_subtree_frame_t *stack = inline_stack;
   u64 len = 0;
   u64 cap = BALANCER_SUBTREE_STACK_INLINE_CAP;

   balancer_subtree_stack_push(&stack, inline_stack, &len, &cap, &tree.root,
                               tree.size);
   while (len > 0) {
      len = size_after_removing_one(len);
      balancer_subtree_frame_t frame = stack[len];
      node_t *p = *frame.root;
      u64 size = frame.size;

      invariant(p != NULL);
      invariant(size > 0);
      invariant(sizeL(p) < size);
      invariant(!tree_shared(p));

      if (size == 1) {
         invariant(p->l == NULL);
         invariant(p->r == NULL);
         invariant(p->s == 0);
         continue;
      }

      p = balancer_partition(p, size, dist_less_than(dist, size));
      *frame.root = p;

      balancer_subtree_stack_push(&stack, inline_stack, &len, &cap, &p->r,
                                  sizeR(p, size));
      balancer_subtree_stack_push(&stack, inline_stack, &len, &cap, &p->l,
                                  sizeL(p));
   }

   balancer_subtree_stack_dispose(stack, inline_stack, len, cap);
   tree_sized_handle_require(tree);
   tree_debug_require_size(tree.root, tree.size);
#ifdef DEBUG
   balancer_require_same_order(&original_order, tree);
   balancer_order_snapshot_free(&original_order);
#endif
   return tree;
}

typedef bool (*balancer_is_balanced_fn)(node_t *node, u64 size);

static bool int_distance_at_most(int a, int b, int max) {
   invariant(max >= 0);
   int64_t distance = (int64_t)a - (int64_t)b;
   if (distance < 0) {
      distance = -distance;
   }
   return distance <= max;
}

static int balancer_height_from_children(int left_height, int right_height) {
   invariant(left_height >= -1);
   invariant(right_height >= -1);
   int child_height = MAX(left_height, right_height);
   invariant(child_height < INT_MAX);
   return child_height + 1;
}

static u64 balancer_child_weight(u64 child_size) {
   return size_after_inserting_one(child_size);
}

static bool balancer_sizes_differ_at_most(u64 left_size, u64 right_size,
                                          u64 maximum_distance) {
   return left_size >= right_size
              ? suffix_size_after_prefix(left_size, right_size) <=
                    maximum_distance
              : suffix_size_after_prefix(right_size, left_size) <=
                    maximum_distance;
}

static tree_t
balancer_restore_by_partition(tree_t tree, u64 stable_size,
                              balancer_is_balanced_fn is_balanced) {
   invariant(is_balanced != NULL);
   invariant(stable_size > 0);
   tree_sized_handle_require(tree);
   tree_debug_require_size(tree.root, tree.size);
#ifdef DEBUG
   balancer_order_snapshot_t original_order = balancer_order_snapshot(tree);
#endif

   balancer_subtree_frame_t inline_stack[BALANCER_SUBTREE_STACK_INLINE_CAP];
   balancer_subtree_frame_t *stack = inline_stack;
   u64 len = 0;
   u64 cap = BALANCER_SUBTREE_STACK_INLINE_CAP;

   balancer_subtree_stack_push(&stack, inline_stack, &len, &cap, &tree.root,
                               tree.size);
   while (len > 0) {
      len = size_after_removing_one(len);
      balancer_subtree_frame_t frame = stack[len];
      node_t *p = *frame.root;
      u64 size = frame.size;

      invariant(p != NULL);
      invariant(size > 0);
      invariant(sizeL(p) < size);
      invariant(!tree_shared(p));

      if (size <= stable_size) {
         continue;
      }

      if (!is_balanced(p, size)) {
         p = balancer_partition(p, size, size >> 1);
         *frame.root = p;
      }

      balancer_subtree_stack_push(&stack, inline_stack, &len, &cap, &p->r,
                                  sizeR(p, size));
      balancer_subtree_stack_push(&stack, inline_stack, &len, &cap, &p->l,
                                  sizeL(p));
   }

   balancer_subtree_stack_dispose(stack, inline_stack, len, cap);
   tree_sized_handle_require(tree);
   tree_debug_require_size(tree.root, tree.size);
#ifdef DEBUG
   balancer_require_same_order(&original_order, tree);
   balancer_order_snapshot_free(&original_order);
#endif
   return tree;
}

typedef struct {
   node_t *node;
   u64 size;
} balancer_verify_frame_t;

typedef void (*balancer_verify_node_fn)(node_t *node, u64 size, void *ctx);

enum {
   BALANCER_VERIFY_STACK_INLINE_CAP = 128,
};

static void balancer_verify_stack_push(balancer_verify_frame_t **stack,
                                       balancer_verify_frame_t *inline_stack,
                                       u64 *len, u64 *cap, node_t *node,
                                       u64 size) {
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
      *stack = (balancer_verify_frame_t *)stack_array_grow(
          *stack, inline_stack, *len, cap, sizeof(**stack));
   }

   invariant(*len < *cap);
   u64 index = *len;
   *len = size_after_inserting_one(*len);
   (*stack)[index] = (balancer_verify_frame_t){.node = node, .size = size};
}

static void balancer_verify_stack_dispose(balancer_verify_frame_t *stack,
                                          balancer_verify_frame_t *inline_stack,
                                          u64 len, u64 cap) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(cap > 0);
   invariant(len <= cap);
   invariant(stack == inline_stack || cap > BALANCER_VERIFY_STACK_INLINE_CAP);
   if (stack != inline_stack) {
      free(stack);
   }
}

static void balancer_verify_each_node(tree_t tree,
                                      balancer_verify_node_fn verify,
                                      void *ctx) {
   invariant(verify != NULL);
   tree_sized_handle_require(tree);

   balancer_verify_frame_t inline_stack[BALANCER_VERIFY_STACK_INLINE_CAP];
   balancer_verify_frame_t *stack = inline_stack;
   u64 len = 0;
   u64 cap = BALANCER_VERIFY_STACK_INLINE_CAP;
   u64 visited = 0;
   node_seen_t seen = {0};

   balancer_verify_stack_push(&stack, inline_stack, &len, &cap, tree.root,
                              tree.size);
   while (len > 0) {
      len = size_after_removing_one(len);
      balancer_verify_frame_t frame = stack[len];
      node_t *node = frame.node;
      u64 size = frame.size;

      node_seen_insert(&seen, node);
      invariant(sizeL(node) < size);
      u64 sl = sizeL(node);
      u64 sr = sizeR(node, size);

      invariant((node->l == NULL) == (sl == 0));
      invariant((node->r == NULL) == (sr == 0));
      visited = size_after_inserting_one(visited);
      verify(node, size, ctx);

      balancer_verify_stack_push(&stack, inline_stack, &len, &cap, node->r, sr);
      balancer_verify_stack_push(&stack, inline_stack, &len, &cap, node->l, sl);
   }

   balancer_verify_stack_dispose(stack, inline_stack, len, cap);
   invariant(visited == tree.size);
   node_seen_dispose(&seen);
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
} balancer_height_verify_frame_t;

typedef void (*balancer_verify_height_node_fn)(node_t *node, u64 size,
                                               int left_height,
                                               int right_height, void *ctx);

enum {
   BALANCER_HEIGHT_VERIFY_STACK_INLINE_CAP = 128,
};

static void
balancer_height_verify_stack_push(balancer_height_verify_frame_t **stack,
                                  balancer_height_verify_frame_t *inline_stack,
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
      *stack = (balancer_height_verify_frame_t *)stack_array_grow(
          *stack, inline_stack, *len, cap, sizeof(**stack));
   }

   invariant(*len < *cap);
   u64 index = *len;
   *len = size_after_inserting_one(*len);
   (*stack)[index] = (balancer_height_verify_frame_t){
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

static void balancer_height_verify_stack_dispose(
    balancer_height_verify_frame_t *stack,
    balancer_height_verify_frame_t *inline_stack, u64 len, u64 cap) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(cap > 0);
   invariant(len <= cap);
   invariant(stack == inline_stack ||
             cap > BALANCER_HEIGHT_VERIFY_STACK_INLINE_CAP);
   if (stack != inline_stack) {
      free(stack);
   }
}

static void
balancer_height_verify_set_child_size(balancer_height_verify_frame_t *parent,
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
balancer_height_verify_set_child_height(balancer_height_verify_frame_t *parent,
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

static void balancer_height_verify_finish_frame(
    balancer_height_verify_frame_t *stack, u64 *len, bool is_right_child,
    int height, u64 size, u64 *root_size, int *root_height) {
   invariant(stack != NULL);
   invariant(len != NULL);
   invariant(*len > 0);
   invariant(height >= 0);
   invariant(size > 0);
   invariant(root_size != NULL);
   invariant(root_height != NULL);

   *len = size_after_removing_one(*len);
   if (*len == 0) {
      *root_size = size;
      *root_height = height;
   } else {
      balancer_height_verify_frame_t *parent = &stack[rank_of_last(*len)];
      balancer_height_verify_set_child_size(parent, is_right_child, size);
      balancer_height_verify_set_child_height(parent, is_right_child, height);
   }
}

static int balancer_verify_height(tree_t tree,
                                  balancer_verify_height_node_fn verify,
                                  void *ctx) {
   invariant(verify != NULL);
   tree_sized_handle_require(tree);

   balancer_height_verify_frame_t
       inline_stack[BALANCER_HEIGHT_VERIFY_STACK_INLINE_CAP];
   balancer_height_verify_frame_t *stack = inline_stack;
   u64 len = 0;
   u64 cap = BALANCER_HEIGHT_VERIFY_STACK_INLINE_CAP;
   u64 visited = 0;
   u64 root_size = 0;
   int root_height = -1;
   node_seen_t seen = {0};

   balancer_height_verify_stack_push(&stack, inline_stack, &len, &cap,
                                     tree.root, tree.size, false);
   while (len > 0) {
      balancer_height_verify_frame_t *frame = &stack[rank_of_last(len)];
      node_t *node = frame->node;
      u64 size = frame->size;

      if (frame->state == 0) {
         frame->state = 1;
         node_seen_insert(&seen, node);
         invariant(sizeL(node) < size);
         invariant((node->l == NULL) == (sizeL(node) == 0));
         balancer_height_verify_stack_push(&stack, inline_stack, &len, &cap,
                                           node->l, sizeL(node), false);
         continue;
      }

      if (frame->state == 1) {
         frame->state = 2;
         invariant((node->r == NULL) == (sizeR(node, size) == 0));
         balancer_height_verify_stack_push(&stack, inline_stack, &len, &cap,
                                           node->r, sizeR(node, size), true);
         continue;
      }

      invariant((node->l == NULL) == (frame->left_height == -1));
      invariant((node->r == NULL) == (frame->right_height == -1));
      invariant((node->l == NULL) == (frame->left_size == 0));
      invariant((node->r == NULL) == (frame->right_size == 0));
      invariant(frame->left_size == sizeL(node));
      invariant(frame->right_size == sizeR(node, size));
      verify(node, size, frame->left_height, frame->right_height, ctx);
      visited = size_after_inserting_one(visited);
      int h = balancer_height_from_children(frame->left_height,
                                            frame->right_height);
      bool is_right_child = frame->is_right_child;
      balancer_height_verify_finish_frame(stack, &len, is_right_child, h, size,
                                          &root_size, &root_height);
   }

   balancer_height_verify_stack_dispose(stack, inline_stack, len, cap);
   invariant(visited == tree.size);
   invariant(root_size == tree.size);
   node_seen_dispose(&seen);
   return root_height;
}

// ─────────────────────────────────────────────────────────────────────────────
// Median
//
// -1 <= L - R <= 1
// ─────────────────────────────────────────────────────────────────────────────

static bool median_isBalanced(node_t *p, u64 s) {
   u64 sl = sizeL(p);
   u64 sr = sizeR(p, s);
   return balancer_sizes_differ_at_most(sl, sr, 1);
}

static tree_t median_restore(tree_t tree) {
   return balancer_restore_by_partition(tree, 2, median_isBalanced);
}

static void median_verify_node(node_t *p, u64 s, void *ctx) {
   (void)ctx;
   u64 sl = sizeL(p);
   u64 sr = sizeR(p, s);
   invariant(balancer_sizes_differ_at_most(sl, sr, 1));
}

static void median_verify(tree_t tree) {
   balancer_verify_each_node(tree, median_verify_node, NULL);
}

const balancer_t balancer_median = {
    .name = "Median",
    .restore = median_restore,
    .verify = median_verify,
};

// ─────────────────────────────────────────────────────────────────────────────
// Height
//
// A node is height-balanced when the difference between the height of its
// subtrees is no greater than 1, and both subtrees are also height-balanced.
//
// invariant(height(p) <= FloorLog2(s))
// ─────────────────────────────────────────────────────────────────────────────

static bool height_isBalanced(node_t *p, u64 s) {
   u64 sl = sizeL(p);
   u64 sr = sizeR(p, s);
   return util_gte_msb(balancer_child_weight(sl), sr) &&
          util_gte_msb(balancer_child_weight(sr), sl);
}

static tree_t height_restore(tree_t tree) {
   return balancer_restore_by_partition(tree, 2, height_isBalanced);
}

static void height_verify_node(node_t *p, u64 s, int heightL, int heightR,
                               void *ctx) {
   (void)p;
   (void)ctx;
   invariant(int_distance_at_most(heightL, heightR, 1));
   int h = balancer_height_from_children(heightL, heightR);
   u64 bound = util_log2(s);
   invariant(bound <= (u64)INT_MAX);
   invariant(h <= (int)bound);
}

static void height_verify(tree_t tree) {
   balancer_verify_height(tree, height_verify_node, NULL);
}

const balancer_t balancer_height = {
    .name = "Height",
    .restore = height_restore,
    .verify = height_verify,
};

// ─────────────────────────────────────────────────────────────────────────────
// Log
//
// -1 <= ⌊log₂(L)⌋ - ⌊log₂(R)⌋ <= 1
// ─────────────────────────────────────────────────────────────────────────────

static bool log_isBalanced(node_t *p, u64 s) {
   u64 sl = sizeL(p);
   u64 sr = sizeR(p, s);
   u64 wl = balancer_child_weight(sl);
   u64 wr = balancer_child_weight(sr);
   return util_gte_msb(wl, wr >> 1) && util_gte_msb(wr, wl >> 1);
}

static tree_t log_restore(tree_t tree) {
   return balancer_restore_by_partition(tree, 3, log_isBalanced);
}

static void log_verify_node(node_t *p, u64 s, void *ctx) {
   (void)ctx;
   u64 sl = sizeL(p);
   u64 sr = sizeR(p, s);

   int logL = (int)util_log2(balancer_child_weight(sl));
   int logR = (int)util_log2(balancer_child_weight(sr));
   invariant(int_distance_at_most(logL, logR, 1));
}

static void log_verify(tree_t tree) {
   balancer_verify_each_node(tree, log_verify_node, NULL);
}

const balancer_t balancer_log = {
    .name = "Log",
    .restore = log_restore,
    .verify = log_verify,
};

// ─────────────────────────────────────────────────────────────────────────────
// Weight
//
// (x + 1) >= (y + 1) >> 1  in both directions
// ─────────────────────────────────────────────────────────────────────────────

static bool weight_isBalanced(u64 x, u64 y) {
   __uint128_t wx = weight_u128_plus_one(x);
   __uint128_t wy = weight_u128_plus_one(y);
   return wx >= (wy >> 1) && wy >= (wx >> 1);
}

static bool weight_isBalancedNode(node_t *p, u64 s) {
   return weight_isBalanced(sizeL(p), sizeR(p, s));
}

static tree_t weight_restore(tree_t tree) {
   return balancer_restore_by_partition(tree, 3, weight_isBalancedNode);
}

static void weight_verify_node(node_t *p, u64 s, void *ctx) {
   (void)ctx;
   u64 sl = sizeL(p);
   u64 sr = sizeR(p, s);

   __uint128_t wl = weight_u128_plus_one(sl);
   __uint128_t wr = weight_u128_plus_one(sr);
   invariant(wl >= (wr >> 1));
   invariant(wr >= (wl >> 1));
}

static void weight_verify(tree_t tree) {
   balancer_verify_each_node(tree, weight_verify_node, NULL);
}

const balancer_t balancer_weight = {
    .name = "Weight",
    .restore = weight_restore,
    .verify = weight_verify,
};

// ─────────────────────────────────────────────────────────────────────────────
// Cost
// ─────────────────────────────────────────────────────────────────────────────

static bool cost_isBalanced(node_t *p, u64 s) {
   invariant(p != NULL);
   invariant(s > 0);
   if (s <= 2) {
      return true;
   }

   u64 sl = sizeL(p);
   u64 sr = sizeR(p, s);
   if (sl >= sr) {
      invariant(sl > 0);
      invariant(p->l != NULL);
      return sr >= sizeL(p->l) && sr >= sizeR(p->l, sl);
   } else {
      invariant(sr > 0);
      invariant(p->r != NULL);
      return sl >= sizeR(p->r, sr) && sl >= sizeL(p->r);
   }
}

static tree_t cost_restore(tree_t tree) {
   return balancer_restore_by_partition(tree, 2, cost_isBalanced);
}

static void cost_verify_node(node_t *p, u64 s, int heightL, int heightR,
                             void *ctx) {
   (void)ctx;
   u64 sl = sizeL(p);
   u64 sr = sizeR(p, s);

   invariant(p->l == NULL || sr >= sizeL(p->l));
   invariant(p->l == NULL || sr >= sizeR(p->l, sl));

   invariant(p->r == NULL || sl >= sizeL(p->r));
   invariant(p->r == NULL || sl >= sizeR(p->r, sr));

   int h = balancer_height_from_children(heightL, heightR);
   invariant(h <= util_avl_height_bound(s));
}

static void cost_verify(tree_t tree) {
   balancer_verify_height(tree, cost_verify_node, NULL);
}

const balancer_t balancer_cost = {
    .name = "Cost",
    .restore = cost_restore,
    .verify = cost_verify,
};

// ─────────────────────────────────────────────────────────────────────────────
// DSW (Day-Stout-Warren)
// ─────────────────────────────────────────────────────────────────────────────

static node_t *dsw_toVine(node_t *p) {
   node_t n = {0};
   node_t *l = &n;
   while (p != NULL) {
      invariant(!tree_shared(p));
      while (p->l != NULL) {
         invariant(!tree_shared(p->l));
         p = rotateR(p);
      }
      invariant(p->s == 0);
      if (l != &n) {
         invariant(!tree_shared(l));
      }
      l->r = p;
      l = l->r;
      p = p->r;
   }
   return n.r;
}

static node_t *dsw_compress(node_t *p, u64 k) {
   invariant(p != NULL);
   node_t n = {0};
   node_t *l = &n;
   n.r = p;
   for (; k > 0; k--) {
      invariant(p != NULL);
      invariant(p->r != NULL);
      invariant(!tree_shared(p));
      invariant(!tree_shared(p->r));
      if (l != &n) {
         invariant(!tree_shared(l));
      }
      l->r = rotateL(p);
      l = l->r;
      p = l->r;
   }
   invariant(n.r != NULL);
   return n.r;
}

static u64 dsw_complete_size_not_exceeding(u64 size) {
   invariant(size > 0);
   if (size == UINT64_MAX) {
      return UINT64_MAX;
   }
   u64 ordinal = size_after_inserting_one(size);
   u64 log = util_log2(ordinal);
   invariant(log < 64);
   return (1ULL << log) - 1;
}

#ifdef DEBUG
static u64 dsw_verify_vine(node_t *p) {
   u64 size = 0;
   while (p != NULL) {
      invariant(p->l == NULL);
      invariant(p->s == 0);
      size = size_after_inserting_one(size);
      p = p->r;
   }
   return size;
}
#endif

static node_t *dsw_toTree(node_t *vine, u64 size) {
   tree_root_size_require(vine, size);
   if (vine == NULL) {
      return NULL;
   }
#ifdef DEBUG
   assert_bst(dsw_verify_vine(vine) == size);
#endif
   u64 complete_size = dsw_complete_size_not_exceeding(size);
   u64 m = complete_size;
   node_t *p = dsw_compress(vine, size - m);
   while (m > 1) {
      m = m >> 1;
      p = dsw_compress(p, m);
   }
#ifdef DEBUG
   tree_debug_require_size(p, size);
#endif
   return p;
}

static tree_t dsw_restore(tree_t tree) {
   tree_sized_handle_require(tree);
   tree_debug_require_size(tree.root, tree.size);
#ifdef DEBUG
   balancer_order_snapshot_t original_order = balancer_order_snapshot(tree);
#endif
   tree.root = dsw_toVine(tree.root);
#ifdef DEBUG
   assert_bst(dsw_verify_vine(tree.root) == tree.size);
#endif
   tree.root = dsw_toTree(tree.root, tree.size);
   tree_sized_handle_require(tree);
   tree_debug_require_size(tree.root, tree.size);
#ifdef DEBUG
   balancer_require_same_order(&original_order, tree);
   balancer_order_snapshot_free(&original_order);
#endif
   return tree;
}

static void dsw_verify(tree_t tree) {
   tree_sized_handle_require(tree);
   node_path_metrics_t metrics =
       tree_verify_size_path_metrics(tree.root, tree.size);
   if (tree.root == NULL) {
      invariant(tree.size == 0);
      invariant(metrics.nodes == 0);
      return;
   }
   u64 expected_height = util_log2(tree.size);
   invariant(expected_height <= (u64)INT_MAX);
   invariant(metrics.maximum_depth == expected_height);
}

const balancer_t balancer_dsw = {
    .name = "DSW",
    .restore = dsw_restore,
    .verify = dsw_verify,
};
