#include "balancer_runs.h"

#include "random.h"

#include <string.h>

balancer_run_output_t *balancer_run_outputs_alloc(void) {
   invariant(balancer_registry_count > 0);
   return (balancer_run_output_t *)bst_calloc_array(
       (u64)balancer_registry_count, sizeof(balancer_run_output_t));
}

sample_schedule_t balancer_run_sample_schedule(u64 scale, u64 samples) {
   sample_schedule_t schedule = sample_schedule_make(scale, samples);
   sample_schedule_require(schedule);
   filesystem_require(schedule.total <= arena_max_capacity(),
                      "balancer scale is too large");
   return schedule;
}

static void
balancer_run_distribution_require_registered(const dist_spec_t *dist_spec) {
   dist_spec_require(dist_spec);
   filesystem_require(dist_find(dist_spec->name) == dist_spec,
                      "balancer distribution is not registered");
}

static void balancer_run_seed_require(const char *domain,
                                      const dist_spec_t *dist_spec,
                                      u64 position, u64 iteration) {
   invariant(domain != NULL);
   invariant(domain[0] != '\0');
   balancer_run_distribution_require_registered(dist_spec);
   invariant(position > 0);
   invariant(iteration > 0);
}

u64 balancer_run_seed(const char *domain, const dist_spec_t *dist_spec,
                      u64 position, u64 iteration) {
   balancer_run_seed_require(domain, dist_spec, position, iteration);

   u64 seed = random_derive_seed(0, domain);
   seed = random_derive_seed(seed, "position");
   seed = random_derive_u64(seed, position);
   seed = random_derive_seed(seed, dist_spec->name);
   seed = random_derive_seed(seed, "iteration");
   return random_derive_u64(seed, iteration);
}

void balancer_run_grow_splay_to(splay_t *instance, u64 size) {
   invariant(instance != NULL);
   tree_sized_handle_require(instance->tree);
   invariant(instance->tree.size <= size);

   while (instance->tree.size < size) {
      splay_insert(instance, 0, 0);
      tree_sized_handle_require(instance->tree);
   }
   invariant(instance->tree.size == size);
   tree_sized_handle_require(instance->tree);
}

tree_t balancer_run_randomize_tree(tree_t tree, const dist_spec_t *dist_spec,
                                   const char *seed_domain, u64 iteration) {
   invariant(tree.size > 0);
   tree_sized_handle_require(tree);
   u64 size = tree.size;
   distribution_t *dist = dist_create(
       dist_spec, balancer_run_seed(seed_domain, dist_spec, size, iteration));
   tree = tree_randomize(tree, dist);
   dist_destroy(dist);
   invariant(tree.size == size);
   tree_sized_handle_require(tree);
   return tree;
}

void balancer_run_distribution_selection_init(
    balancer_run_distribution_selection_t *selection,
    const dist_spec_t *selected) {
   invariant(selection != NULL);
   *selection = (balancer_run_distribution_selection_t){0};

   if (selected == NULL) {
      selection->items = dist_uniform;
      selection->count = dist_uniform_count;
      invariant(selection->count > 0);
      return;
   }

   balancer_run_distribution_require_registered(selected);
   selection->selected[0] = selected;
   selection->items = selection->selected;
   selection->count = 1;
}

void balancer_run_distribution_selection_init_by_name(
    balancer_run_distribution_selection_t *selection, const char *name) {
   invariant(selection != NULL);
   if (name == NULL) {
      balancer_run_distribution_selection_init(selection, NULL);
      return;
   }

   balancer_run_distribution_selection_init(selection, dist_find_or_exit(name));
}

static void balancer_run_output_dispose(balancer_run_output_t *output) {
   invariant(output != NULL);
   staged_file_dispose(&output->file);
   free(output->dir);
   output->balancer = NULL;
   output->dir = NULL;
}

static void balancer_run_output_count_require(int count) {
   filesystem_require(count >= 0, "balancer output count is negative");
   filesystem_require(count <= balancer_registry_count,
                      "balancer output count exceeds registry");
}

static bool
balancer_run_output_balancer_registered(const balancer_t *balancer) {
   if (balancer == NULL) {
      return false;
   }
   for (int i = 0; i < balancer_registry_count; i++) {
      if (balancer == balancer_registry_all[i]) {
         return true;
      }
   }
   return false;
}

static void balancer_run_output_require(const balancer_run_output_t *output) {
   filesystem_require(output != NULL, "balancer output is null");
   filesystem_require(output->balancer != NULL,
                      "balancer output balancer is null");
   filesystem_require(balancer_run_output_balancer_registered(output->balancer),
                      "balancer output balancer is not registered");
   balancer_require(output->balancer);
   staged_file_require(&output->file);
   filesystem_require(output->dir != NULL, "balancer output directory is null");
   filesystem_require(output->dir[0] != '\0',
                      "balancer output directory is empty");

   path_require_parent_dir(output->file.path, output->dir,
                           "balancer output directory is not path parent");
}

