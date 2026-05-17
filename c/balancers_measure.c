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
//   ./balancers_measure_bin [-strategy Median|Height|Weight|Log|Cost|DSW]
//                          [-scale N] [-samples N] [-iterations N]
//                          [-output-dir PATH]
//
#include "bst.h"
#include "list.h"
#include "random.h"
#include "distributions.h"
#include "splay.h"
#include "balancers.h"
#include "filesystem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
// Balancer and distribution tables
// ─────────────────────────────────────────────────────────────────────────────

static const balancer_t *all_balancers[] = {
   &balancer_median,
   &balancer_height,
   &balancer_weight,
   &balancer_log,
   &balancer_cost,
   &balancer_dsw,
};
static const int num_balancers = sizeof(all_balancers) / sizeof(all_balancers[0]);

static const char *measure_dist_names[] = {
   "Uniform",
};
static const dist_factory_fn measure_dist_factories[] = {
   uniform_new,
};
static const int num_measure_dists = 1;


// ─────────────────────────────────────────────────────────────────────────────
// Measurement runner
// ─────────────────────────────────────────────────────────────────────────────

static void run_balancer_measurements(const balancer_t *balancer, int scale, int samples, int iterations, const char *output_dir) {
   if (scale < 1) scale = 1;
   if (samples < 1) samples = 1;
   if (samples > scale) samples = scale;
   if (iterations < 1) iterations = 1;

   // Create output file
   char path[512];
   path_format(path, sizeof(path),
      "%s/balancers/measurements/%s",
      output_dir, balancer->name);

   // Ensure directory exists
   char dir[512];
   path_format(dir, sizeof(dir), "%s/balancers/measurements", output_dir);
   mkdir_p(dir);

   FILE *file = fopen(path, "w");
   if (!file) { perror(path); return; }

   // Header
   fprintf(file, "Distribution Scale Size"
                 " PartitionCount PartitionDepth"
                 " AveragePathLength MaximumPathLength"
                 " Rotations\n");

   // Build a splay tree incrementally
   splay_t instance;
   splay_init(&instance);

   int step = scale / samples;

   for (int position = step; position <= scale; position += step) {

      // Grow the tree if needed
      while (instance.tree.size < (u64)position) {
         splay_insert(&instance, 0, 0);
      }

      // Print progress
      if (position % (scale / step) == 0) {
         time_t now = time(NULL);
         struct tm *tm = localtime(&now);
         char timebuf[64];
         strftime(timebuf, sizeof(timebuf), "%H:%M:%S", tm);
         printf("%s %-10s %10d/%d\n",
            timebuf,
            balancer->name,
            position,
            scale);
         fflush(stdout);
      }

      for (int d = 0; d < num_measure_dists; d++) {

         distribution_t *source = measure_dist_factories[d]((u64)(position + 1));

         for (int iteration = 1; iteration <= iterations; iteration++) {

            // Randomize the tree
            instance.tree = tree_randomize(instance.tree, source);

            // Reset measurement counters
            #ifdef MEASUREMENTS
            measurement_rotations = 0;
            measurement_allocations = 0;
            measurement_partition_count = 0;
            measurement_partition_depth = 0;
            #endif

            // Restore (balance) the tree
            instance.tree = balancer->restore(instance.tree);

            // Collect measurements
            #ifdef MEASUREMENTS
            u64 rot = measurement_rotations;
            u64 pc = measurement_partition_count;
            u64 pd = measurement_partition_depth;
            #else
            u64 rot = 0;
            u64 pc = 0;
            u64 pd = 0;
            #endif

            double apl = nodeAveragePathLength(instance.tree.root);
            int mpl = nodeMaximumPathLength(instance.tree.root);

            fprintf(file, "%s %d %d %llu %llu %g %d %llu\n",
               measure_dist_names[d],
               scale,
               position,
               (unsigned long long)pc,
               (unsigned long long)pd,
               apl,
               mpl,
               (unsigned long long)rot);
         }
         dist_destroy(source);
      }
   }

   splay_free(&instance);
   fclose(file);
}


// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char **argv) {
   random_seed(1);

   const char *strategy_name = NULL;
   int scale      = 10000000;
   int samples    = 10000;
   int iterations = 1;
   const char *output_dir = "docs/benchmarks/data";

   for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-strategy") == 0 && i + 1 < argc) {
         strategy_name = argv[++i];
      } else if (strcmp(argv[i], "-scale") == 0 && i + 1 < argc) {
         scale = parse_int(argv[++i], "-scale");
      } else if (strcmp(argv[i], "-samples") == 0 && i + 1 < argc) {
         samples = parse_int(argv[++i], "-samples");
      } else if (strcmp(argv[i], "-iterations") == 0 && i + 1 < argc) {
         iterations = parse_int(argv[++i], "-iterations");
      } else if (strcmp(argv[i], "-output-dir") == 0 && i + 1 < argc) {
         output_dir = argv[++i];
      } else {
         fprintf(stderr, "unknown or incomplete argument: %s\n", argv[i]);
         return 2;
      }
   }

   for (int b = 0; b < num_balancers; b++) {
      if (strategy_name && strcmp(strategy_name, all_balancers[b]->name) != 0) {
         continue;
      }
      run_balancer_measurements(all_balancers[b], scale, samples, iterations, output_dir);
   }

   return 0;
}
