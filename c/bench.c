// Benchmark harness for BST operations.
//
// Matches the Go benchmark structure:
//   - For each tree × distribution × iteration:
//     - Create a new tree
//     - Perform `steps` operations in chunks of `step`
//     - Time each chunk, output to CSV
//
// Usage:
//   ./bench_bin [-operation Insert|InsertDelete]
//               [-scale N] [-steps N] [-samples N] [-iterations N]
//               [-arena N] [-output-dir PATH]
//
// Persistent operations are available in builds compiled with -DPERSISTENT
// (for example, the bench-persistent Make target).
//
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
#include "filesystem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ─────────────────────────────────────────────────────────────────────────────
// High-resolution timing
// ─────────────────────────────────────────────────────────────────────────────

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

static int parse_int(const char *value, const char *name) {
   u64 parsed = parse_u64(value, name);
   if (parsed > 2147483647ULL) {
      fprintf(stderr, "%s is too large: %s\n", name, value);
      exit(2);
   }
   return (int)parsed;
}


// ─────────────────────────────────────────────────────────────────────────────
// Operation types (matching Go's list.Operation interface)
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
#ifdef PERSISTENT
   OP_INSERT_PERSISTENT,
   OP_INSERT_DELETE_PERSISTENT,
#else
   OP_INSERT,
   OP_INSERT_DELETE,
#endif
} op_type_t;

typedef struct {
   op_type_t type;
   const char *name;
   u64 scale;    // max tree size (Insert) or steady-state size (InsertDelete)
   u64 steps;    // total number of operations
} operation_t;

// Perform one step of the operation. Returns the (possibly new) instance.
static list_t op_update(operation_t *op, list_t instance, distribution_t *dist, u64 *step_count) {
   (*step_count)++;

   switch (op->type) {
#ifdef PERSISTENT
   case OP_INSERT_PERSISTENT: {
      list_t clone = list_clone(&instance);
      u64 i = dist_less_than(dist, list_size(&clone) + 1);
      list_insert(&clone, i, 0);
      list_free(&instance);
      return clone;
   }
   case OP_INSERT_DELETE_PERSISTENT: {
      list_t clone = list_clone(&instance);
      if (list_size(&clone) < op->scale || *step_count % 2 == 0) {
         u64 i = dist_less_than(dist, list_size(&clone) + 1);
         list_insert(&clone, i, 0);
      } else {
         u64 i = dist_less_than(dist, list_size(&clone));
         list_delete(&clone, list_size(&clone) - i - 1);
      }
      list_free(&instance);
      return clone;
   }
#else
   case OP_INSERT: {
      u64 i = dist_less_than(dist, list_size(&instance) + 1);
      list_insert(&instance, i, 0);
      return instance;
   }
   case OP_INSERT_DELETE: {
      if (list_size(&instance) < op->scale || *step_count % 2 == 0) {
         u64 i = dist_less_than(dist, list_size(&instance) + 1);
         list_insert(&instance, i, 0);
      } else {
         u64 i = dist_less_than(dist, list_size(&instance));
         list_delete(&instance, list_size(&instance) - i - 1);
      }
      return instance;
   }
#endif
   }
   return instance; // unreachable
}


// ─────────────────────────────────────────────────────────────────────────────
// Benchmark runner
// ─────────────────────────────────────────────────────────────────────────────

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

static const char *bench_dist_names[] = {
   "Uniform", "Normal", "Skewed", "Zipf", "Maximum",
};
static const dist_factory_fn bench_dist_factories[] = {
   uniform_new, normal_new, skewed_new, zipf_new, maximum_new,
};
static const int num_bench_dists = 5;

