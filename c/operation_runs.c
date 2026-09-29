#include "operation_runs.h"

#include "benchmark_order.h"
#include "bst.h"
#include "filesystem.h"
#include "random.h"
#include "sampling.h"
#include "time_utils.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static void operation_run_require_plan(const operation_run_t *run) {
   invariant(run != NULL);
   operation_require(run->op);
   list_vtable_require(run->tree_vt);
   invariant(run->scale > 0);
   invariant(run->steps > 0);
   invariant(run->iterations > 0);
   invariant(run->op->scale == run->scale);
   invariant(run->op->steps == run->steps);
}

static void operation_run_require_file(const operation_run_t *run) {
   operation_run_require_plan(run);
   invariant(run->file != NULL);
   invariant(run->path != NULL);
}

static void operation_run_require_distribution(const operation_run_t *run) {
   operation_run_require_file(run);
   dist_spec_require(run->dist);
   invariant(run->iteration > 0);
   invariant(run->iteration <= run->iterations);
}

static void operation_run_require_workload(const operation_run_t *run) {
   operation_run_require_plan(run);
   dist_spec_require(run->dist);
   invariant(run->iteration > 0);
   invariant(run->iteration <= run->iterations);
}

static void operation_run_require_workload_seed(const operation_run_t *run,
                                                const char *domain) {
   operation_run_require_workload(run);
   invariant(domain != NULL);
   invariant(domain[0] != '\0');
}

void operation_run_start_chunk_timer(operation_run_t *run) {
   invariant(run != NULL);
   run->chunk_start_ns = monotonic_time_ns();
   run->chunk_duration_ns = 0;
   run->chunk_timer_started = true;
   run->chunk_duration_known = false;
}

void operation_run_stop_chunk_timer(operation_run_t *run) {
   invariant(run != NULL);
   if (!run->chunk_timer_started || run->chunk_duration_known) {
      return;
   }
   run->chunk_duration_ns = elapsed_time_ns_since_nonzero(run->chunk_start_ns);
   run->chunk_duration_known = true;
}

u64 operation_run_chunk_duration(operation_run_t *run) {
   invariant(run != NULL);
   operation_run_stop_chunk_timer(run);
   invariant(run->chunk_timer_started);
   invariant(run->chunk_duration_known);
   invariant(run->chunk_duration_ns > 0);
   return run->chunk_duration_ns;
}

u64 operation_run_workload_seed(const operation_run_t *run,
                                const char *domain) {
   operation_run_require_workload_seed(run, domain);

   u64 seed = random_derive_seed(0, domain);
   seed = random_derive_seed(seed, run->op->name);
   seed = random_derive_seed(seed, "scale");
   seed = random_derive_u64(seed, run->scale);
   seed = random_derive_seed(seed, "steps");
   seed = random_derive_u64(seed, run->steps);
   seed = random_derive_seed(seed, run->dist->name);
   seed = random_derive_seed(seed, "iteration");
   return random_derive_u64(seed, run->iteration);
}

static u64 default_seed(const operation_run_t *run) {
   return operation_run_workload_seed(run, "operation-runs");
}

static void operation_run_header_require(const char *header) {
   if (header == NULL) {
      return;
   }
   filesystem_require(header[0] != '\0', "operation output header is empty");
   filesystem_require(strchr(header, '\n') == NULL &&
                          strchr(header, '\r') == NULL,
                      "operation output header contains newline");
}

sample_schedule_t operation_run_sample_schedule(u64 steps, u64 samples) {
   sample_schedule_t schedule = sample_schedule_make(steps, samples);
   sample_schedule_require(schedule);
   filesystem_require(schedule.total < UINT64_MAX,
                      "operation step count is too large");
   return schedule;
}

static void operation_run_require_instance(const operation_run_t *run) {
   operation_run_require_distribution(run);
   invariant(run->instance.impl != NULL);
   invariant(run->instance.vt == run->tree_vt);
   invariant(run->step_count <= run->steps);
}

static u64 operation_run_instance_size(const operation_run_t *run) {
   operation_run_require_instance(run);
   list_t instance = run->instance;
   return list_size(&instance);
}

#ifdef DEBUG
static char *operation_run_string_snapshot(const char *text) {
   return text == NULL ? NULL : path_format_alloc("%s", text);
}

static off_t operation_run_file_size(FILE *file) {
   invariant(file != NULL);
   int fd = fileno(file);
   invariant(fd >= 0);
   struct stat info;
   invariant(fstat(fd, &info) == 0);
   invariant(info.st_size >= 0);
   return info.st_size;
}

typedef struct {
   node_t *node;
   node_t *left;
   node_t *right;
   u64 size;
   u64 value;
   u64 rank;
#ifdef PERSISTENT
   u64 refs;
#endif
} operation_run_node_snapshot_t;

typedef struct {
   u64 count;
   u64 expected;
   u64 *values;
   bool has_tree;
   node_t *tree_root;
   u64 tree_size;
   u64 node_count;
   u64 node_cap;
   operation_run_node_snapshot_t *nodes;
} operation_run_sequence_t;

static void operation_run_sequence_visit(u64 value, void *ctx) {
   operation_run_sequence_t *seq = (operation_run_sequence_t *)ctx;
   invariant(seq != NULL);
   invariant(seq->values != NULL);
   invariant(seq->count < seq->expected);

   u64 index = seq->count;
   seq->count = size_after_inserting_one(seq->count);
   seq->values[index] = value;
}

