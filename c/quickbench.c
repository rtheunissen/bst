// Quick benchmark: InsertDelete for selected registered trees, 1 iteration.
// Outputs one line per tree: name, elapsed milliseconds, million ops/sec.
//
// Usage:
//   ./quickbench_bin [-scale N] [-steps N] [-arena N] [-tree NAME] [-dist NAME]
#include "bst.h"
#include "cli.h"
#include "distributions.h"
#include "list.h"
#include "operations.h"
#include "operation_runs.h"
#include "random.h"
#include "time_utils.h"
#include "tree_registry.h"

#include <stdio.h>
#include <string.h>

static void quickbench_require_tree(const list_vtable_t *vt) {
   list_vtable_require(vt);
}

static void quickbench_require_distribution(const dist_spec_t *spec) {
   dist_spec_require(spec);
}

static operation_t quickbench_insert_delete_operation(u64 scale, u64 steps) {
   invariant(scale > 0);
   invariant(steps > 0);

   operation_t op = operation_default_mixed();
   op.scale = scale;
   op.steps = steps;
   operation_require(&op);
   return op;
}

static u64 quickbench_workload_seed(const operation_t *op,
                                    const list_vtable_t *tree_vt,
                                    const dist_spec_t *dist_spec) {
   operation_require(op);
   list_vtable_require(tree_vt);
   dist_spec_require(dist_spec);
   operation_run_t run = {
       .op = op,
       .tree_vt = tree_vt,
       .dist = dist_spec,
       .scale = op->scale,
       .steps = op->steps,
       .iteration = 1,
       .iterations = 1,
   };
   return operation_run_workload_seed(&run, "quickbench");
}

int main(int argc, char **argv) {
   random_seed(1);
   u64 scale = 1000000;
   u64 steps = 2000000;
   u64 arena_cap = 30000000;
   const char *tree_name = NULL;
   const char *dist_name = "Uniform";

   for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-scale") == 0) {
         scale = cli_parse_positive_u64(cli_arg_value(argc, argv, &i, "-scale"),
                                        "-scale");
      } else if (strcmp(argv[i], "-steps") == 0) {
         steps = cli_parse_positive_step_count(
             cli_arg_value(argc, argv, &i, "-steps"), "-steps");
      } else if (strcmp(argv[i], "-arena") == 0) {
         arena_cap = cli_parse_positive_arena_capacity(
             cli_arg_value(argc, argv, &i, "-arena"), "-arena");
      } else if (strcmp(argv[i], "-tree") == 0) {
         tree_name = cli_parse_nonempty_string(
             cli_arg_value(argc, argv, &i, "-tree"), "-tree");
      } else if ((strcmp(argv[i], "-distribution") == 0 ||
                  strcmp(argv[i], "-dist") == 0)) {
         const char *flag = argv[i];
         dist_name = cli_parse_nonempty_string(
             cli_arg_value(argc, argv, &i, flag), flag);
      } else {
         return cli_unknown_arg(argv[i]);
      }
   }

   tree_registry_selection_t tree_selection;
   tree_registry_selection_init(&tree_selection, tree_name);

   const dist_spec_t *dist_spec = dist_find_or_exit(dist_name);
   quickbench_require_distribution(dist_spec);

   arena_t arena;
   arena_init(&arena, arena_cap);
   arena_t *previous_arena = arena_install(&arena);
   arena_prewarm(&arena);

   operation_t op = quickbench_insert_delete_operation(scale, steps);

   printf("Operation: %s, Distribution: %s, Scale: %llu, Steps: %llu, Arena: "
          "%llu\n",
          op.name, dist_spec->name, (unsigned long long)scale,
          (unsigned long long)steps, (unsigned long long)arena_cap);
   printf("%-28s %12s %12s\n", "Tree", "Time(ms)", "Mops/s");
   printf("%-28s %12s %12s\n", "----", "-------", "------");

   for (int t = 0; t < tree_selection.count; t++) {
      quickbench_require_tree(tree_selection.items[t]);
      arena_reset(&arena);
      u64 workload_seed =
          quickbench_workload_seed(&op, tree_selection.items[t], dist_spec);
      u64 tree_seed =
          random_derive_seed(workload_seed, tree_selection.items[t]->name);
      tree_seed = random_derive_seed(tree_seed, "tree");
      random_seed(tree_seed);
      distribution_t *dist = dist_create(dist_spec, workload_seed);
      list_t instance = list_new(tree_selection.items[t]);
      const list_vtable_t *tree_vt = tree_selection.items[t];
      list_time_inserts_fn time_inserts_fn = tree_vt->time_inserts_fn;
      list_time_insert_delete_fn time_insert_delete_fn =
          tree_vt->time_insert_delete_fn;
      u64 step_count = 0;

#ifdef BST_FAST_BENCHMARK
      bool inserts_only = operation_inserts_only(&op);
      u64 *positions = (u64 *)bst_malloc_array(steps, sizeof(positions[0]));
      u64 prepared_size;
      if (inserts_only) {
         prepared_size = operation_prepare_insert_positions(&op, dist, 0, 0,
                                                            positions, steps);
      } else {
         prepared_size = operation_prepare_insert_delete_positions(
             &op, dist, 0, 0, positions, steps);
      }
      operation_position_chunk_t chunk =
          operation_position_chunk_plan(&op, 0, steps);
#endif
#ifdef BST_FAST_BENCHMARK
      const u64 *chunk_positions = positions;
      u64 elapsed;
      if (chunk.insert_count > 0 && chunk.insert_delete_count > 0) {
         elapsed = time_insert_delete_fn(
             &instance, chunk_positions, chunk.insert_count,
             chunk.insert_delete_count, chunk.insert_delete_next_is_insert);
      } else if (chunk.insert_count > 0) {
         elapsed =
             time_inserts_fn(&instance, chunk_positions, chunk.insert_count);
      } else {
         invariant(chunk.insert_delete_count > 0);
         elapsed = time_insert_delete_fn(&instance, chunk_positions, 0,
                                         chunk.insert_delete_count,
                                         chunk.insert_delete_next_is_insert);
      }
#else
      u64 start = monotonic_time_ns();
      for (u64 i = 0; i < steps; i++) {
         instance = operation_update(&op, instance, dist, &step_count);
      }
      u64 elapsed = elapsed_time_ns_since_nonzero(start);
#endif
#ifdef BST_FAST_BENCHMARK
      step_count = steps;
#endif
      invariant(step_count == steps);
#ifdef BST_FAST_BENCHMARK
      invariant(list_size_fast(&instance) == prepared_size);
#endif

#ifdef DEBUG
      list_verify(&instance);
#endif

      double ms = (double)elapsed / 1e6;
      double seconds = (double)elapsed / 1e9;
      double mops = seconds > 0.0 ? (double)steps / seconds / 1e6 : 0.0;
      printf("%-28s %10.1f %10.2f\n", tree_selection.items[t]->name, ms, mops);

#ifdef BST_FAST_BENCHMARK
      free(positions);
#endif
      list_free(&instance);
      dist_destroy(dist);
   }

   arena_restore(previous_arena);
   arena_destroy(&arena);
   return 0;
}
