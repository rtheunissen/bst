#include "bst.h"

#ifdef PERSISTENT
typedef enum {
   RELEASE_FRAME_VISIT,
   RELEASE_FRAME_FREE,
} release_frame_action_t;

typedef struct {
   node_t *node;
   release_frame_action_t action;
} release_frame_t;

enum {
   RELEASE_STACK_INLINE_CAP = 128,
};

static void release_stack_push(release_frame_t **stack,
                               release_frame_t *inline_stack, u64 *len,
                               u64 *cap, node_seen_t *seen, node_t *node,
                               release_frame_action_t action) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(len != NULL);
   invariant(cap != NULL);
   invariant(seen != NULL);
   invariant(*stack != NULL);
   invariant(*cap > 0);
   invariant(*len <= *cap);
   if (node == NULL) {
      return;
   }
   if (action == RELEASE_FRAME_VISIT) {
      node_seen_insert(seen, node);
   } else {
      invariant(action == RELEASE_FRAME_FREE);
   }

   if (*len == *cap) {
      *stack = (release_frame_t *)stack_array_grow(*stack, inline_stack, *len,
                                                   cap, sizeof(**stack));
   }
   invariant(*len < *cap);

   u64 index = *len;
   *len = size_after_inserting_one(*len);
   (*stack)[index] = (release_frame_t){.node = node, .action = action};
}

static void release_stack_dispose(release_frame_t *stack,
                                  release_frame_t *inline_stack, u64 len,
                                  u64 cap) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(cap > 0);
   invariant(len <= cap);
   invariant(stack == inline_stack || cap > RELEASE_STACK_INLINE_CAP);
   if (stack != inline_stack) {
      free(stack);
   }
}
#endif

#ifndef PERSISTENT
static void free_nodes_stack_push(node_stack_t *stack, node_t **inline_data,
                                  node_seen_t *seen, node_t *node) {
   invariant(seen != NULL);
   if (node == NULL) {
      return;
   }
   node_seen_insert(seen, node);
   node_stack_push(stack, inline_data, node);
}
#endif

void tree_release(node_t *p) {
   if (p == NULL) {
      return;
   }
#ifdef PERSISTENT
   release_frame_t inline_stack[RELEASE_STACK_INLINE_CAP];
   release_frame_t *stack = inline_stack;
   u64 len = 0;
   u64 cap = RELEASE_STACK_INLINE_CAP;
   node_seen_t seen = {0};

   release_stack_push(&stack, inline_stack, &len, &cap, &seen, p,
                      RELEASE_FRAME_VISIT);
   while (len > 0) {
      len = size_after_removing_one(len);
      release_frame_t frame = stack[len];
      node_t *node = frame.node;

      if (frame.action == RELEASE_FRAME_FREE) {
         if (!arena_contains_live(node)) {
            free(node);
         }
         continue;
      }
      invariant(frame.action == RELEASE_FRAME_VISIT);

      if (node->refs > 0) {
         node->refs = size_after_removing_one(node->refs);
         continue;
      }

      release_stack_push(&stack, inline_stack, &len, &cap, &seen, node,
                         RELEASE_FRAME_FREE);
      release_stack_push(&stack, inline_stack, &len, &cap, &seen, node->r,
                         RELEASE_FRAME_VISIT);
      release_stack_push(&stack, inline_stack, &len, &cap, &seen, node->l,
                         RELEASE_FRAME_VISIT);
   }

   release_stack_dispose(stack, inline_stack, len, cap);
   node_seen_dispose(&seen);
#else
   if (!arena_contains_live(p)) {
      free(p);
   }
#endif
}

void free_nodes(node_t *p) {
#ifdef PERSISTENT
   tree_release(p);
#else
   node_t *inline_data[NODE_STACK_INLINE_CAP];
   node_stack_t stack = {
       .data = inline_data,
       .len = 0,
       .cap = NODE_STACK_INLINE_CAP,
   };
   node_seen_t seen = {0};

   free_nodes_stack_push(&stack, inline_data, &seen, p);
   while (stack.len > 0) {
      node_t *node = node_stack_pop(&stack);
      free_nodes_stack_push(&stack, inline_data, &seen, node->l);
      free_nodes_stack_push(&stack, inline_data, &seen, node->r);
      tree_release(node);
   }

   node_stack_free(&stack, inline_data);
   node_seen_dispose(&seen);
#endif
}
