// Comparison program: outputs deterministic operation results for each tree.
//
// For each tree, inserts N elements, then selects all, then deletes all.
// Outputs the sequence of values for comparison across implementations/runs.
//
#include "bst.h"
#include "list.h"
#include "random.h"
#include "splay.h"
#include "randomized.h"
#include "zip.h"
#include "treap_bottomup.h"
#include "treap_topdown.h"
#include "conc.h"
#include "llrb.h"
#include "aa.h"
#include "avl_bottomup.h"
#include "avl_topdown.h"
#include "avl_weak_bottomup.h"
#include "avl_weak_topdown.h"
#include "avl_relaxed_bottomup.h"
#include "avl_relaxed_topdown.h"
#include "redblack_bottomup.h"
#include "redblack_topdown.h"
#include "redblack_relaxed_bottomup.h"
#include "redblack_relaxed_topdown.h"
#include "wbst_bottomup.h"
#include "wbst_topdown.h"
#include "wbst_relaxed.h"
#include "lbst_bottomup.h"
#include "lbst_topdown.h"
#include "lbst_relaxed.h"
#include "treap_fingertree.h"
#include "distributions.h"

#include <stdio.h>

static const list_vtable_t *all_trees[] = {
   &avl_bottomup_vtable,
   &avl_topdown_vtable,
   &avl_weak_topdown_vtable,
   &avl_weak_bottomup_vtable,
   &avl_relaxed_topdown_vtable,
   &avl_relaxed_bottomup_vtable,
   &redblack_bottomup_vtable,
   &redblack_topdown_vtable,
   &redblack_relaxed_bottomup_vtable,
   &redblack_relaxed_topdown_vtable,
   &wbst_bottomup_vtable,
   &wbst_topdown_vtable,
   &wbst_relaxed_vtable,
   &lbst_bottomup_vtable,
   &lbst_topdown_vtable,
   &lbst_relaxed_vtable,
   &treap_bottomup_vtable,
   &treap_topdown_vtable,
   &treap_fingertree_vtable,
   &randomized_vtable,
   &zip_vtable,
   &splay_vtable,
   &conc_vtable,
   &llrb_vtable,
   &aa_vtable,
};
static const int num_trees = sizeof(all_trees) / sizeof(all_trees[0]);

typedef struct {
   u64 *data;
   u64 len;
} collect_t;

static void collect_visit(u64 x, void *ctx) {
   collect_t *c = (collect_t *)ctx;
   c->data[c->len++] = x;
}

int main(void) {
   random_seed(1);
   u64 N = 200;

   for (int t = 0; t < num_trees; t++) {
      printf("=== %s ===\n", all_trees[t]->name);

      // Use a fresh distribution seeded from the global PRNG for each tree
      distribution_t *dist = uniform_new(random_uint64());

      list_t tree = list_new(all_trees[t]);

      // Insert N elements
      for (u64 i = 0; i < N; i++) {
         u64 pos = dist_less_than(dist, list_size(&tree) + 1);
         u64 val = random_uint64();
         list_insert(&tree, pos, val);
      }

      // Collect all elements via Each
      collect_t col = {.data = malloc(N * sizeof(u64)), .len = 0};
      list_each(&tree, collect_visit, &col);
      printf("size=%llu\n", (unsigned long long)list_size(&tree));
      printf("each:");
      for (u64 i = 0; i < col.len; i++) {
         printf(" %llu", (unsigned long long)col.data[i]);
      }
      printf("\n");

      // Delete all elements, recording deleted values
      printf("delete:");
      while (list_size(&tree) > 0) {
         u64 pos = dist_less_than(dist, list_size(&tree));
         u64 val = list_delete(&tree, pos);
         printf(" %llu", (unsigned long long)val);
      }
      printf("\n\n");

      free(col.data);
      list_free(&tree);
      dist_destroy(dist);
   }

   return 0;
}
