#include "rank.h"

static int avl_weak_rank_bound(u64 size) {
   invariant(size > 0);
   u64 log_size = util_log2(size);
   invariant(log_size <= (u64)(INT_MAX / 2));
   return rank_twice((int)log_size);
}

static void tree_verify_each_node_sized(node_t *root, u64 size,
                                        tree_verify_node_fn verify, void *ctx) {
   invariant(verify != NULL);
   invariant(tree_verify_size_each_node(root, size, verify, ctx) == size);
}

void tree_verify_each_node(node_t *root, tree_verify_node_fn verify,
                           void *ctx) {
   invariant(verify != NULL);
   u64 verified = tree_verify_reachable_size_each_node(root, verify, ctx);
   tree_root_size_require(root, verified);
}

typedef void (*rank_height_verify_node_fn)(node_t *node, u64 size, int height,
                                           void *ctx);

static int verify_rank_height_sized(node_t *root, u64 size,
                                    rank_height_verify_node_fn verify,
                                    void *ctx);
static int verify_rank_height_walk(node_t *p, u64 size,
                                   rank_height_verify_node_fn verify,
                                   void *ctx);
static int verify_rank_height_reachable(node_t *p,
                                        rank_height_verify_node_fn verify,
                                        void *ctx);

typedef struct {
   node_rank_fn rank;
} max_rank_heap_verify_ctx_t;

static void verify_max_rank_heap_node(node_t *p, void *ctx) {
   max_rank_heap_verify_ctx_t *verify = (max_rank_heap_verify_ctx_t *)ctx;
   invariant(verify != NULL);
   invariant(verify->rank(p) >= verify->rank(p->l));
   invariant(verify->rank(p) >= verify->rank(p->r));
}

void verifyMaxRankHeap(node_t *root, node_rank_fn rank) {
   invariant(rank != NULL);
   max_rank_heap_verify_ctx_t ctx = {.rank = rank};
   tree_verify_each_node(root, verify_max_rank_heap_node, &ctx);
}

void verifyMaxRankHeapSized(node_t *root, u64 size, node_rank_fn rank) {
   invariant(rank != NULL);
   max_rank_heap_verify_ctx_t ctx = {.rank = rank};
   tree_verify_each_node_sized(root, size, verify_max_rank_heap_node, &ctx);
}

static void verify_redblack_rank_node(node_t *p, void *ctx) {
   (void)ctx;
   if (isLeaf(p)) {
      invariant(rank_of(p) == 0);
   }
   invariant(rank_of(p) >= rank_of(p->l));
   invariant(rank_of(p) >= rank_of(p->r));

   // All rank differences are 0 or 1.
   invariant(isZeroChild(p, p->l) || isOneChild(p, p->l));
   invariant(isZeroChild(p, p->r) || isOneChild(p, p->r));

   // No parent of a 0-child is a 0-child.
   invariant(!isZeroChild(p, p->l) || !isZeroChild(p->l, p->l->l));
   invariant(!isZeroChild(p, p->l) || !isZeroChild(p->l, p->l->r));
   invariant(!isZeroChild(p, p->r) || !isZeroChild(p->r, p->r->l));
   invariant(!isZeroChild(p, p->r) || !isZeroChild(p->r, p->r->r));
}

void verifyRedBlackRanks(node_t *p) {
   tree_verify_each_node(p, verify_redblack_rank_node, NULL);
}

void verifyRedBlackRanksSized(node_t *p, u64 size) {
   tree_verify_each_node_sized(p, size, verify_redblack_rank_node, NULL);
}

static void verify_redblack_height_node(node_t *node, u64 size, int h,
                                        void *ctx) {
   (void)ctx;
   invariant(h <= rank_twice_plus_one(rank_of(node)));
   invariant(h <= util_redblack_height_bound(size));
}

int verifyRedBlackHeight(node_t *root, u64 size) {
   return verify_rank_height_sized(root, size, verify_redblack_height_node,
                                   NULL);
}

static void verify_avl_rank_node(node_t *node, u64 size, int h, void *ctx) {
   (void)ctx;
   // AVL rule: every node is 1,1 or 1,2.
   invariant(isOneChild(node, node->l) || isTwoChild(node, node->l));
   invariant(isOneChild(node, node->r) || isTwoChild(node, node->r));
   invariant(isOneChild(node, node->l) || isOneChild(node, node->r));

   invariant(rank_of(node) == h);
   invariant(rank_of(node) <= util_avl_height_bound(size));
}

