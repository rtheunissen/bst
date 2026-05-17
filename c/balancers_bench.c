// Balancer timing benchmarks.
//
// Matches the Go benchmark structure (docs/benchmarks/main/balancers/benchmarks.go):
//   - For each balancer × distribution × size:
//     - Grow a Splay tree to that size
//     - Repeatedly: randomize the tree, time the restore, until duration reached
//     - Output average duration per restore
//
// Usage:
//   ./balancers_bench_bin [-scale N] [-samples N] [-duration-ms N]
//                         [-output-dir PATH]
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
// Balancer benchmark runner
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

static const char *bench_dist_names[] = {
   "Uniform",
};
static const dist_factory_fn bench_dist_factories[] = {
   uniform_new,
};
static const int num_bench_dists = 1;

static void run_balancer_benchmarks(int scale, int samples, u64 duration_ns, const char *output_dir) {

   if (scale < 1) scale = 1;
   if (samples < 1) samples = 1;
   if (samples > scale) samples = scale;
   int step = scale / samples;

   for (int b = 0; b < num_balancers; b++) {
      const balancer_t *balancer = all_balancers[b];

      // Create output file
      char path[512];
      path_format(path, sizeof(path),
         "%s/balancers/benchmarks/%s",
         output_dir, balancer->name);

      // Ensure directory exists
      char dir[512];
      path_format(dir, sizeof(dir), "%s/balancers/benchmarks", output_dir);
      mkdir_p(dir);

      FILE *file = fopen(path, "w");
      if (!file) { perror(path); continue; }

      // Header
      fprintf(file, "Distribution Scale Size Iterations Duration\n");

      // Build a splay tree incrementally
      splay_t instance;
      splay_init(&instance);

      for (int position = step; position <= scale; position += step) {

         // Grow the tree
         while (instance.tree.size < (u64)position) {
            splay_insert(&instance, 0, 0);
         }

         for (int d = 0; d < num_bench_dists; d++) {

            u64 start = time_ns();
            int iterations = 0;
            u64 restore_duration = 0;

            for (;;) {
               iterations++;

               // Randomize the tree
               distribution_t *dist = bench_dist_factories[d]((u64)iterations);
               instance.tree = tree_randomize(instance.tree, dist);
               dist_destroy(dist);

               // Time the restore
               u64 checkpoint = time_ns();
               instance.tree = balancer->restore(instance.tree);
               restore_duration += time_ns() - checkpoint;

               if (time_ns() - start > duration_ns) {
                  break;
               }
            }

            u64 avg_ns = restore_duration / (u64)iterations;

            fprintf(file, "%s %d %llu %d %llu\n",
               bench_dist_names[d],
               scale,
               (unsigned long long)instance.tree.size,
               iterations,
               (unsigned long long)avg_ns);

            // Print progress
            time_t now = time(NULL);
            struct tm *tm = localtime(&now);
            char timebuf[64];
            strftime(timebuf, sizeof(timebuf), "%H:%M:%S", tm);
            printf("%s %-10s %-10s %10d/%d %dx\n",
               timebuf,
               balancer->name,
               bench_dist_names[d],
               position,
               scale,
               iterations);
            fflush(stdout);
         }
      }

      splay_free(&instance);
      fclose(file);
   }
}


// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char **argv) {
   random_seed(1);

   int scale   = 10000000;
   int samples = 100;
   u64 duration_ns = 10ULL * 1000000000ULL; // 10 seconds
   const char *output_dir = "docs/benchmarks/data";

   for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-scale") == 0 && i + 1 < argc) {
         scale = parse_int(argv[++i], "-scale");
      } else if (strcmp(argv[i], "-samples") == 0 && i + 1 < argc) {
         samples = parse_int(argv[++i], "-samples");
      } else if (strcmp(argv[i], "-duration-ms") == 0 && i + 1 < argc) {
         duration_ns = parse_u64(argv[++i], "-duration-ms") * 1000000ULL;
      } else if (strcmp(argv[i], "-output-dir") == 0 && i + 1 < argc) {
         output_dir = argv[++i];
      } else {
         fprintf(stderr, "unknown or incomplete argument: %s\n", argv[i]);
         return 2;
      }
   }

   run_balancer_benchmarks(scale, samples, duration_ns, output_dir);

   return 0;
}
