// Measurement harness for BST operations.
//
// Matches the Go measurement structure (docs/benchmarks/main/operations/measurements.go):
//   - For each tree × distribution:
//     - Create a new tree
//     - Perform `steps` operations in chunks of `step`
//     - Record structural measurements per chunk
//
// Must be compiled with -DMEASUREMENTS to enable rotation/allocation counters.
// Persistent operations are available in builds compiled with -DPERSISTENT
// (for example, the measurements-persistent Make target).
//
// Usage:
//   ./measurements_bin [-operation Insert|InsertDelete]
//                      [-scale N] [-steps N] [-samples N]
//                      [-tree NAME] [-dist NAME]
//                      [-arena N] [-output-dir PATH]
//
#ifndef MEASUREMENTS
#error "c/measurements.c requires -DMEASUREMENTS to emit valid counters"
#endif

#include "bst.h"
#include "benchmark_schema.h"
#include "cli.h"
#include "distributions.h"
#include "filesystem.h"
#include "operations.h"
#include "operation_runs.h"
#include "random.h"
#include "time_utils.h"
#include "tree_registry.h"

#include <stdio.h>
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// Measurement runner
// ─────────────────────────────────────────────────────────────────────────────

static u64 measurement_seed(const operation_run_t *run) {
   return operation_run_workload_seed(run, "operation-measurements");
}

static void measurement_progress(operation_run_t *run) {
   char timebuf[64];
   format_local_time_or_exit(timebuf, sizeof(timebuf), "%H:%M:%S");
   printf("%s %-32s %-32s %-32s\n", timebuf, run->op->name, run->tree_vt->name,
          run->dist->name);
   fflush(stdout);
}

static void measurement_before_chunk(operation_run_t *run) {
   (void)run;
   measurement_reset_all();
}

static void measurement_after_chunk(operation_run_t *run) {
   invariant(run != NULL);
   invariant(run->file != NULL);
   invariant(run->path != NULL);
   dist_spec_require(run->dist);
   list_verify(&run->instance);
   tree_t *inner = list_tree(&run->instance);
   invariant(inner != NULL);

   measurement_snapshot_t measurements = measurement_snapshot();

   node_path_metrics_t path =
       tree_verify_size_path_metrics(inner->root, inner->size);
   double apl = nodeAveragePathLengthFromMetrics(path);
   int mpl = nodeMaximumPathLengthFromMetrics(path);

   file_printf_or_exit(
       run->file, run->path,
       "%s %llu %llu %llu %llu %llu %llu %llu %.17g %d %llu %llu\n",
       run->dist->name, (unsigned long long)run->scale,
       (unsigned long long)run->steps, (unsigned long long)inner->size,
       (unsigned long long)run->step, (unsigned long long)run->position,
       (unsigned long long)measurements.partition_count,
       (unsigned long long)measurements.partition_depth, apl, mpl,
       (unsigned long long)measurements.rotations,
       (unsigned long long)measurements.allocations);
}

static void run_measurements(const operation_t *op, u64 samples, u64 arena_cap,
                             const char *output_dir,
                             const list_vtable_t *const *trees, int tree_count,
                             const dist_spec_t *const *distributions,
                             int distribution_count) {
   operation_run_config_t config = {
       .samples = samples,
       .arena_cap = arena_cap,
       .iterations = 1,
       .output_dir = output_dir,
       .kind = "measurements",
       .header = BENCHMARK_SCHEMA_OPERATION_MEASUREMENT_HEADER,
       .trees = trees,
       .tree_count = tree_count,
       .distributions = distributions,
       .distribution_count = distribution_count,
       .seed = measurement_seed,
   };
   operation_run_matrix(op, &config, measurement_progress,
                        measurement_before_chunk, measurement_after_chunk,
                        NULL);
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char **argv) {
   random_seed(1);

   const char *op_name = NULL;
   u64 scale_override = 0;
   u64 steps_override = 0;
   bool scale_override_set = false;
   bool steps_override_set = false;
   u64 samples = 1000;
   u64 arena_cap = 50000000;
   const char *output_dir = "docs/benchmarks/data";
   const char *tree_name = NULL;
   const char *dist_name = NULL;

   for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-operation") == 0) {
         op_name = cli_parse_nonempty_string(
             cli_arg_value(argc, argv, &i, "-operation"), "-operation");
      } else if (strcmp(argv[i], "-scale") == 0) {
         scale_override = cli_parse_positive_u64(
             cli_arg_value(argc, argv, &i, "-scale"), "-scale");
         scale_override_set = true;
      } else if (strcmp(argv[i], "-steps") == 0) {
         steps_override = cli_parse_positive_step_count(
             cli_arg_value(argc, argv, &i, "-steps"), "-steps");
         steps_override_set = true;
      } else if (strcmp(argv[i], "-samples") == 0) {
         samples = cli_parse_positive_sample_count(
             cli_arg_value(argc, argv, &i, "-samples"), "-samples");
      } else if (strcmp(argv[i], "-arena") == 0) {
         arena_cap = cli_parse_positive_arena_capacity(
             cli_arg_value(argc, argv, &i, "-arena"), "-arena");
      } else if (strcmp(argv[i], "-output-dir") == 0) {
         output_dir = cli_parse_nonempty_string(
             cli_arg_value(argc, argv, &i, "-output-dir"), "-output-dir");
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

   operation_t operations[operation_default_count];
   int num_ops = operation_defaults_for_name(operations, op_name);

   tree_registry_selection_t tree_selection;
   tree_registry_selection_init(&tree_selection, tree_name);

   dist_selection_t distributions;
   dist_benchmark_selection_init(&distributions, dist_name);

   for (operation_t *op = operations; op < operations + num_ops; op++) {
      if (scale_override_set) {
         op->scale = scale_override;
      }
      if (steps_override_set) {
         op->steps = steps_override;
      }
      run_measurements(op, samples, arena_cap, output_dir, tree_selection.items,
                       tree_selection.count, distributions.items,
                       distributions.count);
   }

   return 0;
}
