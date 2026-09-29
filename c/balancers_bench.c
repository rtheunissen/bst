// Balancer timing benchmarks.
//
// Matches the Go benchmark structure (docs/benchmarks/main/balancers/benchmarks.go):
//   - For each balancer × distribution × size:
//     - Grow a Splay tree to that size
//     - Repeatedly: randomize the tree, time the restore, until duration reached
//     - Output average duration per restore
//
// Usage:
//   ./balancers_bench_bin [-balancer Median|Height|Weight|Log|Cost|DSW]
//                         [-dist NAME]
//                         [-scale N] [-samples N] [-duration-ms N]
//                         [-output-dir PATH]
//
#include "bst.h"
#include "balancer_runs.h"
#include "balancer_registry.h"
#include "benchmark_order.h"
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
// Balancer benchmark runner
// ─────────────────────────────────────────────────────────────────────────────

static void run_balancer_benchmarks(const balancer_t *selected,
                                    const char *dist_name, u64 scale,
                                    u64 samples, u64 duration_ns,
                                    const char *output_dir) {
   invariant(output_dir != NULL);
   invariant(output_dir[0] != '\0');
   filesystem_require(duration_ns > 0, "balancer benchmark duration is zero");
   if (selected) {
      balancer_require(selected);
   }

   sample_schedule_t schedule = balancer_run_sample_schedule(scale, samples);
   scale = schedule.total;
   samples = schedule.samples;
   invariant(duration_ns > 0);

   balancer_run_distribution_selection_t distributions;
   balancer_run_distribution_selection_init_by_name(&distributions, dist_name);

   balancer_run_output_t *outputs = balancer_run_outputs_alloc();
   int output_count = balancer_run_outputs_make(
       outputs, balancer_registry_count, selected, output_dir, "benchmarks");
   balancer_run_outputs_prepare_dirs(outputs, output_count);

   FILE **files =
       (FILE **)bst_calloc_array((u64)output_count, sizeof(files[0]));
   for (int b = 0; b < output_count; b++) {
      balancer_run_output_t *output = &outputs[b];
      const char *path = output->file.path;
      balancer_require(output->balancer);
      files[b] = staged_file_open_or_exit(&output->file, "w");
      file_printf_or_exit(files[b], path, "%s\n",
                          BENCHMARK_SCHEMA_BALANCER_BENCHMARK_HEADER);
   }

   arena_t arena;
   arena_init(&arena, scale);
   arena_t *previous_arena = arena_install(&arena);
   arena_prewarm(&arena);

   // Every candidate sees the same deterministic randomized tree for its nth
   // restore. Candidate order changes between rounds, but the workload does not.
   splay_t instance;
   splay_init(&instance);

   u64 *iterations =
       (u64 *)bst_calloc_array((u64)output_count, sizeof(iterations[0]));
   u64 *restore_durations =
       (u64 *)bst_calloc_array((u64)output_count, sizeof(restore_durations[0]));

   u64 previous_position = 0;
   for (u64 sample = 0; sample < samples; sample++) {
      sample_schedule_point_t point =
          sample_schedule_next_point(schedule, sample, previous_position);
      u64 position = point.position;
      previous_position = position;

      balancer_run_grow_splay_to(&instance, position);

      for (int d = 0; d < distributions.count; d++) {
         const dist_spec_t *dist_spec = distributions.items[d];
         dist_spec_require(dist_spec);
         bst_array_zero(iterations, (u64)output_count, sizeof(iterations[0]));
         bst_array_zero(restore_durations, (u64)output_count,
                        sizeof(restore_durations[0]));

         int remaining = output_count;
         u64 round = 0;
         while (remaining > 0) {
            for (int order = 0; order < output_count; order++) {
               int b = benchmark_balanced_latin_square_index(output_count,
                                                             round, order);
               if (duration_reached(restore_durations[b], duration_ns)) {
                  continue;
               }

               const balancer_t *balancer = outputs[b].balancer;
               iterations[b] = balancer_run_next_iteration(iterations[b]);
               instance.tree = balancer_run_randomize_tree(
                   instance.tree, dist_spec, "balancer-benchmarks",
                   iterations[b]);

               u64 checkpoint = monotonic_time_ns();
               instance.tree =
                   balancer_run_restore_tree(balancer, instance.tree);
               u64 elapsed = elapsed_time_ns_since_nonzero(checkpoint);
               filesystem_require(
                   !duration_sum_would_overflow(restore_durations[b], elapsed),
                   "balancer benchmark duration total is too large");
               restore_durations[b] =
                   duration_sum(restore_durations[b], elapsed);

               if (duration_reached(restore_durations[b], duration_ns)) {
                  balancer_verify_tree(balancer, instance.tree);
                  remaining--;
               }
            }
            round = balancer_run_next_iteration(round);
         }

         for (int b = 0; b < output_count; b++) {
            const balancer_run_output_t *output = &outputs[b];
            const balancer_t *balancer = output->balancer;
            const char *path = output->file.path;
            invariant(instance.tree.size == position);
            invariant(iterations[b] > 0);
            invariant(restore_durations[b] >= iterations[b]);

            u64 avg_ns =
                duration_average_nonzero(restore_durations[b], iterations[b]);

            file_printf_or_exit(files[b], path, "%s %llu %llu %llu %llu %llu\n",
                                dist_spec->name, (unsigned long long)scale,
                                (unsigned long long)instance.tree.size,
                                (unsigned long long)iterations[b],
                                (unsigned long long)avg_ns,
                                (unsigned long long)restore_durations[b]);

            char timebuf[64];
            format_local_time_or_exit(timebuf, sizeof(timebuf), "%H:%M:%S");
            printf("%s %-10s %-10s %10llu/%llu %llux\n", timebuf,
                   balancer->name, dist_spec->name,
                   (unsigned long long)position, (unsigned long long)scale,
                   (unsigned long long)iterations[b]);
            fflush(stdout);
         }
      }
   }

   free(restore_durations);
   free(iterations);
   splay_free(&instance);
   arena_restore(previous_arena);
   arena_destroy(&arena);

   for (int b = 0; b < output_count; b++) {
      staged_file_finish_or_exit(&outputs[b].file, files[b]);
   }
   free(files);
   balancer_run_outputs_publish(outputs, output_count);
   balancer_run_outputs_dispose(outputs, output_count);
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char **argv) {
   random_seed(1);

   u64 scale = 10000000;
   u64 samples = 100;
   u64 duration_ns = 10ULL * TIME_NS_PER_SECOND;
   const char *output_dir = "docs/benchmarks/data";
   const char *strategy_name = NULL;
   const char *dist_name = NULL;

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
      } else if (strcmp(argv[i], "-duration-ms") == 0) {
         duration_ns = cli_parse_duration_ms_ns(
             cli_arg_value(argc, argv, &i, "-duration-ms"), "-duration-ms");
      } else if (strcmp(argv[i], "-output-dir") == 0) {
         output_dir = cli_parse_nonempty_string(
             cli_arg_value(argc, argv, &i, "-output-dir"), "-output-dir");
      } else {
         return cli_unknown_arg(argv[i]);
      }
   }

   const balancer_t *selected =
       balancer_registry_find_optional_or_exit(strategy_name);

   run_balancer_benchmarks(selected, dist_name, scale, samples, duration_ns,
                           output_dir);

   return 0;
}