static void operation_run_sequence_append_node(operation_run_sequence_t *seq,
                                               node_t *node) {
   invariant(seq != NULL);
   invariant(node != NULL);
   invariant(seq->node_count <= seq->node_cap);
   if (seq->node_count == seq->node_cap) {
      u64 new_cap = bst_array_next_cap(
          seq->node_cap, size_after_inserting_one(seq->node_count), 8,
          sizeof(seq->nodes[0]));
      seq->nodes = seq->nodes == NULL
                       ? (operation_run_node_snapshot_t *)bst_malloc_array(
                             new_cap, sizeof(seq->nodes[0]))
                       : (operation_run_node_snapshot_t *)bst_realloc_array(
                             seq->nodes, new_cap, sizeof(seq->nodes[0]));
      seq->node_cap = new_cap;
   }
   invariant(seq->nodes != NULL);

   seq->nodes[seq->node_count] = (operation_run_node_snapshot_t){
       .node = node,
       .left = node->l,
       .right = node->r,
       .size = node->s,
       .value = node->x,
       .rank = node->y,
#ifdef PERSISTENT
       .refs = node->refs,
#endif
   };
   seq->node_count = size_after_inserting_one(seq->node_count);
}

static u64 operation_run_node_snapshot_initial_cap(u64 logical_size) {
   invariant(logical_size > 0);
   u64 max_cap = bst_array_max_count(sizeof(operation_run_node_snapshot_t));
   invariant(logical_size <= max_cap);

   if (logical_size <= bst_full_binary_tree_max_leaves_for_nodes(max_cap)) {
      return bst_full_binary_tree_nodes_from_leaves(logical_size);
   }

   return logical_size;
}

static void operation_run_capture_tree_nodes(tree_t *tree,
                                             operation_run_sequence_t *seq) {
   invariant(tree != NULL);
   invariant(seq != NULL);
   invariant(seq->has_tree);
   invariant(tree->size == seq->expected);
   tree_handle_require(*tree);

   seq->tree_root = tree->root;
   seq->tree_size = tree->size;
   if (tree->size == 0) {
      return;
   }
   u64 node_cap = operation_run_node_snapshot_initial_cap(tree->size);
   seq->nodes = (operation_run_node_snapshot_t *)bst_malloc_array(
       node_cap, sizeof(seq->nodes[0]));
   seq->node_cap = node_cap;

   node_t *inline_data[NODE_STACK_INLINE_CAP];
   node_stack_t stack = {
       .data = inline_data,
       .len = 0,
       .cap = NODE_STACK_INLINE_CAP,
   };
   node_seen_t seen = {0};

   node_stack_push(&stack, inline_data, tree->root);
   while (stack.len > 0) {
      node_t *node = node_stack_pop(&stack);
      node_seen_insert(&seen, node);
      operation_run_sequence_append_node(seq, node);
      node_stack_push(&stack, inline_data, node->r);
      node_stack_push(&stack, inline_data, node->l);
   }

   node_stack_free(&stack, inline_data);
   node_seen_dispose(&seen);
   // Conc-style trees can have internal nodes in addition to logical leaves.
   invariant(seq->node_count >= tree->size);
}

static operation_run_sequence_t
operation_run_instance_sequence(const operation_run_t *run) {
   invariant(run != NULL);
   if (run->instance.impl == NULL) {
      invariant(run->instance.vt == NULL);
      return (operation_run_sequence_t){0};
   }

   operation_run_require_instance(run);
   list_t instance = run->instance;
   u64 size = list_size(&instance);
   operation_run_sequence_t seq = {.count = 0, .expected = size};
   if (size > 0) {
      seq.values = (u64 *)bst_malloc_array(size, sizeof(seq.values[0]));
      list_each(&instance, operation_run_sequence_visit, &seq);
   }
   invariant(seq.count == size);

   tree_t *tree = list_tree(&instance);
   if (tree != NULL) {
      seq.has_tree = true;
      operation_run_capture_tree_nodes(tree, &seq);
   }
   return seq;
}

#endif

static u64 operation_run_chunk_step(const operation_run_t *run, u64 completed,
                                    u64 target) {
   operation_run_require_instance(run);
   invariant(completed < target);
   invariant(target <= run->steps);
   return sequence_split_inputs_require(completed, target);
}

static void operation_run_require_pending_chunk(const operation_run_t *run,
                                                u64 completed) {
   operation_run_require_instance(run);
   invariant(run->step_count == completed);
   invariant(run->position > completed);
   invariant(run->step ==
             operation_run_chunk_step(run, completed, run->position));
}

static void operation_run_require_completed_chunk(const operation_run_t *run) {
   operation_run_require_instance(run);
   invariant(run->step > 0);
   invariant(run->position > 0);
   invariant(run->position <= run->steps);
   invariant(run->step_count == run->position);
}

typedef struct {
   const operation_t *op;
   operation_type_t op_type;
   const char *op_name;
   u64 op_scale;
   u64 op_steps;
   const list_vtable_t *tree_vt;
   const dist_spec_t *dist;
   FILE *file;
   const char *path;
   void *instance_impl;
   const list_vtable_t *instance_vt;
   u64 scale;
   u64 steps;
   u64 step;
   u64 position;
   u64 step_count;
   u64 iteration;
   u64 iterations;
   void *ctx;
#ifdef DEBUG
   char *op_name_snapshot;
   char *tree_name_snapshot;
   char *dist_name_snapshot;
   char *path_snapshot;
   long file_position;
   off_t file_size;
   u64 instance_count;
   u64 *instance_values;
   bool instance_has_tree;
   node_t *instance_tree_root;
   u64 instance_tree_size;
   u64 instance_node_count;
   operation_run_node_snapshot_t *instance_nodes;
#endif
} operation_run_observer_state_t;

