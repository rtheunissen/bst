#ifndef OPERATION_RUNS_H
#define OPERATION_RUNS_H

#include "distributions.h"
#include "filesystem.h"
#include "list.h"
#include "operations.h"
#include "sampling.h"

#include <stddef.h>
#include <stdio.h>

typedef struct operation_run operation_run_t;
typedef struct operation_run_config operation_run_config_t;

struct operation_run {
   const operation_t *op;
   const list_vtable_t *tree_vt;
   const dist_spec_t *dist;
   FILE *file;
   const char *path;
   list_t instance;
   bool instance_size_known;
   u64 instance_size;
   u64 scale;
   u64 steps;
   u64 step;
   u64 position;
   u64 step_count;
   u64 iteration;
   u64 iterations;
   bool chunk_timer_started;
   bool chunk_duration_known;
   u64 chunk_start_ns;
   u64 chunk_duration_ns;
   void *ctx;
};

typedef void (*operation_run_path_fn)(const operation_run_t *run,
                                      const operation_run_config_t *config,
                                      char *path, size_t path_size, char *dir,
                                      size_t dir_size);

struct operation_run_config {
   u64 samples;
   u64 arena_cap;
   u64 iterations;
   const char *output_dir;
   const char *kind;
   const char *header;
   const list_vtable_t *const *trees;
   int tree_count;
   const dist_spec_t *const *distributions;
   int distribution_count;
   bool prewarm_arena;
   u64 (*seed)(const operation_run_t *run);
   operation_run_path_fn path;
};

typedef void (*operation_run_hook_t)(operation_run_t *run);

void operation_run_start_chunk_timer(operation_run_t *run);
void operation_run_stop_chunk_timer(operation_run_t *run);
u64 operation_run_chunk_duration(operation_run_t *run);

static inline u64 operation_run_reported_size(const operation_run_t *run) {
   invariant(run != NULL);
   if (run->instance_size_known) {
      return run->instance_size;
   }
   list_t instance = run->instance;
   return list_size(&instance);
}

sample_schedule_t operation_run_sample_schedule(u64 steps, u64 samples);
static inline u64 operation_run_next_iteration(u64 iteration) {
   filesystem_require(iteration < UINT64_MAX,
                      "operation iteration count is too large");
   return size_after_inserting_one(iteration);
}

// Tree-independent seed for the operation stream; tree RNGs derive separately.
// The workload key includes scale and steps so distinct workloads cannot
// accidentally reuse a distribution stream.
u64 operation_run_workload_seed(const operation_run_t *run, const char *domain);

void operation_run_matrix(const operation_t *op,
                          const operation_run_config_t *config,
                          operation_run_hook_t progress,
                          operation_run_hook_t before_chunk,
                          operation_run_hook_t after_chunk, void *ctx);

#endif // OPERATION_RUNS_H
