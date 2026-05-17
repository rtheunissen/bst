// Quick benchmark: InsertDelete for all 25 trees, 1 iteration, Uniform distribution.
// Outputs one line per tree: name, total_ns, ops_per_sec.
//
// Usage:
//   ./quickbench_bin [-scale N] [-steps N] [-arena N]
#include "bst.h"
#include "list.h"
#include "random.h"
#include "distributions.h"
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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

static u64 time_ns(void) {
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (u64)ts.tv_sec * 1000000000ULL + (u64)ts.tv_nsec;
}

static u64 parse_u64(const char *value, const char *name) {
   char *end = NULL;
   unsigned long long parsed = strtoull(value, &end, 10);
   if (!value[0] || (end && *end)) {
      fprintf(stderr, "invalid %s: %s\n", name, value);
      exit(2);
   }
   return (u64)parsed;
}

static const list_vtable_t *trees[] = {
   &avl_bottomup_vtable, &avl_topdown_vtable,
   &avl_weak_topdown_vtable, &avl_weak_bottomup_vtable,
   &avl_relaxed_topdown_vtable, &avl_relaxed_bottomup_vtable,
   &redblack_bottomup_vtable, &redblack_topdown_vtable,
   &redblack_relaxed_bottomup_vtable, &redblack_relaxed_topdown_vtable,
   &wbst_bottomup_vtable, &wbst_topdown_vtable, &wbst_relaxed_vtable,
   &lbst_bottomup_vtable, &lbst_topdown_vtable, &lbst_relaxed_vtable,
   &treap_bottomup_vtable, &treap_topdown_vtable, &treap_fingertree_vtable,
   &randomized_vtable, &zip_vtable, &splay_vtable,
   &conc_vtable, &llrb_vtable, &aa_vtable,
};

int main(int argc, char **argv) {
   random_seed(1);
   int ntrees = sizeof(trees) / sizeof(trees[0]);
   u64 scale = 1000000;
   u64 steps = 2000000;
   u64 arena_cap = 30000000;

   for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-scale") == 0 && i + 1 < argc) {
         scale = parse_u64(argv[++i], "-scale");
      } else if (strcmp(argv[i], "-steps") == 0 && i + 1 < argc) {
         steps = parse_u64(argv[++i], "-steps");
      } else if (strcmp(argv[i], "-arena") == 0 && i + 1 < argc) {
         arena_cap = parse_u64(argv[++i], "-arena");
      } else {
         fprintf(stderr, "unknown or incomplete argument: %s\n", argv[i]);
         return 2;
      }
   }

   if (scale < 1) scale = 1;
   if (steps < 1) steps = 1;
   if (arena_cap < 1) arena_cap = 1;

   arena_t arena;
   arena_init(&arena, arena_cap);
   global_arena = &arena;

   printf("%-28s %12s %12s\n", "Tree", "Time(ms)", "Mops/s");
   printf("%-28s %12s %12s\n", "----", "-------", "------");

   for (int t = 0; t < ntrees; t++) {
      arena_reset(&arena);
      distribution_t *dist = uniform_new(2);
      list_t instance = list_new(trees[t]);
      u64 step_count = 0;

      u64 start = time_ns();
      for (u64 i = 0; i < steps; i++) {
         step_count++;
         if (list_size(&instance) < scale || step_count % 2 == 0) {
            u64 j = dist_less_than(dist, list_size(&instance) + 1);
            list_insert(&instance, j, 0);
         } else {
            u64 j = dist_less_than(dist, list_size(&instance));
            list_delete(&instance, list_size(&instance) - j - 1);
         }
      }
      u64 elapsed = time_ns() - start;

      double ms = (double)elapsed / 1e6;
      double mops = (double)steps / ((double)elapsed / 1e9) / 1e6;
      printf("%-28s %10.1f %10.2f\n", trees[t]->name, ms, mops);

      dist_destroy(dist);
   }

   global_arena = NULL;
   arena_destroy(&arena);
   return 0;
}