static operation_run_observer_state_t
operation_run_observer_state(const operation_run_t *run) {
   invariant(run != NULL);
   operation_require(run->op);
   operation_run_observer_state_t state = {
       .op = run->op,
       .op_type = run->op->type,
       .op_name = run->op->name,
       .op_scale = run->op->scale,
       .op_steps = run->op->steps,
       .tree_vt = run->tree_vt,
       .dist = run->dist,
       .file = run->file,
       .path = run->path,
       .instance_impl = run->instance.impl,
       .instance_vt = run->instance.vt,
       .scale = run->scale,
       .steps = run->steps,
       .step = run->step,
       .position = run->position,
       .step_count = run->step_count,
       .iteration = run->iteration,
       .iterations = run->iterations,
       .ctx = run->ctx,
   };
#ifdef DEBUG
   state.op_name_snapshot = operation_run_string_snapshot(run->op->name);
   state.tree_name_snapshot = operation_run_string_snapshot(
       run->tree_vt == NULL ? NULL : run->tree_vt->name);
   state.dist_name_snapshot = operation_run_string_snapshot(
       run->dist == NULL ? NULL : run->dist->name);
   state.path_snapshot = operation_run_string_snapshot(run->path);
   if (run->file == NULL) {
      state.file_position = -1;
      state.file_size = -1;
   } else {
      state.file_position = ftell(run->file);
      invariant(state.file_position >= 0);
      state.file_size = operation_run_file_size(run->file);
   }
   operation_run_sequence_t seq = operation_run_instance_sequence(run);
   state.instance_count = seq.count;
   state.instance_values = seq.values;
   state.instance_has_tree = seq.has_tree;
   state.instance_tree_root = seq.tree_root;
   state.instance_tree_size = seq.tree_size;
   state.instance_node_count = seq.node_count;
   state.instance_nodes = seq.nodes;
#endif
   return state;
}

#ifdef DEBUG
static void
operation_run_observer_state_dispose(operation_run_observer_state_t *state) {
   invariant(state != NULL);
   free(state->op_name_snapshot);
   free(state->tree_name_snapshot);
   free(state->dist_name_snapshot);
   free(state->path_snapshot);
   free(state->instance_values);
   free(state->instance_nodes);
   state->op_name_snapshot = NULL;
   state->tree_name_snapshot = NULL;
   state->dist_name_snapshot = NULL;
   state->path_snapshot = NULL;
   state->file_position = -1;
   state->file_size = -1;
   state->instance_count = 0;
   state->instance_values = NULL;
   state->instance_has_tree = false;
   state->instance_tree_root = NULL;
   state->instance_tree_size = 0;
   state->instance_node_count = 0;
   state->instance_nodes = NULL;
}

static void
operation_run_require_same_nodes(const operation_run_observer_state_t *after,
                                 const operation_run_observer_state_t *before) {
   invariant(after != NULL);
   invariant(before != NULL);
   invariant(after->instance_has_tree == before->instance_has_tree);
   if (!before->instance_has_tree) {
      invariant(before->instance_tree_root == NULL);
      invariant(after->instance_tree_root == NULL);
      invariant(before->instance_tree_size == 0);
      invariant(after->instance_tree_size == 0);
      invariant(before->instance_node_count == 0);
      invariant(after->instance_node_count == 0);
      invariant(before->instance_nodes == NULL);
      invariant(after->instance_nodes == NULL);
      return;
   }

   invariant(after->instance_tree_root == before->instance_tree_root);
   invariant(after->instance_tree_size == before->instance_tree_size);
   invariant(after->instance_node_count == before->instance_node_count);
   invariant((after->instance_nodes == NULL) ==
             (after->instance_node_count == 0));
   invariant((before->instance_nodes == NULL) ==
             (before->instance_node_count == 0));
   for (u64 i = 0; i < before->instance_node_count; i++) {
      const operation_run_node_snapshot_t *a = &after->instance_nodes[i];
      const operation_run_node_snapshot_t *b = &before->instance_nodes[i];
      invariant(a->node == b->node);
      invariant(a->left == b->left);
      invariant(a->right == b->right);
      invariant(a->size == b->size);
      invariant(a->value == b->value);
      invariant(a->rank == b->rank);
#ifdef PERSISTENT
      invariant(a->refs == b->refs);
#endif
   }
}

static void operation_run_require_same_snapshot(const char *after,
                                                const char *before) {
   invariant((after == NULL) == (before == NULL));
   if (before != NULL) {
      invariant(strcmp(after, before) == 0);
   }
}
#endif