static void run_benchmark(operation_t *op, int iterations, u64 samples, u64 arena_cap, const char *output_dir) {
   arena_t arena;
   arena_init(&arena, arena_cap);
   global_arena = &arena;

   u64 steps = op->steps;
   if (steps == 0) steps = 1;
   if (samples == 0) samples = 1;
   if (samples > steps) samples = steps;
   u64 step = steps / samples;

   for (int t = 0; t < num_trees; t++) {
      // Create output directory and file
      char path[512];
      path_format(path, sizeof(path),
         "%s/operations/benchmarks/%s/%s",
         output_dir, op->name, all_trees[t]->name);

      // Ensure directory exists
      char dir[512];
      path_format(dir, sizeof(dir),
         "%s/operations/benchmarks/%s", output_dir, op->name);
      mkdir_p(dir);

      FILE *file = fopen(path, "w");
      if (!file) { perror(path); continue; }

      // Header
      fprintf(file, "Distribution Scale Size Step Position Iteration Duration\n");

      for (int iteration = 1; iteration <= iterations; iteration++) {
         for (int d = 0; d < num_bench_dists; d++) {
            printf("%s %-32s %-24s %-12s %d/%d\n",
               "", op->name, all_trees[t]->name,
               bench_dist_names[d], iteration, iterations);
            fflush(stdout);

            distribution_t *dist = bench_dist_factories[d]((u64)(iteration + 1));

            // Reset arena for this run
            arena_reset(&arena);

            list_t instance = list_new(all_trees[t]);
            u64 step_count = 0;

            for (u64 position = step; position <= steps; position += step) {
               u64 start = time_ns();
               for (u64 i = 0; i < step; i++) {
                  instance = op_update(op, instance, dist, &step_count);
               }
               u64 duration = time_ns() - start;

               fprintf(file, "%s %llu %llu %llu %llu %d %llu\n",
                  bench_dist_names[d],
                  (unsigned long long)steps,
                  (unsigned long long)list_size(&instance),
                  (unsigned long long)step,
                  (unsigned long long)position,
                  iteration,
                  (unsigned long long)duration);
            }

            // Don't free individual nodes — arena_reset handles bulk cleanup
            dist_destroy(dist);
         }
      }
      fclose(file);
   }

   global_arena = NULL;
   arena_destroy(&arena);
}


// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char **argv) {
   random_seed(1);

   const char *op_name = NULL;
   u64 scale_override = 0;
   u64 steps_override = 0;
   u64 samples = 1000;
   u64 arena_cap = 50000000;
   int iterations = 10;
   const char *output_dir = "docs/benchmarks/data";

   for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-operation") == 0 && i + 1 < argc) {
         op_name = argv[++i];
      } else if (strcmp(argv[i], "-scale") == 0 && i + 1 < argc) {
         scale_override = parse_u64(argv[++i], "-scale");
      } else if (strcmp(argv[i], "-steps") == 0 && i + 1 < argc) {
         steps_override = parse_u64(argv[++i], "-steps");
      } else if (strcmp(argv[i], "-samples") == 0 && i + 1 < argc) {
         samples = parse_u64(argv[++i], "-samples");
      } else if (strcmp(argv[i], "-iterations") == 0 && i + 1 < argc) {
         iterations = parse_int(argv[++i], "-iterations");
      } else if (strcmp(argv[i], "-arena") == 0 && i + 1 < argc) {
         arena_cap = parse_u64(argv[++i], "-arena");
      } else if (strcmp(argv[i], "-output-dir") == 0 && i + 1 < argc) {
         output_dir = argv[++i];
      } else {
         fprintf(stderr, "unknown or incomplete argument: %s\n", argv[i]);
         return 2;
      }
   }

   operation_t operations[] = {
#ifdef PERSISTENT
      {OP_INSERT_PERSISTENT,       "InsertPersistent",       10000000, 10000000},
      {OP_INSERT_DELETE_PERSISTENT,"InsertDeletePersistent",  1000000, 10000000},
#else
      {OP_INSERT,                 "Insert",                 10000000, 10000000},
      {OP_INSERT_DELETE,           "InsertDelete",            1000000, 10000000},
#endif
   };
   int num_ops = sizeof(operations) / sizeof(operations[0]);

   if (iterations < 1) iterations = 1;
   if (arena_cap < 1) arena_cap = 1;

   bool ran = false;
   for (int i = 0; i < num_ops; i++) {
      if (op_name && strcmp(op_name, operations[i].name) != 0) {
         continue;
      }
      if (scale_override) operations[i].scale = scale_override;
      if (steps_override) operations[i].steps = steps_override;
      run_benchmark(&operations[i], iterations, samples, arena_cap, output_dir);
      ran = true;
   }

   if (!ran && op_name) {
      fprintf(stderr, "unknown operation for this build: %s\n", op_name);
      return 2;
   }

   return 0;
}