int verifyAVLRanks(node_t *root, u64 size) {
   return verify_rank_height_sized(root, size, verify_avl_rank_node, NULL);
}

static void verify_avl_weak_rank_node(node_t *p, void *ctx) {
   (void)ctx;
   if (isLeaf(p)) {
      invariant(rank_of(p) == 0);
   }
   invariant(rank_of(p) > rank_of(p->l));
   invariant(rank_of(p) > rank_of(p->r));
   invariant(isOneChild(p, p->l) || isTwoChild(p, p->l));
   invariant(isOneChild(p, p->r) || isTwoChild(p, p->r));
}

void verifyAVLWeakRanks(node_t *p) {
   tree_verify_each_node(p, verify_avl_weak_rank_node, NULL);
}

void verifyAVLWeakRanksSized(node_t *p, u64 size) {
   tree_verify_each_node_sized(p, size, verify_avl_weak_rank_node, NULL);
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
} rank_height_verify_frame_t;

enum {
   RANK_HEIGHT_VERIFY_STACK_INLINE_CAP = 128,
};

static void
rank_height_verify_stack_push(rank_height_verify_frame_t **stack,
                              rank_height_verify_frame_t *inline_stack,
                              u64 *len, u64 *cap, node_t *node, u64 size,
                              bool size_is_known, bool is_right_child) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(len != NULL);
   invariant(cap != NULL);
   invariant(*stack != NULL);
   invariant(*cap > 0);
   invariant(*len <= *cap);
   if (node == NULL) {
      if (size_is_known) {
         invariant(size == 0);
      }
      return;
   }
   invariant(size_is_known ? size > 0 : size == 0);

   if (*len == *cap) {
      *stack = (rank_height_verify_frame_t *)stack_array_grow(
          *stack, inline_stack, *len, cap, sizeof(**stack));
   }

   invariant(*len < *cap);
   u64 index = *len;
   *len = size_after_inserting_one(*len);
   (*stack)[index] = (rank_height_verify_frame_t){
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

static rank_height_verify_frame_t *
rank_height_verify_stack_top(rank_height_verify_frame_t *stack, u64 len) {
   invariant(stack != NULL);
   invariant(len > 0);
   return &stack[rank_of_last(len)];
}

static void
rank_height_verify_stack_dispose(rank_height_verify_frame_t *stack,
                                 rank_height_verify_frame_t *inline_stack,
                                 u64 len, u64 cap) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(cap > 0);
   invariant(len <= cap);
   invariant(stack == inline_stack ||
             cap > RANK_HEIGHT_VERIFY_STACK_INLINE_CAP);
   if (stack != inline_stack) {
      free(stack);
   }
}

static int rank_height_from_children(int left_height, int right_height) {
   invariant(left_height >= -1);
   invariant(right_height >= -1);
   int child_height = MAX(left_height, right_height);
   invariant(child_height < INT_MAX);
   return child_height < 0 ? 0
                           : (int)size_after_inserting_one((u64)child_height);
}