static void
operation_run_require_observer_state(const operation_run_t *run,
                                     operation_run_observer_state_t before,
                                     bool require_file_position) {
   operation_run_observer_state_t after = operation_run_observer_state(run);
   invariant(after.op == before.op);
   invariant(after.op_type == before.op_type);
   invariant(after.op_name == before.op_name);
   invariant(after.op_scale == before.op_scale);
   invariant(after.op_steps == before.op_steps);
   invariant(after.tree_vt == before.tree_vt);
   invariant(after.dist == before.dist);
   invariant(after.file == before.file);
   invariant(after.path == before.path);
   invariant(after.instance_impl == before.instance_impl);
   invariant(after.instance_vt == before.instance_vt);
   invariant(after.scale == before.scale);
   invariant(after.steps == before.steps);
   invariant(after.step == before.step);
   invariant(after.position == before.position);
   invariant(after.step_count == before.step_count);
   invariant(after.iteration == before.iteration);
   invariant(after.iterations == before.iterations);
   invariant(after.ctx == before.ctx);
#ifdef DEBUG
   operation_run_require_same_snapshot(after.op_name_snapshot,
                                       before.op_name_snapshot);
   operation_run_require_same_snapshot(after.tree_name_snapshot,
                                       before.tree_name_snapshot);
   operation_run_require_same_snapshot(after.dist_name_snapshot,
                                       before.dist_name_snapshot);
   operation_run_require_same_snapshot(after.path_snapshot,
                                       before.path_snapshot);
   if (require_file_position) {
      invariant(after.file_position == before.file_position);
      invariant(after.file_size == before.file_size);
   }
   invariant(after.instance_count == before.instance_count);
   invariant((after.instance_values == NULL) == (after.instance_count == 0));
   invariant((before.instance_values == NULL) == (before.instance_count == 0));
   for (u64 i = 0; i < before.instance_count; i++) {
      invariant(after.instance_values[i] == before.instance_values[i]);
   }
   operation_run_require_same_nodes(&after, &before);
   operation_run_observer_state_dispose(&after);
   operation_run_observer_state_dispose(&before);
#else
   (void)require_file_position;
#endif
}

static u64 operation_run_tree_seed(const operation_run_t *run, u64 base_seed) {
   operation_run_require_distribution(run);

   u64 seed = random_derive_seed(base_seed, run->op->name);
   seed = random_derive_seed(seed, run->tree_vt->name);
   seed = random_derive_seed(seed, run->dist->name);
   return random_derive_seed(seed, "tree");
}

static char *
operation_run_default_path_alloc(const operation_run_t *run,
                                 const operation_run_config_t *config) {
   operation_run_require_plan(run);
   invariant(config != NULL);
   invariant(config->output_dir != NULL);
   invariant(config->kind != NULL);
   path_require_component(config->kind,
                          "operation output kind is not a path component");
   path_require_component(
       run->op->name,
       "operation output operation name is not a path component");
   path_require_component(run->tree_vt->name,
                          "operation output tree name is not a path component");
   return path_format_alloc("%s/operations/%s/%s/%s", config->output_dir,
                            config->kind, run->op->name, run->tree_vt->name);
}

static char *
operation_run_default_dir_alloc(const operation_run_t *run,
                                const operation_run_config_t *config) {
   operation_run_require_plan(run);
   invariant(config != NULL);
   invariant(config->output_dir != NULL);
   invariant(config->kind != NULL);
   path_require_component(config->kind,
                          "operation output kind is not a path component");
   path_require_component(
       run->op->name,
       "operation output operation name is not a path component");
   return path_format_alloc("%s/operations/%s/%s", config->output_dir,
                            config->kind, run->op->name);
}

static char *operation_run_path_buffer_copy(const char *label, const char *buf,
                                            size_t size) {
   filesystem_require(label != NULL, "operation path label is null");
   filesystem_require(buf != NULL, "operation path buffer is null");
   filesystem_require(size > 0, "operation path buffer is empty");
   filesystem_require(memchr(buf, '\0', size) != NULL,
                      "operation path buffer is not terminated");
   filesystem_require(buf[0] != '\0', label);
   return path_format_alloc("%s", buf);
}

typedef struct {
   staged_file_t file;
   char *dir;
} operation_run_output_t;

static void operation_run_output_dispose(operation_run_output_t *output) {
   invariant(output != NULL);
   staged_file_dispose(&output->file);
   free(output->dir);
   output->dir = NULL;
}

static void operation_run_output_require(const operation_run_output_t *output) {
   filesystem_require(output != NULL, "operation output is null");
   staged_file_require(&output->file);
   filesystem_require(output->dir != NULL,
                      "operation output directory is null");
   filesystem_require(output->dir[0] != '\0',
                      "operation output directory is empty");

   path_require_parent_dir(output->file.path, output->dir,
                           "operation output directory is not path parent");
}

static void operation_run_outputs_dispose(operation_run_output_t *outputs,
                                          int count) {
   filesystem_require(count >= 0, "operation output count is negative");
   filesystem_require(outputs != NULL || count == 0,
                      "operation output list is null");
   if (outputs == NULL) {
      return;
   }
   for (int i = 0; i < count; i++) {
      operation_run_output_dispose(&outputs[i]);
   }
   free(outputs);
}

static void operation_run_outputs_require_unique_paths(
    const operation_run_output_t *outputs, int count) {
   invariant(outputs != NULL);
   invariant(count > 0);
   for (int i = 0; i < count; i++) {
      operation_run_output_require(&outputs[i]);
   }

   for (int i = 0; i < count; i++) {
      for (int j = i + 1; j < count; j++) {
         filesystem_require(
             strcmp(outputs[i].file.path, outputs[j].file.path) != 0,
             "duplicate operation output path");
      }
   }
}