static void
balancer_run_outputs_require_unique(const balancer_run_output_t *outputs,
                                    int count) {
   invariant(outputs != NULL);
   invariant(count > 0);
   for (int i = 0; i < count; i++) {
      balancer_run_output_require(&outputs[i]);
   }

   for (int i = 0; i < count; i++) {
      for (int j = i + 1; j < count; j++) {
         filesystem_require(outputs[i].balancer != outputs[j].balancer,
                            "duplicate balancer output");
         filesystem_require(
             strcmp(outputs[i].file.path, outputs[j].file.path) != 0,
             "duplicate balancer output path");
      }
   }
}

static void balancer_run_outputs_require(const balancer_run_output_t *outputs,
                                         int count) {
   filesystem_require(outputs != NULL, "balancer output list is null");
   balancer_run_output_count_require(count);
   filesystem_require(count > 0, "balancer output list is empty");
   balancer_run_outputs_require_unique(outputs, count);
}

void balancer_run_outputs_dispose(balancer_run_output_t *outputs, int count) {
   balancer_run_output_count_require(count);
   filesystem_require(outputs != NULL || count == 0,
                      "balancer output list is null");
   if (outputs == NULL) {
      return;
   }
   for (int i = 0; i < count; i++) {
      balancer_run_output_dispose(&outputs[i]);
   }
   free(outputs);
}

static void balancer_run_duplicate_output_path_or_exit(
    balancer_run_output_t *outputs, int count, const balancer_t *left,
    const balancer_t *right, const char *path) {
   invariant(outputs != NULL);
   invariant(count > 1);
   balancer_require(left);
   balancer_require(right);
   invariant(path != NULL);

   fprintf(stderr, "duplicate balancer output path for %s and %s: %s\n",
           left->name, right->name, path);
   balancer_run_outputs_dispose(outputs, count);
   exit(2);
}

static char *balancer_run_path_alloc(const char *output_dir, const char *kind,
                                     const balancer_t *balancer) {
   invariant(output_dir != NULL);
   invariant(output_dir[0] != '\0');
   invariant(kind != NULL);
   invariant(kind[0] != '\0');
   balancer_require(balancer);
   path_require_component(kind, "balancer output kind is not a path component");
   path_require_component(
       balancer->name, "balancer output balancer name is not a path component");

   return path_format_alloc("%s/balancers/%s/%s", output_dir, kind,
                            balancer->name);
}

static char *balancer_run_dir_alloc(const char *output_dir, const char *kind) {
   invariant(output_dir != NULL);
   invariant(output_dir[0] != '\0');
   invariant(kind != NULL);
   invariant(kind[0] != '\0');
   path_require_component(kind, "balancer output kind is not a path component");

   return path_format_alloc("%s/balancers/%s", output_dir, kind);
}

int balancer_run_outputs_make(balancer_run_output_t *outputs, int capacity,
                              const balancer_t *selected,
                              const char *output_dir, const char *kind) {
   filesystem_require(outputs != NULL, "balancer output list is null");
   filesystem_require(capacity >= 0, "balancer output capacity is negative");
   filesystem_require(output_dir != NULL, "balancer output directory is null");
   filesystem_require(output_dir[0] != '\0',
                      "balancer output directory is empty");
   filesystem_require(kind != NULL, "balancer output kind is null");
   filesystem_require(kind[0] != '\0', "balancer output kind is empty");
   path_require_component(kind, "balancer output kind is not a path component");
   if (selected) {
      filesystem_require(
          balancer_name_is_component(selected->name),
          "balancer output balancer name is not a path component");
      balancer_require(selected);
      filesystem_require(balancer_run_output_balancer_registered(selected),
                         "selected balancer is not registered");
   }
   int required_capacity = selected == NULL ? balancer_registry_count : 1;
   filesystem_require(capacity >= required_capacity,
                      "balancer output capacity is too small");

   int count = 0;
   for (int b = 0; b < balancer_registry_count; b++) {
      const balancer_t *balancer = balancer_registry_all[b];
      balancer_require(balancer);
      if (selected != NULL && selected != balancer) {
         continue;
      }

      invariant(count < capacity);
      outputs[count] = (balancer_run_output_t){
          .balancer = balancer,
          .file =
              {
                  .path = balancer_run_path_alloc(output_dir, kind, balancer),
              },
          .dir = balancer_run_dir_alloc(output_dir, kind),
      };
      balancer_run_output_require(&outputs[count]);
      for (int previous = 0; previous < count; previous++) {
         if (strcmp(outputs[count].file.path, outputs[previous].file.path) ==
             0) {
            balancer_run_duplicate_output_path_or_exit(
                outputs, count + 1, outputs[previous].balancer,
                outputs[count].balancer, outputs[count].file.path);
         }
      }
      count++;
   }

   filesystem_require(count > 0, selected != NULL
                                     ? "selected balancer is not registered"
                                     : "balancer output list is empty");
   return count;
}

void balancer_run_outputs_prepare_dirs(balancer_run_output_t *outputs,
                                       int count) {
   balancer_run_outputs_require(outputs, count);

   for (int i = 0; i < count; i++) {
      mkdir_p(outputs[i].dir);
   }
}

void balancer_run_outputs_publish(balancer_run_output_t *outputs, int count) {
   balancer_run_outputs_require(outputs, count);

   for (int i = 0; i < count; i++) {
      staged_file_require_publishable(&outputs[i].file);
   }
   for (int i = 0; i < count; i++) {
      staged_file_publish_or_exit(&outputs[i].file);
   }
}
