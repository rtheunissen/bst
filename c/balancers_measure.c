// Balancer measurement harness.
//
// Matches the Go measurement structure (docs/benchmarks/main/balancers/measurements.go):
//   - For each balancer × distribution × size:
//     - Grow a Splay tree to that size
//     - Randomize the tree
//     - Reset measurement counters
//     - Restore the tree (balancing)
//     - Record structural measurements
//
// Must be compiled with -DMEASUREMENTS to enable rotation/partition counters.
//
// Usage:
//   ./balancers_measure_bin [-balancer Median|Height|Weight|Log|Cost|DSW]
//                          [-dist NAME]
//                          [-scale N] [-samples N] [-iterations N]
//                          [-output-dir PATH]
//
#ifndef MEASUREMENTS
#error "c/balancers_measure.c requires -DMEASUREMENTS to emit valid counters"
#endif

#include "bst.h"
#include "balancer_runs.h"
#include "balancer_registry.h"
#include "benchmark_schema.h"
#include "cli.h"
#include "distributions.h"
#include "filesystem.h"
#include "random.h"
#include "sampling.h"
#include "time_utils.h"

#include <stdio.h>
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// Measurement runner
// ─────────────────────────────────────────────────────────────────────────────

static void run_balancer_measurements(const balancer_t *selected,
                                      const char *dist_name, u64 scale,
                                      u64 samples, u64 iterations,
                                      const char *output_dir) {
   if (selected) {
      balancer_require(selected);
   }
   invariant(output_dir != NULL);
   invariant(output_dir[0] != '\0');

   sample_schedule_t schedule = balancer_run_sample_schedule(scale, samples);
   scale = schedule.total;
   samples = schedule.samples;
   if (iterations == 0) {
      iterations = 1;
   }
   filesystem_require(iterations < UINT64_MAX,
                      "balancer iteration count is too large");
   invariant(iterations < UINT64_MAX);

   balancer_run_distribution_selection_t distributions;
   balancer_run_distribution_selection_init_by_name(&distributions, dist_name);

   balancer_run_output_t *outputs = balancer_run_outputs_alloc();
   int output_count = balancer_run_outputs_make(
       outputs, balancer_registry_count, selected, output_dir, "measurements");
   balancer_run_outputs_prepare_dirs(outputs, output_count);

   for (int b = 0; b < output_count; b++) {
      balancer_run_output_t *output = &outputs[b];
      const balancer_t *balancer = output->balancer;
      balancer_require(balancer);
      const char *path = output->file.path;

      FILE *file = staged_file_open_or_exit(&output->file, "w");

      // Header
      file_printf_or_exit(file, path, "%s\n",
                          BENCHMARK_SCHEMA_BALANCER_MEASUREMENT_HEADER);

      arena_t arena;
      arena_init(&arena, (u64)scale);
      arena_t *previous_arena = arena_install(&arena);

      // Build a splay tree incrementally
      splay_t instance;
      splay_init(&instance);

      u64 previous_position = 0;
      for (u64 sample = 0; sample < samples; sample++) {
         sample_schedule_point_t point =
             sample_schedule_next_point(schedule, sample, previous_position);
         u64 position = point.position;
         u64 ordinal = point.ordinal;
         previous_position = position;

         balancer_run_grow_splay_to(&instance, position);

         // Print progress
         if (ordinal == 1 || ordinal == samples || ordinal % 100 == 0) {
            char timebuf[64];
            format_local_time_or_exit(timebuf, sizeof(timebuf), "%H:%M:%S");
            printf("%s %-10s %10llu/%llu\n", timebuf, balancer->name,
                   (unsigned long long)position, (unsigned long long)scale);
            fflush(stdout);
         }

         for (int d = 0; d < distributions.count; d++) {
            const dist_spec_t *dist_spec = distributions.items[d];
            dist_spec_require(dist_spec);

            for (u64 iteration = 0; iteration < iterations; iteration++) {
               u64 iteration_number = balancer_run_next_iteration(iteration);
               instance.tree = balancer_run_randomize_tree(
                   instance.tree, dist_spec, "balancer-measurements",
                   iteration_number);

               // Reset measurement counters
               measurement_reset_all();

               // Restore (balance) the tree
               instance.tree =
                   balancer_run_restore_tree(balancer, instance.tree);

               balancer_verify_tree(balancer, instance.tree);

               // Collect measurements
               measurement_snapshot_t measurements = measurement_snapshot();

               node_path_metrics_t path_metrics = tree_verify_size_path_metrics(
                   instance.tree.root, instance.tree.size);
               double apl = nodeAveragePathLengthFromMetrics(path_metrics);
               int mpl = nodeMaximumPathLengthFromMetrics(path_metrics);

               file_printf_or_exit(
                   file, path,
                   "%s %llu %llu %llu %llu %llu %.17g %d %llu %llu\n",
                   dist_spec->name, (unsigned long long)scale,
                   (unsigned long long)position,
                   (unsigned long long)iteration_number,
                   (unsigned long long)measurements.partition_count,
                   (unsigned long long)measurements.partition_depth, apl, mpl,
                   (unsigned long long)measurements.rotations,
                   (unsigned long long)measurements.allocations);
            }
         }
      }

      splay_free(&instance);
      arena_restore(previous_arena);
      arena_destroy(&arena);
      staged_file_finish_or_exit(&output->file, file);
   }

   balancer_run_outputs_publish(outputs, output_count);
   balancer_run_outputs_dispose(outputs, output_count);
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char **argv) {
   random_seed(1);

   const char *strategy_name = NULL;
   const char *dist_name = NULL;
   u64 scale = 10000000;
   u64 samples = 10000;
   u64 iterations = 1;
   const char *output_dir = "docs/benchmarks/data";

   for (int i = 1; i < argc; i++) {
      if ((strcmp(argv[i], "-balancer") == 0 ||
           strcmp(argv[i], "-strategy") == 0)) {
         const char *flag = argv[i];
         strategy_name = cli_parse_nonempty_string(
             cli_arg_value(argc, argv, &i, flag), flag);
      } else if ((strcmp(argv[i], "-distribution") == 0 ||
                  strcmp(argv[i], "-dist") == 0)) {
         const char *flag = argv[i];
         dist_name = cli_parse_nonempty_string(
             cli_arg_value(argc, argv, &i, flag), flag);
      } else if (strcmp(argv[i], "-scale") == 0) {
         scale = cli_parse_positive_arena_capacity(
             cli_arg_value(argc, argv, &i, "-scale"), "-scale");
      } else if (strcmp(argv[i], "-samples") == 0) {
         samples = cli_parse_positive_sample_count(
             cli_arg_value(argc, argv, &i, "-samples"), "-samples");
      } else if (strcmp(argv[i], "-iterations") == 0) {
         iterations = cli_parse_positive_iteration_count(
             cli_arg_value(argc, argv, &i, "-iterations"), "-iterations");
      } else if (strcmp(argv[i], "-output-dir") == 0) {
         output_dir = cli_parse_nonempty_string(
             cli_arg_value(argc, argv, &i, "-output-dir"), "-output-dir");
      } else {
         return cli_unknown_arg(argv[i]);
      }
   }

   const balancer_t *selected =
       balancer_registry_find_optional_or_exit(strategy_name);

   run_balancer_measurements(selected, dist_name, scale, samples, iterations,
                             output_dir);

   return 0;
}