static void operation_run_outputs_require(const operation_run_output_t *outputs,
                                          int count) {
   filesystem_require(outputs != NULL, "operation output list is null");
   filesystem_require(count > 0, "operation output list is empty");
   operation_run_outputs_require_unique_paths(outputs, count);
}

static void operation_run_outputs_prepare_dirs(operation_run_output_t *outputs,
                                               int count) {
   operation_run_outputs_require(outputs, count);
   for (int i = 0; i < count; i++) {
      mkdir_p(outputs[i].dir);
   }
}

static void operation_run_outputs_publish(operation_run_output_t *outputs,
                                          int count) {
   operation_run_outputs_require(outputs, count);
   for (int i = 0; i < count; i++) {
      staged_file_require_publishable(&outputs[i].file);
   }
   for (int i = 0; i < count; i++) {
      staged_file_publish_or_exit(&outputs[i].file);
   }
}

#ifdef BST_FAST_BENCHMARK
typedef struct {
   u64 target;
   u64 step;
   u64 size_before;
   u64 size_after;
   operation_position_chunk_t chunk;
} operation_run_fast_sample_t;

static inline void operation_run_fast_set_chunk_duration(operation_run_t *run,
                                                         u64 duration) {
   invariant(run != NULL);
   invariant(duration > 0);
   run->chunk_duration_ns = duration;
   run->chunk_timer_started = true;
   run->chunk_duration_known = true;
}

static u64
operation_run_position_chunk_size_after(u64 size_before,
                                        operation_position_chunk_t chunk) {
   u64 size = size_sum(size_before, chunk.insert_count);
   if (chunk.insert_delete_count % 2 != 0) {
      if (chunk.insert_delete_next_is_insert) {
         size = size_after_inserting_one(size);
      } else {
         size = size_after_removing_one(size);
      }
   }
   return size;
}

static operation_run_fast_sample_t *
operation_run_prepare_fast_samples(const operation_t *op,
                                   sample_schedule_t schedule) {
   operation_require(op);
   sample_schedule_require(schedule);

   operation_run_fast_sample_t *samples =
       (operation_run_fast_sample_t *)bst_malloc_array(schedule.samples,
                                                       sizeof(samples[0]));
   u64 completed = 0;
   u64 size = 0;
   for (u64 sample = 0; sample < schedule.samples; sample++) {
      sample_schedule_point_t point =
          sample_schedule_next_point(schedule, sample, completed);
      u64 step = sequence_split_inputs_require(completed, point.position);
      operation_position_chunk_t chunk =
          operation_position_chunk_plan(op, completed, step);
      u64 size_after = operation_run_position_chunk_size_after(size, chunk);
      samples[sample] = (operation_run_fast_sample_t){
          .target = point.position,
          .step = step,
          .size_before = size,
          .size_after = size_after,
          .chunk = chunk,
      };
      completed = point.position;
      size = size_after;
   }

   invariant(completed == schedule.total);
   return samples;
}

static u64 *operation_run_prepare_position_stream(
    const operation_t *op, distribution_t *dist,
    const operation_run_fast_sample_t *samples, u64 sample_count) {
   operation_require(op);
   invariant(dist != NULL);
   invariant(samples != NULL);
   invariant(sample_count > 0);

   u64 *positions = (u64 *)bst_malloc_array(samples[sample_count - 1].target,
                                            sizeof(positions[0]));
   bool inserts_only = operation_inserts_only(op);
   u64 completed = 0;
   for (u64 sample = 0; sample < sample_count; sample++) {
      const operation_run_fast_sample_t *plan = &samples[sample];
      invariant(plan->target > completed);
      if (inserts_only) {
         u64 prepared_size = operation_prepare_insert_positions(
             op, dist, plan->size_before, completed, &positions[completed],
             plan->step);
         invariant(prepared_size == plan->size_after);
      } else {
         u64 prepared_size = operation_prepare_insert_delete_positions(
             op, dist, plan->size_before, completed, &positions[completed],
             plan->step);
         invariant(prepared_size == plan->size_after);
      }
      completed = plan->target;
   }

   return positions;
}

static FILE **operation_run_open_output_files(operation_run_output_t *outputs,
                                              int count, const char *header) {
   operation_run_outputs_require(outputs, count);
   FILE **files = (FILE **)bst_calloc_array((u64)count, sizeof(files[0]));
   for (int t = 0; t < count; t++) {
      const char *path = outputs[t].file.path;
      FILE *file = staged_file_open_or_exit(&outputs[t].file, "w");
      files[t] = file;
      if (header) {
         file_printf_or_exit(file, path, "%s\n", header);
      }
   }
   return files;
}

static void operation_run_finish_output_files(operation_run_output_t *outputs,
                                              FILE **files, int count) {
   operation_run_outputs_require(outputs, count);
   invariant(files != NULL);
   for (int t = 0; t < count; t++) {
      invariant(files[t] != NULL);
      staged_file_finish_or_exit(&outputs[t].file, files[t]);
      files[t] = NULL;
   }
   free(files);
}

static operation_run_t
operation_run_make_fast(const operation_t *op,
                        const operation_run_config_t *config,
                        operation_run_output_t *outputs, FILE **files,
                        int tree_index, const dist_spec_t *dist, u64 iteration,
                        u64 scale, u64 steps, u64 iterations, void *ctx) {
   invariant(config != NULL);
   operation_run_outputs_require(outputs, config->tree_count);
   invariant(files != NULL);
   invariant(tree_index >= 0);
   invariant(tree_index < config->tree_count);
   dist_spec_require(dist);

   operation_run_t run = {
       .op = op,
       .tree_vt = config->trees[tree_index],
       .dist = dist,
       .file = files[tree_index],
       .path = outputs[tree_index].file.path,
       .scale = scale,
       .steps = steps,
       .iteration = iteration,
       .iterations = iterations,
       .ctx = ctx,
   };
   operation_run_require_distribution(&run);
   return run;
}