static void
rank_height_verify_set_child_height(rank_height_verify_frame_t *parent,
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
rank_height_verify_set_child_size(rank_height_verify_frame_t *parent,
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

static void rank_height_verify_finish_frame(rank_height_verify_frame_t *stack,
                                            u64 *len, bool is_right_child,
                                            int height, u64 size,
                                            int *root_height, u64 *root_size) {
   invariant(stack != NULL);
   invariant(len != NULL);
   invariant(*len > 0);
   invariant(height >= 0);
   invariant(size > 0);
   invariant(root_height != NULL);
   invariant(root_size != NULL);

   *len = size_after_removing_one(*len);
   if (*len == 0) {
      *root_height = height;
      *root_size = size;
   } else {
      rank_height_verify_frame_t *parent = &stack[rank_of_last(*len)];
      rank_height_verify_set_child_height(parent, is_right_child, height);
      rank_height_verify_set_child_size(parent, is_right_child, size);
   }
}

static int verify_rank_height_walk(node_t *p, u64 size,
                                   rank_height_verify_node_fn verify,
                                   void *ctx) {
   tree_sized_root_require(p, size);
   invariant(verify != NULL);
   if (p == NULL) {
      return -1;
   }

   rank_height_verify_frame_t inline_stack[RANK_HEIGHT_VERIFY_STACK_INLINE_CAP];
   rank_height_verify_frame_t *stack = inline_stack;
   u64 len = 0;
   u64 cap = RANK_HEIGHT_VERIFY_STACK_INLINE_CAP;
   u64 visited = 0;
   u64 root_size = 0;
   int root_height = -1;
   node_seen_t seen = {0};

   rank_height_verify_stack_push(&stack, inline_stack, &len, &cap, p, size,
                                 true, false);
   while (len > 0) {
      rank_height_verify_frame_t *frame =
          rank_height_verify_stack_top(stack, len);

      if (frame->state == 0) {
         frame->state = 1;
         node_seen_insert(&seen, frame->node);
         invariant(sizeL(frame->node) < frame->size);
         rank_height_verify_stack_push(&stack, inline_stack, &len, &cap,
                                       frame->node->l, sizeL(frame->node), true,
                                       false);
         continue;
      }

      if (frame->state == 1) {
         frame->state = 2;
         rank_height_verify_stack_push(
             &stack, inline_stack, &len, &cap, frame->node->r,
             sizeR(frame->node, frame->size), true, true);
         continue;
      }

      node_t *node = frame->node;
      invariant((node->l == NULL) == (frame->left_height == -1));
      invariant((node->r == NULL) == (frame->right_height == -1));
      invariant((node->l == NULL) == (frame->left_size == 0));
      invariant((node->r == NULL) == (frame->right_size == 0));
      invariant(frame->left_size == sizeL(node));
      invariant(frame->right_size == sizeR(node, frame->size));
      int h =
          rank_height_from_children(frame->left_height, frame->right_height);
      verify(node, frame->size, h, ctx);
      visited = size_after_inserting_one(visited);

      rank_height_verify_finish_frame(stack, &len, frame->is_right_child, h,
                                      frame->size, &root_height, &root_size);
   }

   rank_height_verify_stack_dispose(stack, inline_stack, len, cap);
   invariant(visited == size);
   invariant(root_size == size);
   invariant(root_height >= 0);
   node_seen_dispose(&seen);
   return root_height;
}

static int verify_rank_height_reachable(node_t *p,
                                        rank_height_verify_node_fn verify,
                                        void *ctx) {
   invariant(verify != NULL);
   if (p == NULL) {
      return -1;
   }

   rank_height_verify_frame_t inline_stack[RANK_HEIGHT_VERIFY_STACK_INLINE_CAP];
   rank_height_verify_frame_t *stack = inline_stack;
   u64 len = 0;
   u64 cap = RANK_HEIGHT_VERIFY_STACK_INLINE_CAP;
   u64 visited = 0;
   u64 root_size = 0;
   int root_height = -1;
   node_seen_t seen = {0};

   rank_height_verify_stack_push(&stack, inline_stack, &len, &cap, p, 0, false,
                                 false);
   while (len > 0) {
      rank_height_verify_frame_t *frame =
          rank_height_verify_stack_top(stack, len);

      if (frame->state == 0) {
         frame->state = 1;
         node_seen_insert(&seen, frame->node);
         visited = size_after_inserting_one(visited);
         rank_height_verify_stack_push(&stack, inline_stack, &len, &cap,
                                       frame->node->l, 0, false, false);
         continue;
      }

      if (frame->state == 1) {
         frame->state = 2;
         rank_height_verify_stack_push(&stack, inline_stack, &len, &cap,
                                       frame->node->r, 0, false, true);
         continue;
      }

      node_t *node = frame->node;
      invariant((node->l == NULL) == (frame->left_height == -1));
      invariant((node->r == NULL) == (frame->right_height == -1));
      invariant((node->l == NULL) == (frame->left_size == 0));
      invariant((node->r == NULL) == (frame->right_size == 0));
      invariant(sizeL(node) == frame->left_size);
      u64 size =
          tree_size_from_child_sizes(frame->left_size, frame->right_size);
      int h =
          rank_height_from_children(frame->left_height, frame->right_height);
      verify(node, size, h, ctx);

      rank_height_verify_finish_frame(stack, &len, frame->is_right_child, h,
                                      size, &root_height, &root_size);
   }

   rank_height_verify_stack_dispose(stack, inline_stack, len, cap);
   invariant(root_size == visited);
   invariant(root_height >= 0);
   node_seen_dispose(&seen);
   return root_height;
}

static void verify_rank_height_result(node_t *root, int height) {
   invariant((root == NULL) == (height == -1));
}

static void verify_rank_height_sized_result(node_t *root, u64 size,
                                            int height) {
   tree_sized_root_require(root, size);
   verify_rank_height_result(root, height);
}

static int verify_rank_height_sized(node_t *root, u64 size,
                                    rank_height_verify_node_fn verify,
                                    void *ctx) {
   int height = verify_rank_height_walk(root, size, verify, ctx);
   verify_rank_height_sized_result(root, size, height);
   return height;
}

static void verify_avl_weak_height_node(node_t *node, u64 size, int h,
                                        void *ctx) {
   (void)ctx;
   invariant(rank_of(node) >= h);
   invariant(rank_of(node) <= rank_twice(h));
   invariant(rank_of(node) <= avl_weak_rank_bound(size));
}

void verifyAVLWeakHeight(node_t *root, u64 size) {
   (void)verify_rank_height_sized(root, size, verify_avl_weak_height_node,
                                  NULL);
}

static void verify_avl_relaxed_height_node(node_t *node, u64 size, int h,
                                           void *ctx) {
   (void)size;
   (void)ctx;
   invariant(rank_of(node) >= h);
   invariant(rank_of(node) > rank_of(node->l));
   invariant(rank_of(node) > rank_of(node->r));
}

void verifyAVLRelaxedRanks(node_t *p) {
   int height =
       verify_rank_height_reachable(p, verify_avl_relaxed_height_node, NULL);
   verify_rank_height_result(p, height);
}

void verifyAVLRelaxedRanksSized(node_t *p, u64 size) {
   (void)verify_rank_height_sized(p, size, verify_avl_relaxed_height_node,
                                  NULL);
}

static void verify_redblack_relaxed_rank_node(node_t *p, void *ctx) {
   (void)ctx;
   invariant(rank_of(p) >= rank_of(p->l));
   invariant(rank_of(p) >= rank_of(p->r));
   invariant(!isZeroChild(p, p->l) || !isZeroChild(p->l, p->l->l));
   invariant(!isZeroChild(p, p->l) || !isZeroChild(p->l, p->l->r));
   invariant(!isZeroChild(p, p->r) || !isZeroChild(p->r, p->r->l));
   invariant(!isZeroChild(p, p->r) || !isZeroChild(p->r, p->r->r));
}

void verifyRedBlackRelaxedRanks(node_t *p) {
   tree_verify_each_node(p, verify_redblack_relaxed_rank_node, NULL);
}

void verifyRedBlackRelaxedRanksSized(node_t *p, u64 size) {
   tree_verify_each_node_sized(p, size, verify_redblack_relaxed_rank_node,
                               NULL);
}

static void verify_redblack_relaxed_height_node(node_t *node, u64 size, int h,
                                                void *ctx) {
   (void)size;
   (void)ctx;
   invariant(h <= rank_twice_plus_one(rank_of(node)));
}

static void verify_redblack_relaxed_node(node_t *node, u64 size, int h,
                                         void *ctx) {
   verify_redblack_relaxed_rank_node(node, ctx);
   verify_redblack_relaxed_height_node(node, size, h, ctx);
}

void verifyRedBlackRelaxedHeight(node_t *p) {
   int height = verify_rank_height_reachable(
       p, verify_redblack_relaxed_height_node, NULL);
   verify_rank_height_result(p, height);
}

void verifyRedBlackRelaxedHeightSized(node_t *p, u64 size) {
   (void)verify_rank_height_sized(p, size, verify_redblack_relaxed_height_node,
                                  NULL);
}

void verifyRedBlackRelaxed(node_t *p) {
   int height =
       verify_rank_height_reachable(p, verify_redblack_relaxed_node, NULL);
   verify_rank_height_result(p, height);
}

void verifyRedBlackRelaxedSized(node_t *p, u64 size) {
   (void)verify_rank_height_sized(p, size, verify_redblack_relaxed_node, NULL);
}
