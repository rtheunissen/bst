#ifndef BALANCER_RUNS_H
#define BALANCER_RUNS_H

#include "balancer_registry.h"
#include "distributions.h"
#include "filesystem.h"
#include "sampling.h"
#include "splay.h"

typedef struct {
   const balancer_t *balancer;
   staged_file_t file;
   char *dir;
} balancer_run_output_t;

typedef struct {
   const dist_spec_t *selected[1];
   const dist_spec_t *const *items;
   int count;
} balancer_run_distribution_selection_t;

balancer_run_output_t *balancer_run_outputs_alloc(void);
sample_schedule_t balancer_run_sample_schedule(u64 scale, u64 samples);
static inline u64 balancer_run_next_iteration(u64 iteration) {
   filesystem_require(iteration < UINT64_MAX,
                      "balancer iteration count is too large");
   return size_after_inserting_one(iteration);
}
u64 balancer_run_seed(const char *domain, const dist_spec_t *dist_spec,
                      u64 position, u64 iteration);
void balancer_run_grow_splay_to(splay_t *instance, u64 size);
tree_t balancer_run_randomize_tree(tree_t tree, const dist_spec_t *dist_spec,
                                   const char *seed_domain, u64 iteration);
static inline tree_t balancer_run_restore_tree(const balancer_t *balancer,
                                               tree_t tree) {
   balancer_require(balancer);
   tree_sized_handle_require(tree);
   u64 size_before_restore = tree.size;
   tree = balancer->restore(tree);
   invariant(tree.size == size_before_restore);
   tree_sized_handle_require(tree);
#ifdef DEBUG
   balancer_verify_tree(balancer, tree);
#endif
   return tree;
}
void balancer_run_distribution_selection_init(
    balancer_run_distribution_selection_t *selection,
    const dist_spec_t *selected);
void balancer_run_distribution_selection_init_by_name(
    balancer_run_distribution_selection_t *selection, const char *name);
int balancer_run_outputs_make(balancer_run_output_t *outputs, int capacity,
                              const balancer_t *selected,
                              const char *output_dir, const char *kind);
void balancer_run_outputs_prepare_dirs(balancer_run_output_t *outputs,
                                       int count);
void balancer_run_outputs_publish(balancer_run_output_t *outputs, int count);
void balancer_run_outputs_dispose(balancer_run_output_t *outputs, int count);

#endif // BALANCER_RUNS_H