static void operation_run_matrix_fast(
    const operation_t *op, const operation_run_config_t *config,
    sample_schedule_t schedule, u64 scale, u64 steps, u64 samples,
    u64 iterations, u64 (*seed)(const operation_run_t *run),
    operation_run_output_t *outputs, arena_t *arena,
    operation_run_hook_t progress, operation_run_hook_t before_chunk,
    operation_run_hook_t after_chunk, void *ctx) {
   operation_require(op);
   invariant(config != NULL);
   sample_schedule_require(schedule);
   operation_run_outputs_require(outputs, config->tree_count);
   invariant(arena != NULL);
   invariant(arena_is_live(arena));
   invariant(seed != NULL);

   FILE **files = operation_run_open_output_files(outputs, config->tree_count,
                                                  config->header);
   operation_run_fast_sample_t *fast_samples =
       operation_run_prepare_fast_samples(op, schedule);

   for (u64 iteration = 0; iteration < iterations; iteration++) {
      u64 run_iteration = operation_run_next_iteration(iteration);
      for (int d = 0; d < config->distribution_count; d++) {
         const dist_spec_t *dist_spec = config->distributions[d];
         dist_spec_require(dist_spec);

         operation_run_t seed_run = operation_run_make_fast(
             op, config, outputs, files, 0, dist_spec, run_iteration, scale,
             steps, iterations, ctx);
         u64 run_seed = seed(&seed_run);
         distribution_t *dist = dist_create(dist_spec, run_seed);
         u64 *positions = operation_run_prepare_position_stream(
             op, dist, fast_samples, samples);
         dist_destroy(dist);

         u64 period =
             benchmark_balanced_latin_square_period(config->tree_count);
         u64 trial = (iteration % period) *
                     ((u64)config->distribution_count % period) % period;
         trial = (trial + (u64)d) % period;
         for (int order = 0; order < config->tree_count; order++) {
            int t = benchmark_balanced_latin_square_index(config->tree_count,
                                                          trial, order);
            operation_run_t run = operation_run_make_fast(
                op, config, outputs, files, t, dist_spec, run_iteration, scale,
                steps, iterations, ctx);

            if (progress) {
               progress(&run);
            }

            random_seed(operation_run_tree_seed(&run, run_seed));
            arena_reset(arena);

            run.instance = list_new(run.tree_vt);
            const list_vtable_t *tree_vt = run.tree_vt;
            list_time_inserts_fn time_inserts_fn = tree_vt->time_inserts_fn;
            list_time_insert_delete_fn time_insert_delete_fn =
                tree_vt->time_insert_delete_fn;
            run.step_count = 0;
            operation_run_require_instance(&run);

            u64 completed = 0;
            for (u64 sample = 0; sample < samples; sample++) {
               const operation_run_fast_sample_t *plan = &fast_samples[sample];
               u64 target = plan->target;
               run.step = plan->step;
               run.position = target;
               operation_run_require_pending_chunk(&run, completed);

               run.instance_size_known = true;
               run.instance_size = plan->size_before;
#ifndef BST_DISABLE_INVARIANTS
               u64 before_size = operation_run_instance_size(&run);
               invariant(before_size == plan->size_before);
#endif
               const u64 *chunk_positions = &positions[completed];
               const u64 *insert_delete_positions =
                   chunk_positions + plan->chunk.insert_count;
               u64 insert_count = plan->chunk.insert_count;
               u64 insert_delete_count = plan->chunk.insert_delete_count;
               bool next_is_insert = plan->chunk.insert_delete_next_is_insert;
               u64 duration;

               if (insert_count > 0 && insert_delete_count > 0) {
                  if (before_chunk) {
                     before_chunk(&run);
                  }
                  duration = time_insert_delete_fn(
                      &run.instance, chunk_positions, insert_count,
                      insert_delete_count, next_is_insert);
               } else if (insert_count > 0) {
                  if (before_chunk) {
                     before_chunk(&run);
                  }
                  duration = time_inserts_fn(&run.instance, chunk_positions,
                                             insert_count);
               } else {
                  invariant(insert_delete_count > 0);
                  if (before_chunk) {
                     before_chunk(&run);
                  }
                  duration = time_insert_delete_fn(
                      &run.instance, insert_delete_positions, 0,
                      insert_delete_count, next_is_insert);
               }
               operation_run_fast_set_chunk_duration(&run, duration);

               run.instance_size_known = true;
               run.instance_size = plan->size_after;
               if (after_chunk) {
                  after_chunk(&run);
               }
               run.step_count = target;
               operation_run_require_completed_chunk(&run);
#ifndef BST_DISABLE_INVARIANTS
               u64 after_size = operation_run_instance_size(&run);
               invariant(after_size == plan->size_after);
#endif
               completed = target;
            }
            invariant(completed == steps);
            invariant(run.step_count == steps);
            list_free(&run.instance);
         }

         free(positions);
      }
   }

   free(fast_samples);
   operation_run_finish_output_files(outputs, files, config->tree_count);
}
#endif

static void operation_run_duplicate_output_path_or_exit(
    operation_run_output_t *outputs, int initialized_count,
    const list_vtable_t *left_tree, const list_vtable_t *right_tree,
    const char *path) {
   invariant(outputs != NULL);
   invariant(initialized_count > 0);
   list_vtable_require(left_tree);
   list_vtable_require(right_tree);
   invariant(path != NULL);

   fprintf(stderr, "duplicate operation output path for %s and %s: %s\n",
           left_tree->name, right_tree->name, path);
   operation_run_outputs_dispose(outputs, initialized_count);
   exit(2);
}

static void
operation_run_duplicate_distribution_or_exit(const dist_spec_t *left,
                                             const dist_spec_t *right) {
   dist_spec_require(left);
   dist_spec_require(right);
   invariant(strcmp(left->name, right->name) == 0);

   fprintf(stderr, "duplicate operation distribution: %s\n", left->name);
   exit(2);
}

static void operation_run_duplicate_tree_or_exit(const list_vtable_t *left,
                                                 const list_vtable_t *right) {
   list_vtable_require(left);
   list_vtable_require(right);
   invariant(strcmp(left->name, right->name) == 0);

   fprintf(stderr, "duplicate operation tree: %s\n", left->name);
   exit(2);
}

static operation_run_output_t
operation_run_output_make(const operation_run_t *run,
                          const operation_run_config_t *config) {
   operation_run_require_plan(run);
   invariant(config != NULL);

   operation_run_output_t output = {0};
   if (config->path) {
      char path_buf[512] = {0};
      char dir_buf[512] = {0};
      operation_run_observer_state_t state = operation_run_observer_state(run);
      config->path(run, config, path_buf, sizeof(path_buf), dir_buf,
                   sizeof(dir_buf));
      operation_run_require_observer_state(run, state, false);
      output.file.path = operation_run_path_buffer_copy(
          "operation output path is empty", path_buf, sizeof(path_buf));
      output.dir = operation_run_path_buffer_copy(
          "operation output directory is empty", dir_buf, sizeof(dir_buf));
   } else {
      output.file.path = operation_run_default_path_alloc(run, config);
      output.dir = operation_run_default_dir_alloc(run, config);
   }
   invariant(output.file.path != NULL);
   invariant(output.dir != NULL);
   operation_run_output_require(&output);
   return output;
}

void operation_run_matrix(const operation_t *op,
                          const operation_run_config_t *config,
                          operation_run_hook_t progress,
                          operation_run_hook_t before_chunk,
                          operation_run_hook_t after_chunk, void *ctx) {
   filesystem_require(op != NULL, "operation is null");
   filesystem_require(op->name != NULL, "operation name is null");
   filesystem_require(
       operation_name_is_component(op->name),
       "operation output operation name is not a path component");
   operation_require(op);
   filesystem_require(config != NULL, "operation run config is null");
   filesystem_require(config->output_dir != NULL,
                      "operation output directory is null");
   filesystem_require(config->output_dir[0] != '\0',
                      "operation output directory is empty");
   filesystem_require(config->kind != NULL, "operation output kind is null");
   filesystem_require(config->kind[0] != '\0',
                      "operation output kind is empty");
   operation_run_header_require(config->header);
   path_require_component(config->kind,
                          "operation output kind is not a path component");
   filesystem_require(config->trees != NULL, "operation tree list is null");
   filesystem_require(config->tree_count >= 0,
                      "operation tree count is negative");
   filesystem_require(config->tree_count > 0, "operation tree list is empty");
   filesystem_require(config->distributions != NULL,
                      "operation distribution list is null");
   filesystem_require(config->distribution_count >= 0,
                      "operation distribution count is negative");
   filesystem_require(config->distribution_count > 0,
                      "operation distribution list is empty");
   for (int t = 0; t < config->tree_count; t++) {
      filesystem_require(config->trees[t] != NULL, "operation tree is null");
      filesystem_require(list_vtable_name_is_component(config->trees[t]->name),
                         "operation output tree name is not a path component");
      list_vtable_require(config->trees[t]);
      for (int previous = 0; previous < t; previous++) {
         if (strcmp(config->trees[t]->name, config->trees[previous]->name) ==
             0) {
            operation_run_duplicate_tree_or_exit(config->trees[previous],
                                                 config->trees[t]);
         }
      }
   }
   for (int d = 0; d < config->distribution_count; d++) {
      filesystem_require(config->distributions[d] != NULL,
                         "operation distribution is null");
      filesystem_require(
          dist_name_is_data_token(config->distributions[d]->name),
          "operation distribution name is not a printable token");
      dist_spec_require(config->distributions[d]);
      for (int previous = 0; previous < d; previous++) {
         if (strcmp(config->distributions[d]->name,
                    config->distributions[previous]->name) == 0) {
            operation_run_duplicate_distribution_or_exit(
                config->distributions[previous], config->distributions[d]);
         }
      }
   }

   sample_schedule_t schedule =
       operation_run_sample_schedule(op->steps, config->samples);
   u64 steps = schedule.total;
   u64 scale = operation_effective_scale_value(op->scale);
   u64 samples = schedule.samples;
   u64 arena_cap = config->arena_cap == 0 ? 1 : config->arena_cap;
   u64 iterations = config->iterations == 0 ? 1 : config->iterations;
   filesystem_require(arena_cap <= arena_max_capacity(),
                      "operation arena capacity is too large");
   filesystem_require(iterations < UINT64_MAX,
                      "operation iteration count is too large");
   invariant(steps > 0);
   invariant(steps < UINT64_MAX);
   invariant(scale > 0);
   invariant(samples > 0);
   invariant(samples <= steps);
   invariant(arena_cap > 0);
   invariant(iterations > 0);
   invariant(iterations < UINT64_MAX);
   operation_t run_op = *op;
   run_op.scale = scale;
   run_op.steps = steps;
   operation_require(&run_op);
   u64 (*seed)(const operation_run_t *run) =
       config->seed ? config->seed : default_seed;

   operation_run_output_t *outputs = (operation_run_output_t *)bst_calloc_array(
       (u64)config->tree_count, sizeof(outputs[0]));
   for (int t = 0; t < config->tree_count; t++) {
      operation_run_t output_run = {
          .op = &run_op,
          .tree_vt = config->trees[t],
          .scale = scale,
          .steps = steps,
          .iterations = iterations,
          .ctx = ctx,
      };
      outputs[t] = operation_run_output_make(&output_run, config);
      for (int previous = 0; previous < t; previous++) {
         if (strcmp(outputs[t].file.path, outputs[previous].file.path) == 0) {
            operation_run_duplicate_output_path_or_exit(
                outputs, t + 1, config->trees[previous], config->trees[t],
                outputs[t].file.path);
         }
      }
   }
   operation_run_outputs_prepare_dirs(outputs, config->tree_count);

   arena_t arena;
   arena_init(&arena, arena_cap);
   arena_t *previous_arena = arena_install(&arena);
   if (config->prewarm_arena) {
      arena_prewarm(&arena);
   }

#ifdef BST_FAST_BENCHMARK
   operation_run_matrix_fast(&run_op, config, schedule, scale, steps, samples,
                             iterations, seed, outputs, &arena, progress,
                             before_chunk, after_chunk, ctx);
#else
   for (int t = 0; t < config->tree_count; t++) {
      list_vtable_require(config->trees[t]);

      operation_run_t file_run = {
          .op = &run_op,
          .tree_vt = config->trees[t],
          .scale = scale,
          .steps = steps,
          .iterations = iterations,
          .ctx = ctx,
      };
      operation_run_require_plan(&file_run);

      char *path = outputs[t].file.path;

      FILE *file = staged_file_open_or_exit(&outputs[t].file, "w");
      file_run.file = file;
      file_run.path = path;
      operation_run_require_file(&file_run);
      if (config->header) {
         file_printf_or_exit(file, path, "%s\n", config->header);
      }

      for (u64 iteration = 0; iteration < iterations; iteration++) {
         u64 run_iteration = operation_run_next_iteration(iteration);
         for (int d = 0; d < config->distribution_count; d++) {
            dist_spec_require(config->distributions[d]);

            operation_run_t run = file_run;
            run.dist = config->distributions[d];
            run.iteration = run_iteration;
            operation_run_require_distribution(&run);

            if (progress) {
               operation_run_observer_state_t state =
                   operation_run_observer_state(&run);
               progress(&run);
               operation_run_require_observer_state(&run, state, true);
               operation_run_require_distribution(&run);
            }

            operation_run_observer_state_t seed_state =
                operation_run_observer_state(&run);
            u64 run_seed = seed(&run);
            operation_run_require_observer_state(&run, seed_state, true);
            operation_run_require_distribution(&run);
            distribution_t *dist = dist_create(run.dist, run_seed);
            random_seed(operation_run_tree_seed(&run, run_seed));
            arena_reset(&arena);

            run.instance = list_new(run.tree_vt);
            run.step_count = 0;
            operation_run_require_instance(&run);

            u64 completed = 0;
            for (u64 sample = 0; sample < samples; sample++) {
               sample_schedule_point_t point =
                   sample_schedule_next_point(schedule, sample, completed);
               u64 target = point.position;
               run.step = operation_run_chunk_step(&run, completed, target);
               run.position = target;
               operation_run_require_pending_chunk(&run, completed);
               u64 before_size = operation_run_instance_size(&run);
               if (before_chunk) {
                  operation_run_observer_state_t state =
                      operation_run_observer_state(&run);
                  before_chunk(&run);
                  operation_run_require_observer_state(&run, state, true);
                  operation_run_require_pending_chunk(&run, completed);
                  invariant(operation_run_instance_size(&run) == before_size);
               }
               for (u64 i = 0; i < run.step; i++) {
                  run.instance = operation_update(&run_op, run.instance, dist,
                                                  &run.step_count);
               }
               operation_run_require_completed_chunk(&run);
               u64 after_size = operation_run_instance_size(&run);
               if (after_chunk) {
                  operation_run_observer_state_t state =
                      operation_run_observer_state(&run);
                  after_chunk(&run);
                  operation_run_require_observer_state(&run, state, false);
                  operation_run_require_completed_chunk(&run);
                  invariant(operation_run_instance_size(&run) == after_size);
               }
               completed = target;
            }
            invariant(completed == steps);
            invariant(run.step_count == steps);

            list_free(&run.instance);
            dist_destroy(dist);
         }
      }

      staged_file_finish_or_exit(&outputs[t].file, file);
   }
#endif

   arena_restore(previous_arena);
   arena_destroy(&arena);

   operation_run_outputs_publish(outputs, config->tree_count);
   operation_run_outputs_dispose(outputs, config->tree_count);
}
