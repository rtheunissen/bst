#include "bst.h"
#include "aa.h"
#include "avl_bottomup.h"
#include "avl_relaxed_bottomup.h"
#include "avl_topdown.h"
#include "avl_weak_bottomup.h"
#include "avl_weak_topdown.h"
#include "balancer_runs.h"
#include "benchmark_index.h"
#include "benchmark_order.h"
#include "benchmark_schema.h"
#include "cli.h"
#include "conc.h"
#include "list.h"
#include "llrb.h"
#include "random.h"
#include "distributions.h"
#include "balancer_registry.h"
#include "filesystem.h"
#include "operation_runs.h"
#include "operations.h"
#include "rank.h"
#include "reference_list.h"
#include "randomized.h"
#include "redblack_bottomup.h"
#include "redblack_relaxed_bottomup.h"
#include "redblack_topdown.h"
#include "sampling.h"
#include "splay.h"
#include "time_utils.h"
#include "tree_list_adapter.h"
#include "tree_registry.h"
#include "treap_bottomup.h"
#include "treap_fingertree.h"
#include "treap_topdown.h"
#include "wbst_bottomup.h"
#include "wbst_relaxed.h"
#include "wbst_topdown.h"
#include "zip.h"

#include <errno.h>
#include <dirent.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

// ─────────────────────────────────────────────────────────────────────────────
// Test helpers
// ─────────────────────────────────────────────────────────────────────────────

static char *test_read_text_file(const char *path);
static const char *test_require_substring(const char *text, const char *needle);
static const char *test_executable_path = NULL;

static void assert_equal(u64 expected, u64 actual) {
   if (expected != actual) {
      fprintf(stderr, "expected: %llu, actual: %llu\n",
              (unsigned long long)expected, (unsigned long long)actual);
      abort();
   }
}

static void assert_int_equal(int expected, int actual) {
   if (expected != actual) {
      fprintf(stderr, "expected: %d, actual: %d\n", expected, actual);
      abort();
   }
}

static void assert_operation_action_equal(operation_action_t expected,
                                          operation_action_t actual) {
   if (expected != actual) {
      fprintf(stderr, "expected action: %d, actual action: %d\n", (int)expected,
              (int)actual);
      abort();
   }
}

static volatile sig_atomic_t test_wait_child_signal_seen = 0;
static u64 test_wait_child_eintr_count = 0;

static void test_wait_child_signal_handler(int signal) {
   (void)signal;
   test_wait_child_signal_seen = 1;
}

static int wait_child_or_abort(pid_t pid) {
   invariant(pid > 0);
   int status = 0;
   for (;;) {
      pid_t waited = waitpid(pid, &status, 0);
      if (waited == pid) {
         return status;
      }
      if (waited < 0 && errno == EINTR) {
         invariant(test_wait_child_eintr_count < UINT64_MAX);
         test_wait_child_eintr_count++;
         continue;
      }
      perror("waitpid");
      abort();
   }
}

static void assert_child_exited_2(pid_t pid) {
   int status = wait_child_or_abort(pid);
   invariant(WIFEXITED(status));
   assert_int_equal(2, WEXITSTATUS(status));
}

static void assert_child_aborted(pid_t pid) {
   int status = wait_child_or_abort(pid);
   invariant(WIFSIGNALED(status));
   assert_int_equal(SIGABRT, WTERMSIG(status));
}

static void assert_aborts(void (*fn)(void)) {
   invariant(fn != NULL);
   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      fn();
      _exit(0);
   }
   assert_child_aborted(pid);
}

static void assert_void_child_exits_2(void (*fn)(void)) {
   invariant(fn != NULL);
   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      fn();
      _exit(0);
   }
   assert_child_exited_2(pid);
}

static pid_t
assert_void_child_exits_2_with_stderr(void (*fn)(void),
                                      const char *expected_stderr) {
   invariant(fn != NULL);
   invariant(expected_stderr != NULL);

   char stderr_path[] = "/tmp/bst-child-stderr-XXXXXX";
   int stderr_fd = mkstemp(stderr_path);
   invariant(stderr_fd >= 0);
   invariant(close(stderr_fd) == 0);

   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen(stderr_path, "w", stderr);
      invariant(err != NULL);
      fn();
      _exit(0);
   }
   assert_child_exited_2(pid);

   char *stderr_text = test_read_text_file(stderr_path);
   test_require_substring(stderr_text, expected_stderr);
   free(stderr_text);
   invariant(remove(stderr_path) == 0);
   return pid;
}

static void assert_double_equal(double expected, double actual) {
   if (expected != actual) {
      fprintf(stderr, "expected: %g, actual: %g\n", expected, actual);
      abort();
   }
}

static void assert_path_metrics_equal(node_path_metrics_t expected,
                                      node_path_metrics_t actual) {
   assert_equal(expected.nodes, actual.nodes);
   assert_equal(expected.total_depth, actual.total_depth);
   assert_equal(expected.maximum_depth, actual.maximum_depth);
}

static void assert_path_absent(const char *path) {
   invariant(path != NULL);
   errno = 0;
   invariant(access(path, F_OK) != 0);
   invariant(errno == ENOENT);
}

static bool test_fgets_or_abort(char *line, size_t size, FILE *file,
                                const char *path) {
   invariant(line != NULL);
   invariant(size > 0);
   invariant(size <= (size_t)INT_MAX);
   invariant(file != NULL);
   invariant(path != NULL);

   if (fgets(line, (int)size, file) != NULL) {
      return true;
   }
   if (ferror(file)) {
      perror(path);
      abort();
   }
   return false;
}

static void test_expect_file_line(FILE *file, const char *path, char *line,
                                  size_t line_size, const char *expected) {
   invariant(expected != NULL);
   invariant(test_fgets_or_abort(line, line_size, file, path));
   if (strcmp(line, expected) != 0) {
      fprintf(stderr, "unexpected line in %s: expected %s, got %s", path,
              expected, line);
      abort();
   }
}

static void test_expect_file_eof(FILE *file, const char *path, char *line,
                                 size_t line_size) {
   if (test_fgets_or_abort(line, line_size, file, path)) {
      fprintf(stderr, "unexpected trailing line in %s: %s", path, line);
      abort();
   }
}

typedef struct {
   list_t *expected;
   u64 i;
   u64 n;
} assert_list_ctx_t;

static void assert_list_visit(u64 actual, void *ctx) {
   assert_list_ctx_t *c = (assert_list_ctx_t *)ctx;
   invariant(c != NULL);
   invariant(c->expected != NULL);
   invariant(c->i < c->n);
   u64 expected = list_select(c->expected, c->i);
   assert_equal(expected, actual);
   c->i++;
}

static void assert_list(list_t *expected, list_t *actual) {
   invariant(expected != NULL);
   invariant(actual != NULL);
   u64 en = list_size(expected);
   u64 an = list_size(actual);
   assert_equal(en, an);
   assert_list_ctx_t ctx = {expected, 0, en};
   list_each(actual, assert_list_visit, &ctx);
   assert_equal(en, ctx.i);
}

static void test_operation_run_path(const operation_run_t *run,
                                    const operation_run_config_t *config,
                                    char *path, size_t path_size, char *dir,
                                    size_t dir_size) {
   (void)run;
   path_format(path, path_size, "%s/run.txt", config->output_dir);
   path_format(dir, dir_size, "%s", config->output_dir);
}

static void test_operation_run_empty_path(const operation_run_t *run,
                                          const operation_run_config_t *config,
                                          char *path, size_t path_size,
                                          char *dir, size_t dir_size) {
   (void)run;
   invariant(path != NULL);
   invariant(path_size > 0);
   path[0] = '\0';
   path_format(dir, dir_size, "%s", config->output_dir);
}

static void test_operation_run_empty_dir(const operation_run_t *run,
                                         const operation_run_config_t *config,
                                         char *path, size_t path_size,
                                         char *dir, size_t dir_size) {
   (void)run;
   invariant(dir != NULL);
   invariant(dir_size > 0);
   path_format(path, path_size, "%s/run.txt", config->output_dir);
   dir[0] = '\0';
}

static void test_operation_run_wrong_parent_dir(
    const operation_run_t *run, const operation_run_config_t *config,
    char *path, size_t path_size, char *dir, size_t dir_size) {
   (void)run;
   path_format(path, path_size, "%s/missing-parent/run.txt",
               config->output_dir);
   path_format(dir, dir_size, "%s", config->output_dir);
}

static void test_operation_run_second_tree_empty_path(
    const operation_run_t *run, const operation_run_config_t *config,
    char *path, size_t path_size, char *dir, size_t dir_size) {
   invariant(run != NULL);
   list_vtable_require(run->tree_vt);
   if (strcmp(run->tree_vt->name, "Tracking") == 0) {
      invariant(path != NULL);
      invariant(path_size > 0);
      path[0] = '\0';
      path_format(dir, dir_size, "%s", config->output_dir);
      return;
   }
   test_operation_run_path(run, config, path, path_size, dir, dir_size);
}

static void test_operation_run_second_tree_empty_dir(
    const operation_run_t *run, const operation_run_config_t *config,
    char *path, size_t path_size, char *dir, size_t dir_size) {
   invariant(run != NULL);
   list_vtable_require(run->tree_vt);
   if (strcmp(run->tree_vt->name, "Tracking") == 0) {
      invariant(dir != NULL);
      invariant(dir_size > 0);
      path_format(path, path_size, "%s/tracking/run.txt", config->output_dir);
      dir[0] = '\0';
      return;
   }
   path_format(path, path_size, "%s/preflight-valid/run.txt",
               config->output_dir);
   path_format(dir, dir_size, "%s/preflight-valid", config->output_dir);
}

static void test_operation_run_unterminated_path(
    const operation_run_t *run, const operation_run_config_t *config,
    char *path, size_t path_size, char *dir, size_t dir_size) {
   (void)run;
   invariant(path != NULL);
   invariant(path_size > 0);
   memset(path, 'x', path_size);
   path_format(dir, dir_size, "%s", config->output_dir);
}

static void test_operation_run_unterminated_dir(
    const operation_run_t *run, const operation_run_config_t *config,
    char *path, size_t path_size, char *dir, size_t dir_size) {
   (void)run;
   invariant(dir != NULL);
   invariant(dir_size > 0);
   path_format(path, path_size, "%s/run.txt", config->output_dir);
   memset(dir, 'x', dir_size);
}

#ifdef DEBUG
static void test_operation_run_mutating_tree_name_path(
    const operation_run_t *run, const operation_run_config_t *config,
    char *path, size_t path_size, char *dir, size_t dir_size) {
   test_operation_run_path(run, config, path, path_size, dir, dir_size);
   invariant(run != NULL);
   list_vtable_require(run->tree_vt);
   char *name = (char *)run->tree_vt->name;
   name[0] = name[0] == 'M' ? 'N' : 'M';
}
#endif

typedef struct {
   u64 progress_calls;
   u64 before_calls;
   u64 after_calls;
} test_operation_run_hooks_t;

static u64 test_operation_run_seed_calls = 0;
static u64 test_operation_run_recorded_seeds[8];
static u64 test_operation_run_recorded_seed_count = 0;

static u64 test_recording_less_than(distribution_t *self, u64 n) {
   (void)self;
   invariant(n > 0);
   return 0;
}

static void test_recording_destroy(distribution_t *self) {
   free(self);
}

static distribution_t *test_recording_dist_new(u64 seed) {
   invariant(test_operation_run_recorded_seed_count <
             sizeof(test_operation_run_recorded_seeds) /
                 sizeof(test_operation_run_recorded_seeds[0]));
   test_operation_run_recorded_seeds[test_operation_run_recorded_seed_count++] =
       seed;

   distribution_t *dist = (distribution_t *)bst_malloc(sizeof(*dist));
   dist->less_than = test_recording_less_than;
   dist->destroy = test_recording_destroy;
   return dist;
}

static const dist_spec_t test_recording_dist_spec = {
    .name = "Recording",
    .factory = test_recording_dist_new,
};

static u64 test_operation_run_seed(const operation_run_t *run) {
   invariant(run != NULL);
   operation_require(run->op);
   invariant(run->tree_vt != NULL);
   invariant(run->tree_vt->name != NULL);
   dist_spec_require(run->dist);
   invariant(run->iteration > 0);
   invariant(run->iteration <= run->iterations);
   invariant(run->instance.impl == NULL);
   invariant(run->instance.vt == NULL);

   test_operation_run_seed_calls++;
   u64 seed = random_derive_seed(run->iteration, "test-operation-run");
   seed = random_derive_seed(seed, run->op->name);
   seed = random_derive_seed(seed, run->tree_vt->name);
   return random_derive_seed(seed, run->dist->name);
}

#ifdef DEBUG
static u64 test_operation_run_writing_seed(const operation_run_t *run) {
   invariant(run != NULL);
   invariant(run->file != NULL);
   invariant(run->path != NULL);
   file_puts_or_exit("seed-corrupt\n", run->file, run->path);
   invariant(fflush(run->file) == 0);
   return 1;
}
#endif

static test_operation_run_hooks_t *
test_operation_run_hooks(operation_run_t *run) {
   invariant(run != NULL);
   invariant(run->ctx != NULL);
   return (test_operation_run_hooks_t *)run->ctx;
}

static void test_operation_run_progress(operation_run_t *run) {
   test_operation_run_hooks_t *hooks = test_operation_run_hooks(run);
   invariant(run->file != NULL);
   invariant(run->path != NULL);
   operation_require(run->op);
   dist_spec_require(run->dist);
   assert_equal(run->scale, run->op->scale);
   assert_equal(run->steps, run->op->steps);
   assert_equal(0, run->step);
   assert_equal(0, run->position);
   assert_equal(0, run->step_count);
   invariant(run->instance.impl == NULL);
   invariant(run->instance.vt == NULL);
   hooks->progress_calls++;
}

static void test_operation_run_before_chunk(operation_run_t *run) {
   test_operation_run_hooks_t *hooks = test_operation_run_hooks(run);
   invariant(run->file != NULL);
   invariant(run->path != NULL);
   operation_require(run->op);
   assert_equal(run->scale, run->op->scale);
   assert_equal(run->steps, run->op->steps);
   assert_equal(run->position, size_sum(run->step_count, run->step));
   assert_equal(run->step_count, list_size(&run->instance));
   list_verify(&run->instance);
   hooks->before_calls++;
}

static void test_operation_run_after_chunk(operation_run_t *run) {
   invariant(run != NULL);
   invariant(run->file != NULL);
   invariant(run->path != NULL);
   operation_require(run->op);
   assert_equal(run->scale, run->op->scale);
   assert_equal(run->steps, run->op->steps);
   list_verify(&run->instance);
   assert_equal(run->position, run->step_count);
   assert_equal(run->position, list_size(&run->instance));
   if (run->ctx != NULL) {
      test_operation_run_hooks_t *hooks =
          (test_operation_run_hooks_t *)run->ctx;
      hooks->after_calls++;
   }
   file_printf_or_exit(run->file, run->path, "%llu %llu %llu %llu\n",
                       (unsigned long long)run->scale,
                       (unsigned long long)run->position,
                       (unsigned long long)run->step,
                       (unsigned long long)list_size(&run->instance));
}

static void
test_operation_run_abort_tracking_after_chunk(operation_run_t *run) {
   invariant(run != NULL);
   list_vtable_require(run->tree_vt);
   if (strcmp(run->tree_vt->name, "Tracking") == 0) {
      abort();
   }
   test_operation_run_after_chunk(run);
}

static void test_operation_run_abort_maximum_after_chunk(operation_run_t *run) {
   invariant(run != NULL);
   dist_spec_require(run->dist);
   if (strcmp(run->dist->name, "Maximum") == 0) {
      abort();
   }
   test_operation_run_after_chunk(run);
}

#ifdef DEBUG
static void test_operation_run_writing_hook(operation_run_t *run) {
   invariant(run != NULL);
   invariant(run->file != NULL);
   invariant(run->path != NULL);
   file_puts_or_exit("corrupt\n", run->file, run->path);
   invariant(fflush(run->file) == 0);
}

static void test_operation_run_writing_rewinding_hook(operation_run_t *run) {
   invariant(run != NULL);
   invariant(run->file != NULL);
   invariant(run->path != NULL);
   long position = ftell(run->file);
   invariant(position >= 0);
   file_puts_or_exit("rewind-corrupt\n", run->file, run->path);
   invariant(fflush(run->file) == 0);
   invariant(fseek(run->file, position, SEEK_SET) == 0);
}

static void test_operation_run_mutating_ctx_hook(operation_run_t *run) {
   invariant(run != NULL);
   run->ctx = NULL;
}

static void test_operation_run_mutating_path_hook(operation_run_t *run) {
   invariant(run != NULL);
   invariant(run->path != NULL);
   invariant(run->path[0] != '\0');
   char *path = (char *)run->path;
   path[0] = path[0] == '/' ? '.' : '/';
}

static void
test_operation_run_mutating_operation_name_hook(operation_run_t *run) {
   invariant(run != NULL);
   operation_require(run->op);
   char *name = (char *)run->op->name;
   name[0] = name[0] == 'M' ? 'N' : 'M';
}

static void test_operation_run_mutating_tree_name_hook(operation_run_t *run) {
   invariant(run != NULL);
   list_vtable_require(run->tree_vt);
   char *name = (char *)run->tree_vt->name;
   name[0] = name[0] == 'M' ? 'N' : 'M';
}

static void
test_operation_run_mutating_distribution_name_hook(operation_run_t *run) {
   invariant(run != NULL);
   dist_spec_require(run->dist);
   char *name = (char *)run->dist->name;
   name[0] = name[0] == 'M' ? 'N' : 'M';
}

static void test_operation_run_mutating_after_chunk(operation_run_t *run) {
   invariant(run != NULL);
   list_update(&run->instance, 0, 777);
}

static void test_operation_run_mutating_tree_after_chunk(operation_run_t *run) {
   invariant(run != NULL);
   tree_t *tree = list_tree(&run->instance);
   invariant(tree != NULL);
   invariant(tree->root != NULL);
   invariant(tree->root->y < UINT64_MAX);
   tree->root->y++;
}

static void test_operation_run_aliasing_tree_after_chunk(operation_run_t *run) {
   invariant(run != NULL);
   tree_t *tree = list_tree(&run->instance);
   invariant(tree != NULL);
   invariant(tree->size >= 3);
   invariant(tree->root != NULL);
   node_t *child = tree->root->l != NULL ? tree->root->l : tree->root->r;
   invariant(child != NULL);
   tree->root->l = child;
   tree->root->r = child;
   tree->root->s = 1;
}

#ifdef PERSISTENT
static void test_operation_run_mutating_refs_after_chunk(operation_run_t *run) {
   invariant(run != NULL);
   tree_t *tree = list_tree(&run->instance);
   invariant(tree != NULL);
   invariant(tree->root != NULL);
   tree_share(tree->root);
}
#endif
#endif

static u64 test_insert_index(list_t *list, distribution_t *dist) {
   u64 size = list_size(list);
   return dist_less_than(dist, sequence_insert_position_count(size));
}

static list_t insert_to_size(const list_vtable_t *vt, u64 size,
                             distribution_t *dist) {
   list_t instance = list_new(vt);
   while (list_size(&instance) < size) {
      list_insert(&instance, test_insert_index(&instance, dist),
                  random_uint64());
   }
   return instance;
}

static list_t from_reference(const list_vtable_t *vt, list_t *ref) {
   list_t instance = list_new(vt);
   u64 sz = list_size(ref);
   for (u64 i = 0; i < sz; i++) {
      list_insert(&instance, list_size(&instance), list_select(ref, i));
   }
   return instance;
}

static tree_t right_chain_of_size(u64 size) {
   tree_t tree = {0};
   node_t **link = &tree.root;

   for (u64 i = 0; i < size; i++) {
      node_t *node = tree_allocate((node_t){.x = i});
      *link = node;
      link = &node->r;
      tree.size = size_after_inserting_one(tree.size);
   }

   return tree;
}

static tree_t right_chain_with_value(u64 size, u64 value) {
   tree_t tree = {0};
   node_t **link = &tree.root;

   for (u64 i = 0; i < size; i++) {
      node_t *node = tree_allocate((node_t){.x = value});
      *link = node;
      link = &node->r;
      tree.size = size_after_inserting_one(tree.size);
   }

   return tree;
}

static void collect_inorder_nodes(tree_t tree, node_t **nodes) {
   invariant((tree.root == NULL) == (tree.size == 0));
   invariant(tree.size == 0 || nodes != NULL);

   node_t *inline_data[NODE_STACK_INLINE_CAP];
   node_stack_t stack = {
       .data = inline_data,
       .len = 0,
       .cap = NODE_STACK_INLINE_CAP,
   };

   u64 count = 0;
   node_t *node = tree.root;
   while (node != NULL || stack.len > 0) {
      while (node != NULL) {
         node_stack_push(&stack, inline_data, node);
         node = node->l;
      }

      node = node_stack_pop(&stack);
      invariant(count < tree.size);
      u64 index = count;
      count = size_after_inserting_one(count);
      nodes[index] = node;
      node = node->r;
   }

   node_stack_free(&stack, inline_data);
   assert_equal(tree.size, count);
}

typedef struct {
   node_t **nodes;
   u64 *values;
   u64 count;
} test_tree_order_snapshot_t;

static test_tree_order_snapshot_t test_tree_order_snapshot(tree_t tree) {
   invariant((tree.root == NULL) == (tree.size == 0));

   test_tree_order_snapshot_t snapshot = {.count = tree.size};
   if (tree.size == 0) {
      return snapshot;
   }

   snapshot.nodes =
       (node_t **)bst_malloc_array(tree.size, sizeof(snapshot.nodes[0]));
   snapshot.values =
       (u64 *)bst_malloc_array(tree.size, sizeof(snapshot.values[0]));
   bst_array_zero(snapshot.nodes, tree.size, sizeof(snapshot.nodes[0]));
   collect_inorder_nodes(tree, snapshot.nodes);
   for (u64 i = 0; i < tree.size; i++) {
      invariant(snapshot.nodes[i] != NULL);
      snapshot.values[i] = snapshot.nodes[i]->x;
   }
   return snapshot;
}

static void
test_tree_order_snapshot_dispose(test_tree_order_snapshot_t *snapshot) {
   invariant(snapshot != NULL);
   free(snapshot->nodes);
   free(snapshot->values);
   *snapshot = (test_tree_order_snapshot_t){0};
}

static void
assert_tree_order_snapshot(tree_t tree,
                           const test_tree_order_snapshot_t *expected) {
   invariant(expected != NULL);
   assert_equal(expected->count, tree_verify_size(tree.root, tree.size));
   assert_equal(expected->count, tree.size);
   if (expected->count == 0) {
      return;
   }

   invariant(expected->nodes != NULL);
   invariant(expected->values != NULL);
   node_t **actual =
       (node_t **)bst_malloc_array(expected->count, sizeof(actual[0]));
   bst_array_zero(actual, expected->count, sizeof(actual[0]));
   collect_inorder_nodes(tree, actual);
   for (u64 i = 0; i < expected->count; i++) {
      invariant(actual[i] != NULL);
      invariant(actual[i] == expected->nodes[i]);
      assert_equal(expected->values[i], actual[i]->x);
   }
   free(actual);
}

static tree_t assert_tree_randomize_preserves_order(tree_t tree,
                                                    distribution_t *dist) {
   invariant(dist != NULL);
   test_tree_order_snapshot_t order = test_tree_order_snapshot(tree);
   tree = tree_randomize(tree, dist);
   assert_tree_order_snapshot(tree, &order);
   test_tree_order_snapshot_dispose(&order);
   return tree;
}

static u64 test_arena_allocated_slots(const arena_t *arena) {
   invariant(arena != NULL);

   u64 slots = 0;
   for (const arena_chunk_t *chunk = &arena->first; chunk != NULL;
        chunk = chunk->next) {
      invariant(chunk->offset <= chunk->capacity);
      slots = size_sum(slots, chunk->offset);
   }
   return slots;
}

static tree_t right_chain_ranked_by_height(u64 size, u64 rank_height_stride) {
   invariant(rank_height_stride > 0);
   tree_t tree = {0};
   node_t **link = &tree.root;

   for (u64 i = 0; i < size; i++) {
      u64 subtree_height = suffix_size_after_rank(size, i);
      node_t *node = tree_allocate((node_t){
          .x = i,
          .y = subtree_height / rank_height_stride,
      });
      *link = node;
      link = &node->r;
      tree.size = size_after_inserting_one(tree.size);
   }

   return tree;
}

static node_t *perfect_ranked_subtree(int height, u64 *next, u64 *out_size) {
   invariant(next != NULL);
   invariant(out_size != NULL);
   if (height < 0) {
      *out_size = 0;
      return NULL;
   }

   u64 left_size;
   node_t *left = perfect_ranked_subtree(height - 1, next, &left_size);
   u64 x = (*next)++;

   u64 right_size;
   node_t *right = perfect_ranked_subtree(height - 1, next, &right_size);

   *out_size = tree_size_from_child_sizes(left_size, right_size);
   return tree_allocate((node_t){
       .l = left,
       .r = right,
       .s = left_size,
       .x = x,
       .y = (u64)height,
   });
}

static tree_t perfect_ranked_tree(int height) {
   invariant(height >= -1);
   u64 next = 0;
   tree_t tree = {0};
   tree.root = perfect_ranked_subtree(height, &next, &tree.size);
   assert_equal(tree.size, next);
   return tree;
}

static node_t *minimal_height_balanced_subtree(int height, u64 *next,
                                               u64 *out_size) {
   invariant(next != NULL);
   invariant(out_size != NULL);
   if (height < 0) {
      *out_size = 0;
      return NULL;
   }

   u64 left_size;
   node_t *left = minimal_height_balanced_subtree(height - 1, next, &left_size);
   u64 x = (*next)++;

   u64 right_size;
   node_t *right =
       minimal_height_balanced_subtree(height - 2, next, &right_size);

   *out_size = tree_size_from_child_sizes(left_size, right_size);
   return tree_allocate((node_t){
       .l = left,
       .r = right,
       .s = left_size,
       .x = x,
       .y = (u64)height,
   });
}

static tree_t minimal_height_balanced_tree(int height) {
   invariant(height >= -1);
   u64 next = 0;
   tree_t tree = {0};
   tree.root = minimal_height_balanced_subtree(height, &next, &tree.size);
   assert_equal(tree.size, next);
   return tree;
}

static tree_t left_chain_of_size(u64 size) {
   tree_t tree = {0};
   node_t *root = NULL;

   for (u64 i = 0; i < size; i++) {
      root = tree_allocate((node_t){.l = root, .s = i, .x = i});
      tree.size = size_after_inserting_one(tree.size);
   }

   tree.root = root;
   return tree;
}

typedef struct {
   u64 expected;
   u64 count;
} inorder_sequence_ctx_t;

static void assert_inorder_sequence(u64 x, void *ctx) {
   inorder_sequence_ctx_t *c = (inorder_sequence_ctx_t *)ctx;
   assert_equal(c->expected, x);
   c->expected++;
   c->count++;
}

static void assert_tree_inorder_sequence(tree_t tree) {
   assert_equal(tree.size, tree_verify_size(tree.root, tree.size));
   inorder_sequence_ctx_t order = {0};
   inorder(tree.root, assert_inorder_sequence, &order);
   assert_equal(tree.size, order.count);
   assert_equal(tree.size, order.expected);
}

static void assert_tree_values(tree_t tree, const u64 *values, u64 count) {
   invariant(values != NULL || count == 0);
   assert_equal(count, tree.size);
   assert_equal(count, tree_verify_size(tree.root, tree.size));
   for (u64 i = 0; i < count; i++) {
      assert_equal(values[i], tree_lookup(&tree, i));
   }
}

enum {
   TRACKING_LIST_POSITION_CAP = 8,
};

typedef struct {
   u64 size;
   u64 insert_count;
   u64 delete_count;
   u64 verify_count;
   u64 each_count;
   u64 free_min_verify_count;
   bool require_verify_before_each;
   bool omit_last_each;
   bool extra_each;
   bool drop_each_ctx;
   bool mutate_size_on_verify;
   bool omit_split_vtable;
   bool leave_impl_on_free;
   bool expose_tree;
   bool overlap_split_trees;
   tree_t tree;
   u64 inserts[TRACKING_LIST_POSITION_CAP];
   u64 deletes[TRACKING_LIST_POSITION_CAP];
} tracking_list_t;

static const list_vtable_t tracking_vtable;

static tracking_list_t *tracking_list_impl(list_t *list) {
   list_require(list);
   invariant(list->vt == &tracking_vtable);
   return (tracking_list_t *)list->impl;
}

static list_t tracking_list_new(const list_vtable_t *vt) {
   invariant(vt == &tracking_vtable);
   return (list_t){bst_calloc(1, sizeof(tracking_list_t)), vt};
}

static list_t tracking_list_clone(list_t *self) {
   tracking_list_t *src = tracking_list_impl(self);
   tracking_list_t *dst = (tracking_list_t *)bst_malloc(sizeof(*dst));
   memcpy(dst, src, sizeof(*dst));
   return (list_t){dst, self->vt};
}

static void tracking_list_free(list_t *self) {
   tracking_list_t *impl = tracking_list_impl(self);
   invariant(impl->verify_count >= impl->free_min_verify_count);
   if (impl->leave_impl_on_free) {
      return;
   }
   free(impl);
   self->impl = NULL;
}

static tree_t *tracking_list_tree(list_t *self) {
   tracking_list_t *impl = tracking_list_impl(self);
   return impl->expose_tree ? &impl->tree : NULL;
}

static u64 tracking_list_size(list_t *self) {
   return tracking_list_impl(self)->size;
}

static void tracking_list_each(list_t *self, void (*visit)(u64, void *),
                               void *ctx) {
   tracking_list_t *impl = tracking_list_impl(self);
   invariant(!impl->require_verify_before_each || impl->verify_count > 0);
   invariant(impl->each_count < UINT64_MAX);
   impl->each_count++;
   u64 count = impl->omit_last_each && impl->size > 0 ? rank_of_last(impl->size)
                                                      : impl->size;
   void *visit_ctx = impl->drop_each_ctx ? NULL : ctx;
   for (u64 i = 0; i < count; i++) {
      visit(0, visit_ctx);
   }
   if (impl->extra_each) {
      visit(0, visit_ctx);
   }
}

static void tracking_list_verify(list_t *self) {
   tracking_list_t *impl = tracking_list_impl(self);
   invariant(impl->insert_count <= TRACKING_LIST_POSITION_CAP);
   invariant(impl->delete_count <= TRACKING_LIST_POSITION_CAP);
   invariant(impl->verify_count < UINT64_MAX);
   if (impl->mutate_size_on_verify) {
      impl->size = size_after_inserting_one(impl->size);
   }
   impl->verify_count++;
}

static u64 tracking_list_select(list_t *self, u64 i) {
   tracking_list_t *impl = tracking_list_impl(self);
   invariant(i < impl->size);
   return 0;
}

static void tracking_list_update(list_t *self, u64 i, u64 x) {
   tracking_list_t *impl = tracking_list_impl(self);
   invariant(i < impl->size);
   (void)x;
}

static void tracking_list_insert(list_t *self, u64 i, u64 x) {
   tracking_list_t *impl = tracking_list_impl(self);
   invariant(i <= impl->size);
   invariant(impl->size < UINT64_MAX);
   invariant(impl->insert_count < TRACKING_LIST_POSITION_CAP);
   invariant(x == 0);
   impl->inserts[impl->insert_count++] = i;
   impl->size = size_after_inserting_one(impl->size);
}

static u64 tracking_list_delete(list_t *self, u64 i) {
   tracking_list_t *impl = tracking_list_impl(self);
   invariant(i < impl->size);
   invariant(impl->delete_count < TRACKING_LIST_POSITION_CAP);
   impl->deletes[impl->delete_count++] = i;
   impl->size = size_after_removing_one(impl->size);
   return 0;
}

static void tracking_list_split(list_t *self, u64 i, list_t *out_l,
                                list_t *out_r) {
   tracking_list_t *impl = tracking_list_impl(self);
   invariant(i <= impl->size);
   tracking_list_t *left = (tracking_list_t *)bst_calloc(1, sizeof(*left));
   tracking_list_t *right = (tracking_list_t *)bst_calloc(1, sizeof(*right));
   left->size = i;
   right->size = suffix_size_after_prefix(impl->size, i);
   if (impl->overlap_split_trees) {
      invariant(left->size == 1);
      invariant(right->size == 1);
      node_t *shared = (node_t *)bst_calloc(1, sizeof(*shared));
      shared->s = 1;
      left->expose_tree = true;
      right->expose_tree = true;
      left->tree = (tree_t){.root = shared, .size = left->size};
      right->tree = (tree_t){.root = shared, .size = right->size};
   }
   out_l->impl = left;
   out_r->impl = right;
   if (!impl->omit_split_vtable) {
      out_l->vt = self->vt;
      out_r->vt = self->vt;
   }
}

static list_t tracking_list_join(list_t *self, list_t *other) {
   tracking_list_t *left = tracking_list_impl(self);
   tracking_list_t *right = tracking_list_impl(other);

   tracking_list_t *joined = (tracking_list_t *)bst_calloc(1, sizeof(*joined));
   joined->size = size_sum(left->size, right->size);
   return (list_t){joined, self->vt};
}

static const list_vtable_t tracking_vtable = {
    .name = "Tracking",
    .new_fn = tracking_list_new,
    .clone_fn = tracking_list_clone,
    .free_fn = tracking_list_free,
    .tree_fn = tracking_list_tree,
    .size_fn = tracking_list_size,
    .each_fn = tracking_list_each,
    .verify_fn = tracking_list_verify,
    .select_fn = tracking_list_select,
    .update_fn = tracking_list_update,
    .insert_fn = tracking_list_insert,
    .delete_fn = tracking_list_delete,
    .apply_inserts_fn = list_apply_inserts_default,
    .apply_insert_delete_fn = list_apply_insert_delete_default,
    .apply_events_fn = list_apply_events_default,
    .time_inserts_fn = list_time_inserts_default,
    .time_insert_delete_fn = list_time_insert_delete_default,
    .split_fn = tracking_list_split,
    .join_fn = tracking_list_join,
};

typedef struct {
   u64 size;
} rng_probe_list_t;

typedef struct {
   const char *tree_name;
   u64 value;
} rng_probe_record_t;

enum {
   RNG_PROBE_RECORD_CAP = 8,
};

static const list_vtable_t rng_probe_a_vtable;
static const list_vtable_t rng_probe_b_vtable;
static rng_probe_record_t rng_probe_records[RNG_PROBE_RECORD_CAP];
static u64 rng_probe_record_count = 0;

static rng_probe_list_t *rng_probe_impl(list_t *list) {
   list_require(list);
   invariant(list->vt == &rng_probe_a_vtable ||
             list->vt == &rng_probe_b_vtable);
   return (rng_probe_list_t *)list->impl;
}

static list_t rng_probe_new(const list_vtable_t *vt) {
   invariant(vt == &rng_probe_a_vtable || vt == &rng_probe_b_vtable);
   invariant(rng_probe_record_count < RNG_PROBE_RECORD_CAP);
   rng_probe_records[rng_probe_record_count++] = (rng_probe_record_t){
       .tree_name = vt->name,
       .value = random_uint64(),
   };
   return (list_t){bst_calloc(1, sizeof(rng_probe_list_t)), vt};
}

static list_t rng_probe_clone(list_t *self) {
   rng_probe_list_t *src = rng_probe_impl(self);
   rng_probe_list_t *dst = (rng_probe_list_t *)bst_malloc(sizeof(*dst));
   memcpy(dst, src, sizeof(*dst));
   return (list_t){dst, self->vt};
}

static void rng_probe_free(list_t *self) {
   free(rng_probe_impl(self));
   self->impl = NULL;
}

static tree_t *rng_probe_tree(list_t *self) {
   (void)self;
   return NULL;
}

static u64 rng_probe_size(list_t *self) {
   return rng_probe_impl(self)->size;
}

static void rng_probe_each(list_t *self, void (*visit)(u64, void *),
                           void *ctx) {
   rng_probe_list_t *impl = rng_probe_impl(self);
   for (u64 i = 0; i < impl->size; i++) {
      visit(0, ctx);
   }
}

static void rng_probe_verify(list_t *self) {
   (void)rng_probe_impl(self);
}

static u64 rng_probe_select(list_t *self, u64 i) {
   rng_probe_list_t *impl = rng_probe_impl(self);
   invariant(i < impl->size);
   return 0;
}

static void rng_probe_update(list_t *self, u64 i, u64 x) {
   rng_probe_list_t *impl = rng_probe_impl(self);
   invariant(i < impl->size);
   (void)x;
}

static void rng_probe_insert(list_t *self, u64 i, u64 x) {
   rng_probe_list_t *impl = rng_probe_impl(self);
   invariant(i <= impl->size);
   invariant(impl->size < UINT64_MAX);
   invariant(x == 0);
   impl->size = size_after_inserting_one(impl->size);
}

static u64 rng_probe_delete(list_t *self, u64 i) {
   rng_probe_list_t *impl = rng_probe_impl(self);
   invariant(i < impl->size);
   impl->size = size_after_removing_one(impl->size);
   return 0;
}

static void rng_probe_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   rng_probe_list_t *impl = rng_probe_impl(self);
   invariant(i <= impl->size);
   rng_probe_list_t *left = (rng_probe_list_t *)bst_calloc(1, sizeof(*left));
   rng_probe_list_t *right = (rng_probe_list_t *)bst_calloc(1, sizeof(*right));
   left->size = i;
   right->size = suffix_size_after_prefix(impl->size, i);
   *out_l = (list_t){left, self->vt};
   *out_r = (list_t){right, self->vt};
}

static list_t rng_probe_join(list_t *self, list_t *other) {
   rng_probe_list_t *left = rng_probe_impl(self);
   rng_probe_list_t *right = rng_probe_impl(other);
   rng_probe_list_t *joined =
       (rng_probe_list_t *)bst_calloc(1, sizeof(*joined));
   joined->size = size_sum(left->size, right->size);
   return (list_t){joined, self->vt};
}

#define RNG_PROBE_VTABLE(NAME)                                                 \
   {                                                                           \
       .name = NAME,                                                           \
       .new_fn = rng_probe_new,                                                \
       .clone_fn = rng_probe_clone,                                            \
       .free_fn = rng_probe_free,                                              \
       .tree_fn = rng_probe_tree,                                              \
       .size_fn = rng_probe_size,                                              \
       .each_fn = rng_probe_each,                                              \
       .verify_fn = rng_probe_verify,                                          \
       .select_fn = rng_probe_select,                                          \
       .update_fn = rng_probe_update,                                          \
       .insert_fn = rng_probe_insert,                                          \
       .delete_fn = rng_probe_delete,                                          \
       .apply_inserts_fn = list_apply_inserts_default,                         \
       .apply_insert_delete_fn = list_apply_insert_delete_default,             \
       .apply_events_fn = list_apply_events_default,                           \
       .time_inserts_fn = list_time_inserts_default,                           \
       .time_insert_delete_fn = list_time_insert_delete_default,               \
       .split_fn = rng_probe_split,                                            \
       .join_fn = rng_probe_join,                                              \
   }

static const list_vtable_t rng_probe_a_vtable = RNG_PROBE_VTABLE("RngProbeA");
static const list_vtable_t rng_probe_b_vtable = RNG_PROBE_VTABLE("RngProbeB");

#undef RNG_PROBE_VTABLE

#ifdef DEBUG
enum {
   MUTATING_LIST_CAP = 8,
};

typedef struct {
   u64 size;
   bool corrupt_before_clone_copy;
   bool corrupt_after_update;
   bool corrupt_after_insert;
   bool corrupt_after_delete;
   u64 values[MUTATING_LIST_CAP];
} mutating_list_t;

static const list_vtable_t mutating_vtable;
static void mutating_list_flip_first(mutating_list_t *impl);

static mutating_list_t *mutating_list_impl(list_t *list) {
   list_require(list);
   invariant(list->vt == &mutating_vtable);
   return (mutating_list_t *)list->impl;
}

static list_t mutating_list_new(const list_vtable_t *vt) {
   invariant(vt == &mutating_vtable);
   return (list_t){bst_calloc(1, sizeof(mutating_list_t)), vt};
}

static list_t mutating_list_clone(list_t *self) {
   mutating_list_t *src = mutating_list_impl(self);
   if (src->corrupt_before_clone_copy) {
      mutating_list_flip_first(src);
   }
   mutating_list_t *dst = (mutating_list_t *)bst_malloc(sizeof(*dst));
   memcpy(dst, src, sizeof(*dst));
   return (list_t){dst, self->vt};
}

static void mutating_list_free(list_t *self) {
   free(mutating_list_impl(self));
   self->impl = NULL;
}

static tree_t *mutating_list_tree(list_t *self) {
   (void)self;
   return NULL;
}

static u64 mutating_list_size(list_t *self) {
   return mutating_list_impl(self)->size;
}

static void mutating_list_each(list_t *self, void (*visit)(u64, void *),
                               void *ctx) {
   mutating_list_t *impl = mutating_list_impl(self);
   for (u64 i = 0; i < impl->size; i++) {
      visit(impl->values[i], ctx);
   }
}

static void mutating_list_verify(list_t *self) {
   invariant(mutating_list_impl(self)->size <= MUTATING_LIST_CAP);
}

static u64 mutating_list_select(list_t *self, u64 i) {
   mutating_list_t *impl = mutating_list_impl(self);
   invariant(i < impl->size);
   u64 value = impl->values[i];
   mutating_list_flip_first(impl);
   return value;
}

static void mutating_list_update(list_t *self, u64 i, u64 x) {
   mutating_list_t *impl = mutating_list_impl(self);
   invariant(i < impl->size);
   impl->values[i] = x;
   if (impl->corrupt_after_update) {
      mutating_list_flip_first(impl);
   }
}

static void mutating_list_insert(list_t *self, u64 i, u64 x) {
   mutating_list_t *impl = mutating_list_impl(self);
   invariant(i <= impl->size);
   invariant(impl->size < MUTATING_LIST_CAP);
   u64 move_count = suffix_size_after_prefix(impl->size, i);
   u64 inserted_suffix_offset = size_after_inserting_one(i);
   if (move_count > 0) {
      bst_array_move(&impl->values[inserted_suffix_offset], &impl->values[i],
                     move_count, sizeof(impl->values[0]));
   }
   impl->values[i] = x;
   impl->size = size_after_inserting_one(impl->size);
   if (impl->corrupt_after_insert) {
      mutating_list_flip_first(impl);
   }
}

static u64 mutating_list_delete(list_t *self, u64 i) {
   mutating_list_t *impl = mutating_list_impl(self);
   invariant(i < impl->size);
   u64 x = impl->values[i];
   u64 move_count = suffix_size_after_rank(impl->size, i);
   u64 original_suffix_offset = size_after_inserting_one(i);
   if (move_count > 0) {
      bst_array_move(&impl->values[i], &impl->values[original_suffix_offset],
                     move_count, sizeof(impl->values[0]));
   }
   impl->size = size_after_removing_one(impl->size);
   if (impl->corrupt_after_delete) {
      mutating_list_flip_first(impl);
   }
   return x;
}

static mutating_list_t *mutating_list_segment(const mutating_list_t *src,
                                              u64 offset, u64 size) {
   invariant(src != NULL);
   invariant(offset <= src->size);
   invariant(size <= src->size - offset);
   mutating_list_t *dst = (mutating_list_t *)bst_calloc(1, sizeof(*dst));
   dst->size = size;
   if (size > 0) {
      bst_array_copy(dst->values, &src->values[offset], size,
                     sizeof(dst->values[0]));
   }
   return dst;
}

static void mutating_list_flip_first(mutating_list_t *impl) {
   invariant(impl != NULL);
   invariant(impl->size > 0);
   impl->values[0] ^= 1;
}

static void mutating_list_split(list_t *self, u64 i, list_t *out_l,
                                list_t *out_r) {
   mutating_list_t *src = mutating_list_impl(self);
   invariant(i <= src->size);
   out_l->vt = self->vt;
   out_l->impl = mutating_list_segment(src, 0, i);
   out_r->vt = self->vt;
   out_r->impl =
       mutating_list_segment(src, i, suffix_size_after_prefix(src->size, i));
   mutating_list_flip_first(src);
}

static list_t mutating_list_join(list_t *self, list_t *other) {
   mutating_list_t *left = mutating_list_impl(self);
   mutating_list_t *right = mutating_list_impl(other);
   u64 joined_size = size_sum(left->size, right->size);
   invariant(joined_size <= MUTATING_LIST_CAP);

   mutating_list_t *joined = (mutating_list_t *)bst_calloc(1, sizeof(*joined));
   joined->size = joined_size;
   if (left->size > 0) {
      bst_array_copy(joined->values, left->values, left->size,
                     sizeof(joined->values[0]));
   }
   if (right->size > 0) {
      bst_array_copy(&joined->values[left->size], right->values, right->size,
                     sizeof(joined->values[0]));
   }
   mutating_list_flip_first(left);
   return (list_t){joined, self->vt};
}

static const list_vtable_t mutating_vtable = {
    .name = "Mutating",
    .new_fn = mutating_list_new,
    .clone_fn = mutating_list_clone,
    .free_fn = mutating_list_free,
    .tree_fn = mutating_list_tree,
    .size_fn = mutating_list_size,
    .each_fn = mutating_list_each,
    .verify_fn = mutating_list_verify,
    .select_fn = mutating_list_select,
    .update_fn = mutating_list_update,
    .insert_fn = mutating_list_insert,
    .delete_fn = mutating_list_delete,
    .apply_inserts_fn = list_apply_inserts_default,
    .apply_insert_delete_fn = list_apply_insert_delete_default,
    .apply_events_fn = list_apply_events_default,
    .time_inserts_fn = list_time_inserts_default,
    .time_insert_delete_fn = list_time_insert_delete_default,
    .split_fn = mutating_list_split,
    .join_fn = mutating_list_join,
};

static void mutating_list_fill(list_t *list) {
   for (u64 i = 0; i < 4; i++) {
      list_insert(list, list_size(list), 10 + i);
   }
}

static void test_list_contract_mutating_update_child(void) {
   list_t list = list_new(&mutating_vtable);
   mutating_list_fill(&list);
   mutating_list_impl(&list)->corrupt_after_update = true;
   list_update(&list, 1, 99);
   list_free(&list);
}

static void test_list_contract_mutating_insert_child(void) {
   list_t list = list_new(&mutating_vtable);
   mutating_list_fill(&list);
   mutating_list_impl(&list)->corrupt_after_insert = true;
   list_insert(&list, 2, 99);
   list_free(&list);
}

static void test_list_contract_mutating_delete_child(void) {
   list_t list = list_new(&mutating_vtable);
   mutating_list_fill(&list);
   mutating_list_impl(&list)->corrupt_after_delete = true;
   assert_equal(12, list_delete(&list, 2));
   list_free(&list);
}

static void test_list_contract_mutating_select_child(void) {
   list_t list = list_new(&mutating_vtable);
   mutating_list_fill(&list);
   assert_equal(11, list_select(&list, 1));
   list_free(&list);
}

static void test_list_contract_mutating_clone_child(void) {
   list_t list = list_new(&mutating_vtable);
   mutating_list_fill(&list);
   mutating_list_impl(&list)->corrupt_before_clone_copy = true;
   list_t clone = list_clone(&list);
   list_free(&clone);
   list_free(&list);
}

typedef struct {
   list_t *list;
   bool mutated;
} list_each_mutating_visit_ctx_t;

static void test_list_each_mutating_visit(u64 x, void *ctx) {
   (void)x;
   list_each_mutating_visit_ctx_t *visit =
       (list_each_mutating_visit_ctx_t *)ctx;
   invariant(visit != NULL);
   invariant(visit->list != NULL);
   if (!visit->mutated) {
      mutating_list_flip_first(mutating_list_impl(visit->list));
      visit->mutated = true;
   }
}

static void test_list_contract_mutating_each_child(void) {
   list_t list = list_new(&mutating_vtable);
   mutating_list_fill(&list);
   list_each_mutating_visit_ctx_t visit = {.list = &list};
   list_each(&list, test_list_each_mutating_visit, &visit);
   list_free(&list);
}

#ifdef PERSISTENT
static void test_list_contract_mutating_split_child(void) {
   list_t list = list_new(&mutating_vtable);
   mutating_list_fill(&list);
   list_t left;
   list_t right;
   list_split(&list, 2, &left, &right);
   list_free(&left);
   list_free(&right);
   list_free(&list);
}

static void test_list_contract_mutating_join_child(void) {
   list_t left = list_new(&mutating_vtable);
   list_t right = list_new(&mutating_vtable);
   mutating_list_fill(&left);
   mutating_list_fill(&right);
   list_t joined = list_join(&left, &right);
   list_free(&joined);
   list_free(&right);
   list_free(&left);
}
#endif

#endif

// ─────────────────────────────────────────────────────────────────────────────
// Core test operations (in-place and persistent)
// ─────────────────────────────────────────────────────────────────────────────

static void test_insert_in_place(list_t *ref, list_t *seq,
                                 distribution_t *dist) {
   u64 i = test_insert_index(ref, dist);
   u64 x = random_uint64();
   list_insert(seq, i, x);
   list_insert(ref, i, x);
   list_verify(seq);
   assert_list(ref, seq);
}

static void test_delete_in_place(list_t *ref, list_t *seq,
                                 distribution_t *dist) {
   u64 i = dist_less_than(dist, list_size(ref));
   u64 x = list_delete(ref, i);
   assert_equal(x, list_delete(seq, i));
   list_verify(seq);
   assert_list(ref, seq);
}

static void test_insert_persistent(list_t *ref, list_t *seq,
                                   distribution_t *dist) {
   list_t snapshot = list_clone(seq);

   u64 i = test_insert_index(ref, dist);
   u64 x = random_uint64();

   list_insert(seq, i, x);
   list_verify(seq);
   list_verify(&snapshot);
   assert_list(ref, &snapshot);

   list_insert(ref, i, x);
   assert_list(ref, seq);

   list_insert(&snapshot, i, x);
   list_verify(&snapshot);
   list_verify(seq);
   assert_list(ref, &snapshot);
   assert_list(ref, seq);

   list_free(&snapshot);
}

static void test_persistent_delete(list_t *ref, list_t *seq,
                                   distribution_t *dist) {
   list_t snapshot = list_clone(seq);

   u64 i = dist_less_than(dist, list_size(ref));
   u64 x = list_delete(seq, i);

   list_verify(seq);
   list_verify(&snapshot);
   assert_list(ref, &snapshot);

   assert_equal(x, list_delete(ref, i));
   assert_list(ref, seq);

   assert_equal(x, list_delete(&snapshot, i));
   list_verify(&snapshot);
   list_verify(seq);
   assert_list(ref, &snapshot);
   assert_list(ref, seq);

   list_free(&snapshot);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test functions (matching Go test suite)
// ─────────────────────────────────────────────────────────────────────────────

typedef void (*test_fn)(const list_vtable_t *vt, u64 size,
                        distribution_t *dist);

static void TestInsert(const list_vtable_t *vt, u64 size,
                       distribution_t *dist) {
   list_t seq = list_new(vt);
   list_t ref = reference_for(&seq);
   while (list_size(&ref) < size) {
      test_insert_in_place(&ref, &seq, dist);
   }
   list_free(&seq);
   list_free(&ref);
}

static void TestInsertPersistent(const list_vtable_t *vt, u64 size,
                                 distribution_t *dist) {
   list_t seq = list_new(vt);
   list_t ref = reference_for(&seq);
   while (list_size(&ref) < size) {
      test_insert_persistent(&ref, &seq, dist);
   }
   list_free(&seq);
   list_free(&ref);
}

static void TestSelect(const list_vtable_t *vt, u64 size,
                       distribution_t *dist) {
   list_t ref = reference_of_size(size);
   list_t seq = from_reference(vt, &ref);

   for (u64 s = size; s > 0; s--) {
      u64 i = dist_less_than(dist, list_size(&ref));

      assert_equal(list_select(&ref, i), list_select(&seq, i));
      list_verify(&seq);
      list_select(&ref, i);

      assert_equal(list_select(&ref, i), list_select(&seq, i));
      list_verify(&seq);
   }
   list_free(&seq);
   list_free(&ref);
}

static void TestClone(const list_vtable_t *vt, u64 size, distribution_t *dist) {
   list_t original = insert_to_size(vt, size, dist);
   list_t clone = list_clone(&original);
#ifdef PERSISTENT
   bool shared_nonempty_root = size > 0;
   if (shared_nonempty_root) {
      tree_t *original_tree = list_tree(&original);
      tree_t *clone_tree = list_tree(&clone);
      invariant(original_tree->root == clone_tree->root);
      invariant(tree_shared(original_tree->root));
   }
#endif
   list_t original_ref = reference_for(&original);
   list_t clone_ref = list_clone(&original_ref);

   list_verify(&original);
   list_verify(&clone);
   assert_list(&original_ref, &original);
   assert_list(&clone_ref, &clone);

   u64 clone_insert_index = test_insert_index(&clone_ref, dist);
   u64 clone_insert_value = random_uint64();
   list_insert(&clone, clone_insert_index, clone_insert_value);
   list_insert(&clone_ref, clone_insert_index, clone_insert_value);

   list_verify(&original);
   list_verify(&clone);
#ifdef PERSISTENT
   if (shared_nonempty_root) {
      tree_t *original_tree = list_tree(&original);
      tree_t *clone_tree = list_tree(&clone);
      invariant(original_tree->root != clone_tree->root);
      invariant(!tree_shared(clone_tree->root));
   }
#endif
   assert_list(&original_ref, &original);
   assert_list(&clone_ref, &clone);

   if (list_size(&original) > 0) {
      u64 update_index = dist_less_than(dist, list_size(&original));
      u64 update_value = random_uint64();
      list_update(&original, update_index, update_value);
      list_update(&original_ref, update_index, update_value);
   }

   list_verify(&original);
   list_verify(&clone);
   assert_list(&original_ref, &original);
   assert_list(&clone_ref, &clone);

   u64 clone_delete_index = dist_less_than(dist, list_size(&clone_ref));
   assert_equal(list_delete(&clone_ref, clone_delete_index),
                list_delete(&clone, clone_delete_index));

   if (list_size(&original) > 0) {
      u64 original_delete_index =
          dist_less_than(dist, list_size(&original_ref));
      assert_equal(list_delete(&original_ref, original_delete_index),
                   list_delete(&original, original_delete_index));
   }

   list_verify(&original);
   list_verify(&clone);
   assert_list(&original_ref, &original);
   assert_list(&clone_ref, &clone);

   list_free(&original);
   list_free(&original_ref);
   list_free(&clone);
   list_free(&clone_ref);
}

static void TestSelectPersistent(const list_vtable_t *vt, u64 size,
                                 distribution_t *dist) {
   list_t ref = reference_of_size(size);
   list_t seq = from_reference(vt, &ref);

   for (u64 s = size; s > 0; s--) {
      u64 i = dist_less_than(dist, list_size(&ref));

      list_t tmp = list_clone(&seq);

      assert_equal(list_select(&ref, i), list_select(&tmp, i));
      list_verify(&seq);
      list_verify(&tmp);

      list_select(&ref, i);

      assert_equal(list_select(&ref, i), list_select(&seq, i));
      list_verify(&seq);
      list_verify(&tmp);

      list_free(&seq);
      seq = tmp;
   }
   list_free(&seq);
   list_free(&ref);
}

static void TestSelectAfterInsert(const list_vtable_t *vt, u64 scale,
                                  distribution_t *dist) {
   list_t instance = list_new(vt);
   list_t ref = reference_for(&instance);
   while (list_size(&ref) < scale) {
      test_insert_in_place(&ref, &instance, dist);

      u64 i = dist_less_than(dist, list_size(&ref));
      assert_equal(list_select(&ref, i), list_select(&instance, i));
      list_verify(&instance);
   }
   list_free(&instance);
   list_free(&ref);
}

static void TestSelectAfterInsertPersistent(const list_vtable_t *vt, u64 scale,
                                            distribution_t *dist) {
   list_t instance = list_new(vt);
   list_t ref = reference_for(&instance);
   while (list_size(&ref) < scale) {
      test_insert_persistent(&ref, &instance, dist);

      u64 i = dist_less_than(dist, list_size(&ref));
      assert_equal(list_select(&ref, i), list_select(&instance, i));
      list_verify(&instance);
   }
   list_free(&instance);
   list_free(&ref);
}

static void TestUpdate(const list_vtable_t *vt, u64 size,
                       distribution_t *dist) {
   list_t seq = insert_to_size(vt, size, dist);
   list_t ref = reference_for(&seq);

   for (u64 s = size; s > 0; s--) {
      u64 i = dist_less_than(dist, list_size(&ref));
      u64 x = random_uint64();

      list_update(&seq, i, x);
      list_update(&ref, i, x);
      list_verify(&seq);
      assert_list(&ref, &seq);
   }
   list_free(&seq);
   list_free(&ref);
}

static void TestUpdatePersistent(const list_vtable_t *vt, u64 size,
                                 distribution_t *dist) {
   list_t ref = reference_of_size(size);
   list_t seq = from_reference(vt, &ref);

   for (u64 s = size; s > 0; s--) {
      u64 x = dist_less_than(dist, list_size(&ref));
      u64 v = random_uint64();

      list_t tmp = list_clone(&seq);
      list_update(&tmp, x, v);
      list_verify(&seq);
      assert_list(&ref, &seq);

      list_update(&ref, x, v);
      list_verify(&tmp);
      assert_list(&ref, &tmp);

      list_free(&seq);
      seq = tmp;
   }
   list_free(&seq);
   list_free(&ref);
}

static void TestDelete(const list_vtable_t *vt, u64 size,
                       distribution_t *dist) {
   list_t seq = insert_to_size(vt, size, dist);
   list_t ref = reference_for(&seq);
   while (list_size(&ref) > 0) {
      test_delete_in_place(&ref, &seq, dist);
   }
   list_free(&seq);
   list_free(&ref);
}

static void TestDeletePersistent(const list_vtable_t *vt, u64 size,
                                 distribution_t *dist) {
   list_t seq = insert_to_size(vt, size, dist);
   list_t ref = reference_for(&seq);
   while (list_size(&ref) > 0) {
      test_persistent_delete(&ref, &seq, dist);
   }
   list_free(&seq);
   list_free(&ref);
}

static void TestInsertDelete(const list_vtable_t *vt, u64 size,
                             distribution_t *dist) {
   list_t seq = list_new(vt);
   list_t ref = list_new(&reference_vtable);
   while (list_size(&ref) < size) {
      test_insert_in_place(&ref, &seq, dist);
      test_insert_in_place(&ref, &seq, dist);
      test_insert_in_place(&ref, &seq, dist);

      distribution_t *max_dist = maximum_new(0);
      test_delete_in_place(&ref, &seq, max_dist);
      test_delete_in_place(&ref, &seq, max_dist);
      dist_destroy(max_dist);
   }
   list_free(&seq);
   list_free(&ref);
}

static void TestInsertDeletePersistent(const list_vtable_t *vt, u64 size,
                                       distribution_t *dist) {
   list_t seq = list_new(vt);
   list_t ref = list_new(&reference_vtable);
   while (list_size(&ref) < size) {
      test_insert_persistent(&ref, &seq, dist);
      test_insert_persistent(&ref, &seq, dist);
      test_insert_persistent(&ref, &seq, dist);

      distribution_t *ud = uniform_new(random_uint64());
      test_persistent_delete(&ref, &seq, ud);
      test_persistent_delete(&ref, &seq, ud);
      dist_destroy(ud);
   }
   list_free(&seq);
   list_free(&ref);
}

static void TestSplit(const list_vtable_t *vt, u64 size,
                      distribution_t *position) {
   list_t instance = insert_to_size(vt, size, position);
   list_t ref = reference_for(&instance);

   u64 split_size = list_size(&ref);
   for (u64 i = 0;; i++) {

      // Split
      list_t L, R;
      list_split(&instance, i, &L, &R);

      // Check invariants.
      list_verify(&instance);
      list_verify(&L);
      list_verify(&R);

      // Compare against a reference.
      list_t _L, _R;
      list_split(&ref, i, &_L, &_R);
      assert_list(&_L, &L);
      assert_list(&_R, &R);
      assert_list(&ref, &instance);

      list_verify(&L);
      list_verify(&R);

      // Delete all values from L and R after the split.
      while (list_size(&L) > 0)
         list_delete(&L, 0);
      while (list_size(&R) > 0)
         list_delete(&R, 0);

      list_verify(&instance);
      assert_list(&ref, &instance);

      list_free(&L);
      list_free(&R);
      list_free(&_L);
      list_free(&_R);

      if (i == split_size) {
         break;
      }
   }
   list_free(&instance);
   list_free(&ref);
}

static void TestJoin(const list_vtable_t *vt, u64 size, distribution_t *dist) {
   if (size == 0) {
      return;
   }
   for (u64 i = 0;; i++) {
      list_t L = insert_to_size(vt, random_less_than(size), dist);
      list_t R = insert_to_size(vt, random_less_than(size), dist);

      // Check each join against a reference.
      list_t _L = reference_for(&L);
      list_t _R = reference_for(&R);

      list_t LL = list_join(&L, &L);
      list_t LR = list_join(&L, &R);
      list_t RL = list_join(&R, &L);
      list_t RR = list_join(&R, &R);

      assert_list(&_L, &L);
      assert_list(&_R, &R);

      list_t _LL = list_join(&_L, &_L);
      list_t _LR = list_join(&_L, &_R);
      list_t _RL = list_join(&_R, &_L);
      list_t _RR = list_join(&_R, &_R);
      assert_list(&_LL, &LL);
      assert_list(&_LR, &LR);
      assert_list(&_RL, &RL);
      assert_list(&_RR, &RR);

      list_verify(&LL);
      list_verify(&LR);
      list_verify(&RL);
      list_verify(&RR);

      list_verify(&L);
      list_verify(&R);

      list_free(&LL);
      list_free(&LR);
      list_free(&RL);
      list_free(&RR);
      list_free(&_LL);
      list_free(&_LR);
      list_free(&_RL);
      list_free(&_RR);
      list_free(&L);
      list_free(&R);
      list_free(&_L);
      list_free(&_R);

      if (i + 1 == size) {
         break;
      }
   }
}

static void TestJoinFromSplit(const list_vtable_t *vt, u64 size,
                              distribution_t *dist) {
   list_t instance = insert_to_size(vt, size, dist);
   list_t ref = reference_for(&instance);

   for (u64 i = 0;; i++) {

      // Split the sequence and reference.
      list_t L, R;
      list_split(&instance, i, &L, &R);

      // Join again, in various combinations.
      list_t LL = list_join(&L, &L);
      list_t LR = list_join(&L, &R);
      list_t RL = list_join(&R, &L);
      list_t RR = list_join(&R, &R);

      // Check invariants.
      list_verify(&LL);
      list_verify(&LR);
      list_verify(&RL);
      list_verify(&RR);

      // Check each join against a reference.
      list_t _L, _R;
      list_split(&ref, i, &_L, &_R);

      list_t _LL = list_join(&_L, &_L);
      list_t _LR = list_join(&_L, &_R);
      list_t _RL = list_join(&_R, &_L);
      list_t _RR = list_join(&_R, &_R);

      assert_list(&_LL, &LL);
      assert_list(&_LR, &LR);
      assert_list(&_RL, &RL);
      assert_list(&_RR, &RR);

      list_free(&LL);
      list_free(&LR);
      list_free(&RL);
      list_free(&RR);
      list_free(&_LL);
      list_free(&_LR);
      list_free(&_RL);
      list_free(&_RR);
      list_free(&L);
      list_free(&R);
      list_free(&_L);
      list_free(&_R);

      if (i == size) {
         break;
      }
   }
   list_free(&instance);
   list_free(&ref);
}

static void TestJoinAfterInsertDelete(const list_vtable_t *vt, u64 size,
                                      distribution_t *dist) {
   list_t L = list_new(vt);
   list_t R = list_new(vt);

   // Check each join against a reference.
   list_t _L = reference_for(&L);
   list_t _R = reference_for(&R);

   while (list_size(&_L) < size) {

      test_insert_in_place(&_L, &L, dist);
      test_insert_in_place(&_L, &L, dist);
      test_delete_in_place(&_L, &L, dist);

      test_insert_in_place(&_R, &R, dist);
      test_insert_in_place(&_R, &R, dist);
      test_delete_in_place(&_R, &R, dist);

      list_t LL = list_join(&L, &L);
      list_t LR = list_join(&L, &R);
      list_t RL = list_join(&R, &L);
      list_t RR = list_join(&R, &R);

      // Check each join against a reference.
      list_t _LL = list_join(&_L, &_L);
      list_t _LR = list_join(&_L, &_R);
      list_t _RL = list_join(&_R, &_L);
      list_t _RR = list_join(&_R, &_R);
      assert_list(&_LL, &LL);
      assert_list(&_LR, &LR);
      assert_list(&_RL, &RL);
      assert_list(&_RR, &RR);

      // Check invariants.
      list_verify(&L);
      list_verify(&R);

      list_verify(&LL);
      list_verify(&LR);
      list_verify(&RL);
      list_verify(&RR);

      list_free(&LL);
      list_free(&LR);
      list_free(&RL);
      list_free(&RR);
      list_free(&_LL);
      list_free(&_LR);
      list_free(&_RL);
      list_free(&_RR);
   }
   list_free(&L);
   list_free(&R);
   list_free(&_L);
   list_free(&_R);
}

// ─────────────────────────────────────────────────────────────────────────────
// Balancer tests (matching Go TestBalancers)
// ─────────────────────────────────────────────────────────────────────────────

static bool name_matches(const char *filter, const char *name);

static void assert_balancer_verify_shape_aborts(const balancer_t *balancer) {
   balancer_require(balancer);
   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      node_t child = {0};
      node_t root = {
          .l = &child,
          .r = &child,
          .s = 1,
      };
      balancer->verify((tree_t){.root = &root, .size = 3});
      _exit(0);
   }
   assert_child_aborted(pid);
}

static void
assert_balancer_verify_null_nonzero_aborts(const balancer_t *balancer) {
   balancer_require(balancer);
   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      balancer->verify((tree_t){.root = NULL, .size = 1});
      _exit(0);
   }
   assert_child_aborted(pid);
}

static void
assert_balancer_verify_root_zero_aborts(const balancer_t *balancer) {
   balancer_require(balancer);
   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      node_t root = {0};
      balancer->verify((tree_t){.root = &root, .size = 0});
      _exit(0);
   }
   assert_child_aborted(pid);
}

#ifdef PERSISTENT
static void
assert_balancer_restore_shared_input_aborts(const balancer_t *balancer) {
   balancer_require(balancer);
   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      tree_t shared = right_chain_of_size(17);
      tree_t clone = tree_clone(&shared);
      invariant(clone.root == shared.root);
      invariant(tree_shared(shared.root));
      (void)clone;
      (void)balancer->restore(shared);
      _exit(0);
   }
   assert_child_aborted(pid);
}
#endif

static void test_height_balancer_rejects_too_tall_child(void) {
   tree_t tree = minimal_height_balanced_tree(5);
   assert_equal(20, tree.size);
   balancer_height.verify(tree);
}

static void test_height_balancer_rejects_unbalanced_child(void) {
   tree_t tree = right_chain_of_size(4);
   balancer_height.verify(tree);
}

static void test_log_balancer_rejects_log_gap_child(void) {
   tree_t tree = right_chain_of_size(8);
   balancer_log.verify(tree);
}

static tree_t
assert_balancer_restore_preserves_order(const balancer_t *balancer,
                                        tree_t tree) {
   balancer_require(balancer);
   test_tree_order_snapshot_t order = test_tree_order_snapshot(tree);

   tree = balancer->restore(tree);
   assert_tree_order_snapshot(tree, &order);
   balancer_verify_tree(balancer, tree);

   tree = balancer->restore(tree);
   assert_tree_order_snapshot(tree, &order);
   balancer_verify_tree(balancer, tree);

   test_tree_order_snapshot_dispose(&order);
   return tree;
}

static void TestBalancerEdgeCases(const balancer_t *balancer) {
   balancer_require(balancer);

   assert_balancer_verify_shape_aborts(balancer);
   assert_balancer_verify_null_nonzero_aborts(balancer);
   assert_balancer_verify_root_zero_aborts(balancer);
#ifdef PERSISTENT
   assert_balancer_restore_shared_input_aborts(balancer);
#endif
   if (balancer == &balancer_height) {
      assert_aborts(test_height_balancer_rejects_too_tall_child);
      assert_aborts(test_height_balancer_rejects_unbalanced_child);
   }
   if (balancer == &balancer_log) {
      assert_aborts(test_log_balancer_rejects_log_gap_child);
   }
   if (balancer == &balancer_cost) {
      tree_t minimal_cost = minimal_height_balanced_tree(20);
      assert_equal(28656, minimal_cost.size);
      balancer_verify_tree(balancer, minimal_cost);
      free_nodes(minimal_cost.root);
   }

   tree_t empty = {0};
   empty = assert_balancer_restore_preserves_order(balancer, empty);
   invariant(empty.root == NULL);
   assert_equal(0, empty.size);

   tree_t singleton = {0};
   tree_insert_base(&singleton, 0, 42);
   singleton = assert_balancer_restore_preserves_order(balancer, singleton);
   assert_equal(1, singleton.size);
   free_nodes(singleton.root);

   tree_t perfect = perfect_ranked_tree(8);
   perfect = assert_balancer_restore_preserves_order(balancer, perfect);
   assert_equal(511, perfect.size);
   assert_tree_inorder_sequence(perfect);
   free_nodes(perfect.root);

   const u64 chain_size = NODE_STACK_INLINE_CAP * 2 + 5;
   tree_t left_chain = left_chain_of_size(chain_size);
   left_chain = assert_balancer_restore_preserves_order(balancer, left_chain);
   assert_tree_inorder_sequence(left_chain);
   free_nodes(left_chain.root);

   tree_t right_chain = right_chain_of_size(chain_size);
   right_chain = assert_balancer_restore_preserves_order(balancer, right_chain);
   assert_tree_inorder_sequence(right_chain);
   free_nodes(right_chain.root);

   tree_t duplicate_chain = right_chain_with_value(chain_size, 7);
   duplicate_chain =
       assert_balancer_restore_preserves_order(balancer, duplicate_chain);
   free_nodes(duplicate_chain.root);

   arena_t arena;
   arena_init(&arena, 8);
   arena_t *previous_arena = arena_install(&arena);
   tree_t arena_chain = right_chain_of_size(257);
   u64 allocated_before_restore = test_arena_allocated_slots(&arena);
   arena_chain = assert_balancer_restore_preserves_order(balancer, arena_chain);
   assert_equal(allocated_before_restore, test_arena_allocated_slots(&arena));
   free_nodes(arena_chain.root);
   arena_restore(previous_arena);
   arena_destroy(&arena);
}

static u64 TestBalancers(u64 scale, const char *balancer_filter,
                         const char *dist_filter) {
   u64 total = 0;
   const dist_spec_t *dist_selection[1];
   const dist_spec_t *const *dists = dist_benchmark;
   int dist_test_count = dist_benchmark_count;

   if (dist_filter != NULL) {
      dist_selection[0] = dist_find(dist_filter);
      dist_spec_require(dist_selection[0]);
      dists = dist_selection;
      dist_test_count = 1;
   }

   printf("Balancers:\n");
   for (int b = 0; b < balancer_registry_count; b++) {
      const balancer_t *balancer = balancer_registry_all[b];
      if (!name_matches(balancer_filter, balancer->name)) {
         continue;
      }
      TestBalancerEdgeCases(balancer);

      bool printed = false;

      for (int d = 0; d < dist_test_count; d++) {
         const dist_spec_t *dist_spec = dists[d];
         dist_spec_require(dist_spec);
         if (!name_matches(dist_filter, dist_spec->name)) {
            continue;
         }

         if (!printed) {
            printf("  %s: ", balancer->name);
            fflush(stdout);
            printed = true;
         }

         tree_t tree = {0};
         distribution_t *dist = dist_create(dist_spec, 1);

         while (tree.size < scale) {
            u64 i =
                dist_less_than(dist, sequence_insert_position_count(tree.size));
            u64 x = random_uint64();

            tree_insert_base(&tree, i, x);

            invariant(!tree_shared(tree.root));
            tree = balancer->restore(tree);
            invariant(!tree_shared(tree.root));
            tree = balancer->restore(tree);
            invariant(!tree_shared(tree.root));

            balancer_verify_tree(balancer, tree);
         }

         dist_destroy(dist);
         free_nodes(tree.root);
         total++;
         printf(".");
         fflush(stdout);
      }
      if (printed) {
         printf(" OK\n");
      }
   }
   return total;
}

static void test_random_derive_seed_null_label_child(void) {
   (void)random_derive_seed(0, NULL);
}

static void test_random_derive_seed_empty_label_child(void) {
   (void)random_derive_seed(0, "");
}

static void test_splitmix64_seed_null_child(void) {
   splitmix64_seed(NULL, 0);
}

static void test_splitmix64_uint64_null_child(void) {
   (void)splitmix64_uint64(NULL);
}

static void test_xoshiro256_seed_null_child(void) {
   xoshiro256_seed(NULL, 0);
}

static void test_xoshiro256_uint64_null_child(void) {
   (void)xoshiro256_uint64(NULL);
}

static void test_random_less_than_unseeded_singleton_child(void) {
   xoshiro256_t source = {0};
   (void)random_less_than_src(1, &source);
}

static void test_random_less_than_src_null_child(void) {
   (void)random_less_than_src(1, NULL);
}

static void test_xoshiro256_require_unseeded_child(void) {
   xoshiro256_t source = {0};
   xoshiro256_require(&source);
}

static void test_xoshiro256_float64_null_child(void) {
   (void)xoshiro256_float64(NULL);
}

static void test_xoshiro256_float64_unseeded_child(void) {
   xoshiro256_t source = {0};
   (void)xoshiro256_float64(&source);
}

static void test_xoshiro256_norm_float64_null_child(void) {
   (void)xoshiro256_norm_float64(NULL);
}

static void test_xoshiro256_norm_float64_unseeded_child(void) {
   xoshiro256_t source = {0};
   (void)xoshiro256_norm_float64(&source);
}

static void test_xoshiro256_exp_float64_null_child(void) {
   (void)xoshiro256_exp_float64(NULL);
}

static void test_xoshiro256_exp_float64_unseeded_child(void) {
   xoshiro256_t source = {0};
   (void)xoshiro256_exp_float64(&source);
}

static void test_random_less_than_zero_bound_child(void) {
   (void)random_less_than(0);
}

static void test_random_less_than_src_zero_bound_child(void) {
   xoshiro256_t source;
   xoshiro256_seed(&source, 0x6a09e667f3bcc909ULL);
   (void)random_less_than_src(0, &source);
}

static void assert_random_less_than_rejection_path(void) {
   xoshiro256_t manual = {{1, 0, 0, 0}};
   u64 rejected = xoshiro256_uint64(&manual);
   u64 accepted = xoshiro256_uint64(&manual);
   u64 threshold = (u64)(-3) % 3;
   assert_equal(1, threshold);
   assert_equal(0, rejected);
   invariant((u64)((__uint128_t)3 * rejected) < threshold);
   invariant((u64)((__uint128_t)3 * accepted) >= threshold);

   xoshiro256_t bounded = {{1, 0, 0, 0}};
   u64 expected = (u64)(((__uint128_t)3 * accepted) >> 64);
   assert_equal(expected, random_less_than_src(3, &bounded));
   invariant(memcmp(&bounded, &manual, sizeof(bounded)) == 0);
}

static void TestRandom(void) {
   printf("Random:\n");

   splitmix64_t splitmix;
   splitmix64_seed(&splitmix, 0);
   static const u64 splitmix_expected[] = {
       0xe220a8397b1dcdafULL, 0x6e789e6aa1b965f4ULL, 0x06c45d188009454fULL,
       0xf88bb8a8724c81ecULL, 0x1b39896a51a8749bULL,
   };
   for (u64 i = 0; i < sizeof(splitmix_expected) / sizeof(splitmix_expected[0]);
        i++) {
      assert_equal(splitmix_expected[i], splitmix64_uint64(&splitmix));
   }
   printf("  SplitMix64 vectors OK\n");

   xoshiro256_t xoshiro;
   xoshiro256_seed(&xoshiro, 1);
   static const u64 xoshiro_expected[] = {
       0xb3f2af6d0fc710c5ULL, 0x853b559647364ceaULL, 0x92f89756082a4514ULL,
       0x642e1c7bc266a3a7ULL, 0xb27a48e29a233673ULL,
   };
   for (u64 i = 0; i < sizeof(xoshiro_expected) / sizeof(xoshiro_expected[0]);
        i++) {
      assert_equal(xoshiro_expected[i], xoshiro256_uint64(&xoshiro));
   }
   printf("  xoshiro256** vectors OK\n");

   static const u64 bounds[] = {
       1,
       2,
       17,
       (1ULL << 32) - 1,
       (1ULL << 63),
       (1ULL << 63) + 1,
       UINT64_MAX - 1,
       UINT64_MAX,
   };
   for (u64 b = 0; b < sizeof(bounds) / sizeof(bounds[0]); b++) {
      xoshiro256_t a, c;
      xoshiro256_seed(&a, 0x243f6a8885a308d3ULL + b);
      xoshiro256_seed(&c, 0x243f6a8885a308d3ULL + b);
      for (u64 i = 0; i < 128; i++) {
         u64 x = random_less_than_src(bounds[b], &a);
         u64 y = random_less_than_src(bounds[b], &c);
         assert_equal(x, y);
         invariant(x < bounds[b]);
      }
   }
   printf("  bounded random streams OK\n");

   assert_random_less_than_rejection_path();
   printf("  bounded random rejection path OK\n");

   xoshiro256_t singleton;
   xoshiro256_seed(&singleton, 0xbb67ae8584caa73bULL);
   xoshiro256_require(&singleton);
   xoshiro256_t singleton_before = singleton;
   for (int i = 0; i < 8; i++) {
      assert_equal(0, random_less_than_src(1, &singleton));
   }
   invariant(memcmp(&singleton, &singleton_before, sizeof(singleton)) == 0);
   assert_aborts(test_splitmix64_seed_null_child);
   assert_aborts(test_splitmix64_uint64_null_child);
   assert_aborts(test_xoshiro256_seed_null_child);
   assert_aborts(test_xoshiro256_uint64_null_child);
   assert_aborts(test_xoshiro256_require_unseeded_child);
   assert_aborts(test_random_less_than_unseeded_singleton_child);
   assert_aborts(test_random_less_than_src_null_child);
   assert_aborts(test_random_less_than_zero_bound_child);
   assert_aborts(test_random_less_than_src_zero_bound_child);

   random_seed(0x3c6ef372fe94f82bULL);
   u64 expected_global_next = random_uint64();
   random_seed(0x3c6ef372fe94f82bULL);
   for (int i = 0; i < 8; i++) {
      assert_equal(0, random_less_than(1));
   }
   assert_equal(expected_global_next, random_uint64());
   xoshiro256_t *global_source = random_global_source();
   invariant(global_source == random_global_source());
   random_seed(0x3c6ef372fe94f82bULL);
   u64 next_from_global_function = random_uint64();
   random_seed(0x3c6ef372fe94f82bULL);
   assert_equal(next_from_global_function, xoshiro256_uint64(global_source));
   printf("  singleton bounded random purity OK\n");

   xoshiro256_seed(&xoshiro, 0x517cc1b727220a95ULL);
   for (u64 i = 0; i < 1024; i++) {
      double unit = xoshiro256_float64(&xoshiro);
      invariant(unit >= 0.0);
      invariant(unit < 1.0);

      double normal = xoshiro256_norm_float64(&xoshiro);
      invariant(isfinite(normal));

      double exponential = xoshiro256_exp_float64(&xoshiro);
      invariant(exponential > 0.0);
      invariant(isfinite(exponential));
   }
   assert_aborts(test_xoshiro256_float64_null_child);
   assert_aborts(test_xoshiro256_float64_unseeded_child);
   assert_aborts(test_xoshiro256_norm_float64_null_child);
   assert_aborts(test_xoshiro256_norm_float64_unseeded_child);
   assert_aborts(test_xoshiro256_exp_float64_null_child);
   assert_aborts(test_xoshiro256_exp_float64_unseeded_child);
   printf("  floating helper bounds OK\n");

   u64 base = 0xA0761D6478BD642FULL;
   u64 treap_seed = random_derive_seed(base, "TreapBottomUp");
   assert_equal(treap_seed, random_derive_seed(base, "TreapBottomUp"));
   assert_equal(0x4355a96958ad4956ULL, treap_seed);
   invariant(treap_seed != random_derive_seed(base, "Zip"));
   invariant(treap_seed != random_derive_seed(base + 1, "TreapBottomUp"));

   u64 numeric_seed = random_derive_u64(base, 42);
   assert_equal(numeric_seed, random_derive_u64(base, 42));
   assert_equal(0xf7c52d8eaf341446ULL, numeric_seed);
   invariant(numeric_seed != random_derive_u64(base, 43));
   invariant(numeric_seed != random_derive_u64(base + 1, 42));
   invariant(random_derive_u64(base, 0x0100) !=
             random_derive_u64(base, 0x0001));

   random_seed(base);
   u64 first = random_uint64();
   random_seed(base);
   (void)random_derive_seed(base, "global-state-check");
   (void)random_derive_u64(base, 42);
   assert_equal(first, random_uint64());
   assert_aborts(test_random_derive_seed_null_label_child);
   assert_aborts(test_random_derive_seed_empty_label_child);
   printf("  stable derived seeds OK\n");
}

static xoshiro256_t *test_tree_random_source(list_t *list) {
   list_require(list);
   if (list->vt == &randomized_vtable) {
      return &((randomized_t *)list->impl)->source;
   }
   if (list->vt == &treap_bottomup_vtable) {
      return &((treap_bottomup_t *)list->impl)->source;
   }
   if (list->vt == &treap_topdown_vtable) {
      return &((treap_topdown_t *)list->impl)->source;
   }
   if (list->vt == &treap_fingertree_vtable) {
      return &((treap_fingertree_t *)list->impl)->source;
   }
   if (list->vt == &zip_vtable) {
      return &((zip_t *)list->impl)->source;
   }
   bst_unreachable();
}

static void assert_random_source_equal(const xoshiro256_t *actual,
                                       const xoshiro256_t *expected) {
   invariant(actual != NULL);
   invariant(expected != NULL);
   invariant(memcmp(actual, expected, sizeof(*expected)) == 0);
}

static void assert_random_source_not_equal(const xoshiro256_t *actual,
                                           const xoshiro256_t *expected) {
   invariant(actual != NULL);
   invariant(expected != NULL);
   invariant(memcmp(actual, expected, sizeof(*expected)) != 0);
}

static void assert_random_source_split_purity(const list_vtable_t *vt) {
   list_vtable_require(vt);

   list_t source = list_new(vt);
   for (u64 i = 0; i < 65; i++) {
      list_insert(&source, list_size(&source), i);
   }

   xoshiro256_t source_before = *test_tree_random_source(&source);

   list_t left;
   list_t right;
   list_split(&source, 32, &left, &right);

   assert_random_source_equal(test_tree_random_source(&source), &source_before);
   assert_random_source_equal(test_tree_random_source(&left), &source_before);
   assert_random_source_equal(test_tree_random_source(&right), &source_before);

   assert_equal(65, list_size(&source));
   assert_equal(32, list_size(&left));
   assert_equal(33, list_size(&right));
   for (u64 i = 0; i < 32; i++) {
      assert_equal(i, list_select(&left, i));
   }
   for (u64 i = 0; i < 33; i++) {
      assert_equal(32 + i, list_select(&right, i));
   }

   list_verify(&source);
   list_verify(&left);
   list_verify(&right);

   list_free(&right);
   list_free(&left);
   list_free(&source);
}

static void assert_random_source_join_purity(const list_vtable_t *vt,
                                             bool join_advances_source) {
   list_vtable_require(vt);

   list_t left = list_new(vt);
   list_t right = list_new(vt);

   for (u64 i = 0; i < 32; i++) {
      list_insert(&left, list_size(&left), i);
   }
   for (u64 i = 0; i < 33; i++) {
      list_insert(&right, list_size(&right), 1000 + i);
   }

   xoshiro256_t left_source = *test_tree_random_source(&left);
   xoshiro256_t right_source = *test_tree_random_source(&right);

   list_t joined = list_join(&left, &right);
   xoshiro256_t joined_source = *test_tree_random_source(&joined);

   assert_random_source_equal(test_tree_random_source(&left), &left_source);
   assert_random_source_equal(test_tree_random_source(&right), &right_source);
   if (join_advances_source) {
      assert_random_source_not_equal(&joined_source, &left_source);
   } else {
      assert_random_source_equal(&joined_source, &left_source);
   }

   assert_equal(65, list_size(&joined));
   for (u64 i = 0; i < 32; i++) {
      assert_equal(i, list_select(&joined, i));
   }
   for (u64 i = 0; i < 33; i++) {
      assert_equal(1000 + i, list_select(&joined, 32 + i));
   }

   list_verify(&left);
   list_verify(&right);
   list_verify(&joined);

   list_free(&joined);
   list_free(&left);
   list_free(&right);
}

static void TestRandomizedJoinPurity(void) {
   printf("Random source purity:\n");

   random_seed(0x243f6a8885a308d3ULL);
   assert_random_source_split_purity(&randomized_vtable);
   assert_random_source_split_purity(&treap_bottomup_vtable);
   assert_random_source_split_purity(&treap_topdown_vtable);
   assert_random_source_split_purity(&treap_fingertree_vtable);
   assert_random_source_split_purity(&zip_vtable);
   printf("  splits preserve input and output random state OK\n");

   assert_random_source_join_purity(&randomized_vtable, true);
   assert_random_source_join_purity(&treap_bottomup_vtable, false);
   assert_random_source_join_purity(&treap_topdown_vtable, false);
   assert_random_source_join_purity(&treap_fingertree_vtable, false);
   assert_random_source_join_purity(&zip_vtable, false);

   printf("  joins preserve input random state OK\n");
}

static void assert_shared_join_contract(const list_vtable_t *vt) {
   list_vtable_require(vt);

   list_t left = list_new(vt);
   list_t ref = list_new(&reference_vtable);
   for (u64 i = 0; i < 17; i++) {
      u64 value = 100 + i;
      list_insert(&left, list_size(&left), value);
      list_insert(&ref, list_size(&ref), value);
   }

   list_t right = list_clone(&left);
#ifdef PERSISTENT
   tree_t *left_tree = list_tree(&left);
   tree_t *right_tree = list_tree(&right);
   invariant(left_tree != NULL);
   invariant(right_tree != NULL);
   invariant(left_tree->root != NULL);
   invariant(left_tree->root == right_tree->root);
   node_t *shared_root = left_tree->root;
#endif

   list_t joined = list_join(&left, &right);
   list_t ref_joined = list_join(&ref, &ref);

   list_verify(&left);
   list_verify(&right);
   list_verify(&joined);
   assert_list(&ref, &left);
   assert_list(&ref, &right);
   assert_list(&ref_joined, &joined);

#ifdef PERSISTENT
   invariant(left_tree->root == shared_root);
   invariant(right_tree->root == shared_root);
#endif

   list_free(&joined);
   list_free(&ref_joined);
   list_free(&right);
   list_free(&left);
   list_free(&ref);
}

static void assert_divergent_shared_join_contract(const list_vtable_t *vt) {
   list_vtable_require(vt);

   list_t base = list_new(vt);
   list_t ref = list_new(&reference_vtable);
   for (u64 i = 0; i < 32; i++) {
      u64 value = 1000 + i;
      list_insert(&base, list_size(&base), value);
      list_insert(&ref, list_size(&ref), value);
   }

   list_t left = list_clone(&base);
   list_t right = list_clone(&base);
   list_t ref_left = list_clone(&ref);
   list_t ref_right = list_clone(&ref);

   list_update(&left, 0, 7001);
   list_update(&ref_left, 0, 7001);
   list_update(&right, list_size(&right) - 1, 9001);
   list_update(&ref_right, list_size(&ref_right) - 1, 9001);

#ifdef PERSISTENT
   tree_t *left_tree = list_tree(&left);
   tree_t *right_tree = list_tree(&right);
   invariant(left_tree != NULL);
   invariant(right_tree != NULL);
   invariant(left_tree->root != NULL);
   invariant(right_tree->root != NULL);
   invariant(left_tree->root != right_tree->root);
   invariant(tree_contains_shared_node(right_tree->root));
#endif

   list_t joined = list_join(&left, &right);
   list_t ref_joined = list_join(&ref_left, &ref_right);

   list_verify(&base);
   list_verify(&left);
   list_verify(&right);
   list_verify(&joined);
   assert_list(&ref, &base);
   assert_list(&ref_left, &left);
   assert_list(&ref_right, &right);
   assert_list(&ref_joined, &joined);

   list_free(&joined);
   list_free(&ref_joined);
   list_free(&ref_right);
   list_free(&ref_left);
   list_free(&right);
   list_free(&left);
   list_free(&ref);
   list_free(&base);
}

static void assert_empty_left_shared_join_contract(const list_vtable_t *vt) {
   list_vtable_require(vt);

   list_t left = list_new(vt);
   list_t source = list_new(vt);
   list_t ref = list_new(&reference_vtable);
   for (u64 i = 0; i < 24; i++) {
      u64 value = 2000 + i;
      list_insert(&source, list_size(&source), value);
      list_insert(&ref, list_size(&ref), value);
   }

   list_t right = list_clone(&source);
#ifdef PERSISTENT
   tree_t *source_tree = list_tree(&source);
   tree_t *right_tree = list_tree(&right);
   invariant(source_tree != NULL);
   invariant(right_tree != NULL);
   invariant(source_tree->root != NULL);
   invariant(source_tree->root == right_tree->root);
#endif

   list_t joined = list_join(&left, &right);
   list_verify(&left);
   list_verify(&source);
   list_verify(&right);
   list_verify(&joined);
   assert_list(&ref, &source);
   assert_list(&ref, &right);
   assert_list(&ref, &joined);

#ifdef PERSISTENT
   tree_t *joined_tree = list_tree(&joined);
   invariant(joined_tree != NULL);
   invariant(joined_tree->root == right_tree->root);
#endif

   list_update(&joined, 0, 7777);
   list_verify(&joined);
   assert_equal(7777, list_select(&joined, 0));
   assert_equal(2000, list_select(&source, 0));
   assert_equal(2000, list_select(&right, 0));

   list_free(&joined);
   list_free(&right);
   list_free(&source);
   list_free(&left);
   list_free(&ref);
}

static void assert_self_join_isolation_contract(const list_vtable_t *vt) {
   list_vtable_require(vt);

   list_t source = list_new(vt);
   list_t ref = list_new(&reference_vtable);
   for (u64 i = 0; i < 19; i++) {
      u64 value = 3000 + i;
      list_insert(&source, list_size(&source), value);
      list_insert(&ref, list_size(&ref), value);
   }

   list_t joined = list_join(&source, &source);
   list_t ref_joined = list_join(&ref, &ref);
   list_verify(&source);
   list_verify(&joined);
   assert_list(&ref, &source);
   assert_list(&ref_joined, &joined);

   u64 half = list_size(&source);
   list_update(&joined, 0, 8888);
   list_update(&joined, half, 9999);
   assert_equal(3000, list_select(&source, 0));
   assert_equal(3001, list_select(&joined, half + 1));
   assert_equal(8888, list_select(&joined, 0));
   assert_equal(9999, list_select(&joined, half));

   list_verify(&source);
   list_verify(&joined);
   assert_list(&ref, &source);

   list_free(&ref_joined);
   list_free(&joined);
   list_free(&ref);
   list_free(&source);
}

static void TestSharedJoinContracts(void) {
   printf("Shared join contracts:\n");

   random_seed(0x13198a2e03707344ULL);
   for (int i = 0; i < tree_registry_count; i++) {
      const list_vtable_t *vt = tree_registry_all[i];
      assert_shared_join_contract(vt);
      assert_divergent_shared_join_contract(vt);
      assert_empty_left_shared_join_contract(vt);
      assert_self_join_isolation_contract(vt);
   }

   printf("  joins of shared tree handles stay acyclic for %d implementations "
          "OK\n",
          tree_registry_count);
}

static void test_treap_fingertree_verify_duplicate_child_child(void) {
   node_t child = {0};
   node_t root = {
       .l = &child,
       .r = &child,
       .s = 1,
   };
   treap_fingertree_t tree = {.tree = {.root = &root, .size = 3}};
   list_t list = {.impl = &tree, .vt = &treap_fingertree_vtable};
   list_verify(&list);
}

enum {
   TREAP_FINGERTREE_SEEN_GROW_DUPLICATE_COUNT = NODE_SEEN_INLINE_CAP + 16,
};

static node_t *test_treap_fingertree_seen_grow_duplicate_tree(node_t *nodes,
                                                              u64 count) {
   invariant(nodes != NULL);
   invariant(count >= TREAP_FINGERTREE_SEEN_GROW_DUPLICATE_COUNT);
   bst_array_zero(nodes, count, sizeof(nodes[0]));

   nodes[0].y = UINT64_MAX;
   for (u64 i = 1; i < count; i++) {
      nodes[i - 1].r = &nodes[i];
      nodes[i].y = i;
   }
   nodes[count - 1].r = &nodes[0];
   return &nodes[0];
}

static void test_treap_fingertree_verify_duplicate_after_seen_grow_child(void) {
   node_t nodes[TREAP_FINGERTREE_SEEN_GROW_DUPLICATE_COUNT];
   node_t *root = test_treap_fingertree_seen_grow_duplicate_tree(
       nodes, TREAP_FINGERTREE_SEEN_GROW_DUPLICATE_COUNT);
   treap_fingertree_t tree = {
       .tree =
           {
               .root = root,
               .size = TREAP_FINGERTREE_SEEN_GROW_DUPLICATE_COUNT,
           },
   };
   list_t list = {.impl = &tree, .vt = &treap_fingertree_vtable};
   list_verify(&list);
}

static void test_treap_fingertree_verify_root_size_mismatch_child(void) {
   node_t root = {0};
   treap_fingertree_t tree = {.tree = {.root = &root, .size = 0}};
   list_t list = {.impl = &tree, .vt = &treap_fingertree_vtable};
   list_verify(&list);
}

static void test_treap_fingertree_verify_wrong_root_size_child(void) {
   node_t left = {.s = 0, .y = 0};
   node_t root = {
       .l = &left,
       .s = 0,
       .y = 1,
   };
   treap_fingertree_t tree = {.tree = {.root = &root, .size = 2}};
   list_t list = {.impl = &tree, .vt = &treap_fingertree_vtable};
   list_verify(&list);
}

static void test_treap_fingertree_verify_wrong_right_spine_size_child(void) {
   node_t right = {.s = 0, .y = 0};
   node_t root = {
       .r = &right,
       .s = 0,
       .y = 1,
   };
   treap_fingertree_t tree = {.tree = {.root = &root, .size = 1}};
   list_t list = {.impl = &tree, .vt = &treap_fingertree_vtable};
   list_verify(&list);
}

static void test_treap_fingertree_verify_left_spine_priority_child(void) {
   node_t left = {.s = 0, .y = 2};
   node_t root = {
       .l = &left,
       .s = 1,
       .y = 1,
   };
   treap_fingertree_t tree = {.tree = {.root = &root, .size = 2}};
   list_t list = {.impl = &tree, .vt = &treap_fingertree_vtable};
   list_verify(&list);
}

static void test_treap_fingertree_verify_right_spine_priority_child(void) {
   node_t right = {.s = 0, .y = 2};
   node_t root = {
       .r = &right,
       .s = 0,
       .y = 1,
   };
   treap_fingertree_t tree = {.tree = {.root = &root, .size = 2}};
   list_t list = {.impl = &tree, .vt = &treap_fingertree_vtable};
   list_verify(&list);
}

static void test_treap_fingertree_verify_wrong_attached_size_child(void) {
   node_t attached = {.s = 0, .y = 0};
   node_t left = {
       .r = &attached,
       .s = 0,
       .y = 1,
   };
   node_t root = {
       .l = &left,
       .s = 1,
       .y = 2,
   };
   treap_fingertree_t tree = {.tree = {.root = &root, .size = 3}};
   list_t list = {.impl = &tree, .vt = &treap_fingertree_vtable};
   list_verify(&list);
}

static void test_treap_fingertree_verify_wrong_tail_attached_size_child(void) {
   node_t attached = {.s = 0, .y = 0};
   node_t right = {
       .l = &attached,
       .s = 0,
       .y = 1,
   };
   node_t root = {
       .r = &right,
       .s = 0,
       .y = 2,
   };
   treap_fingertree_t tree = {.tree = {.root = &root, .size = 3}};
   list_t list = {.impl = &tree, .vt = &treap_fingertree_vtable};
   list_verify(&list);
}

#ifndef PERSISTENT
static void test_treap_fingertree_clone_duplicate_child_child(void) {
   node_t child = {0};
   node_t root = {
       .l = &child,
       .r = &child,
       .s = 1,
   };
   treap_fingertree_t tree = {.tree = {.root = &root, .size = 3}};
   list_t list = {.impl = &tree, .vt = &treap_fingertree_vtable};
   (void)list_clone(&list);
}

static void test_treap_fingertree_clone_duplicate_after_seen_grow_child(void) {
   node_t nodes[TREAP_FINGERTREE_SEEN_GROW_DUPLICATE_COUNT];
   node_t *root = test_treap_fingertree_seen_grow_duplicate_tree(
       nodes, TREAP_FINGERTREE_SEEN_GROW_DUPLICATE_COUNT);
   treap_fingertree_t tree = {
       .tree =
           {
               .root = root,
               .size = TREAP_FINGERTREE_SEEN_GROW_DUPLICATE_COUNT,
           },
   };
   list_t list = {.impl = &tree, .vt = &treap_fingertree_vtable};
   (void)list_clone(&list);
}

static void test_treap_fingertree_clone_root_size_mismatch_child(void) {
   node_t root = {0};
   treap_fingertree_t tree = {.tree = {.root = &root, .size = 0}};
   list_t list = {.impl = &tree, .vt = &treap_fingertree_vtable};
   (void)list_clone(&list);
}

static void test_treap_fingertree_clone_wrong_root_size_child(void) {
   node_t left = {.s = 0, .y = 0};
   node_t root = {
       .l = &left,
       .s = 0,
       .y = 1,
   };
   treap_fingertree_t tree = {.tree = {.root = &root, .size = 2}};
   list_t list = {.impl = &tree, .vt = &treap_fingertree_vtable};
   (void)list_clone(&list);
}

static void test_treap_fingertree_clone_wrong_right_spine_size_child(void) {
   node_t right = {.s = 0, .y = 0};
   node_t root = {
       .r = &right,
       .s = 0,
       .y = 1,
   };
   treap_fingertree_t tree = {.tree = {.root = &root, .size = 1}};
   list_t list = {.impl = &tree, .vt = &treap_fingertree_vtable};
   (void)list_clone(&list);
}

static void test_treap_fingertree_clone_left_spine_priority_child(void) {
   node_t left = {.s = 0, .y = 2};
   node_t root = {
       .l = &left,
       .s = 1,
       .y = 1,
   };
   treap_fingertree_t tree = {.tree = {.root = &root, .size = 2}};
   list_t list = {.impl = &tree, .vt = &treap_fingertree_vtable};
   (void)list_clone(&list);
}

static void test_treap_fingertree_clone_right_spine_priority_child(void) {
   node_t right = {.s = 0, .y = 2};
   node_t root = {
       .r = &right,
       .s = 0,
       .y = 1,
   };
   treap_fingertree_t tree = {.tree = {.root = &root, .size = 2}};
   list_t list = {.impl = &tree, .vt = &treap_fingertree_vtable};
   (void)list_clone(&list);
}

static void test_treap_fingertree_clone_wrong_attached_size_child(void) {
   node_t attached = {.s = 0, .y = 0};
   node_t left = {
       .r = &attached,
       .s = 0,
       .y = 1,
   };
   node_t root = {
       .l = &left,
       .s = 1,
       .y = 2,
   };
   treap_fingertree_t tree = {.tree = {.root = &root, .size = 3}};
   list_t list = {.impl = &tree, .vt = &treap_fingertree_vtable};
   (void)list_clone(&list);
}

static void test_treap_fingertree_clone_wrong_tail_attached_size_child(void) {
   node_t attached = {.s = 0, .y = 0};
   node_t right = {
       .l = &attached,
       .s = 0,
       .y = 1,
   };
   node_t root = {
       .r = &right,
       .s = 0,
       .y = 2,
   };
   treap_fingertree_t tree = {.tree = {.root = &root, .size = 3}};
   list_t list = {.impl = &tree, .vt = &treap_fingertree_vtable};
   (void)list_clone(&list);
}
#endif

static u64 treap_fingertree_contract_insert_position(u64 size, u64 step) {
   u64 insert_positions = sequence_insert_position_count(size);
   switch (step % 10) {
   case 0:
      return 0;
   case 1:
      return size;
   case 2:
      return size / 2;
   case 3:
      return size > 0 ? 1 : 0;
   case 4:
      return size > 0 ? size - 1 : 0;
   case 5:
      return (step * 17 + 3) % insert_positions;
   case 6:
      return (3 * size) / 4;
   case 7:
      return size / 4;
   case 8:
      return size > 2 ? 2 : size;
   default:
      return size > 2 ? size - 2 : 0;
   }
}

static void treap_fingertree_insert_contract_shape(list_t *tree, list_t *ref) {
   invariant(tree != NULL);
   invariant(ref != NULL);

   for (u64 i = 0; i < 96; i++) {
      u64 position =
          treap_fingertree_contract_insert_position(list_size(tree), i);
      u64 value = 100000 + 37 * i;
      list_insert(tree, position, value);
      list_insert(ref, position, value);

      if ((i & 7) == 7) {
         list_verify(tree);
         assert_list(ref, tree);
      }
   }
}

static void treap_fingertree_update_and_match(list_t *tree, list_t *ref,
                                              u64 position, u64 value) {
   list_update(tree, position, value);
   list_update(ref, position, value);
   list_verify(tree);
   assert_list(ref, tree);
}

static void treap_fingertree_delete_and_match(list_t *tree, list_t *ref,
                                              u64 position) {
   u64 expected = list_delete(ref, position);
   u64 actual = list_delete(tree, position);
   assert_equal(expected, actual);
   list_verify(tree);
   assert_list(ref, tree);
}

static void treap_fingertree_assert_split_join(list_t *tree, list_t *ref,
                                               u64 split) {
   list_t left, right;
   list_t ref_left, ref_right;
   list_split(tree, split, &left, &right);
   list_split(ref, split, &ref_left, &ref_right);

   list_verify(&left);
   list_verify(&right);
   assert_list(&ref_left, &left);
   assert_list(&ref_right, &right);

   list_t joined = list_join(&left, &right);
   list_t ref_joined = list_join(&ref_left, &ref_right);
   list_verify(&joined);
   assert_list(&ref_joined, &joined);

   list_t rotated = list_join(&right, &left);
   list_t ref_rotated = list_join(&ref_right, &ref_left);
   list_verify(&rotated);
   assert_list(&ref_rotated, &rotated);

   list_free(&ref_rotated);
   list_free(&rotated);
   list_free(&ref_joined);
   list_free(&joined);
   list_free(&ref_right);
   list_free(&ref_left);
   list_free(&right);
   list_free(&left);
}

enum {
   TREAP_FINGERTREE_SPLIT_CASE_CAP = 8,
};

static void treap_fingertree_assert_split_case_once(list_t *tree, list_t *ref,
                                                    u64 split, u64 *seen,
                                                    u64 *seen_count) {
   invariant(seen != NULL);
   invariant(seen_count != NULL);
   invariant(split <= list_size(tree));
   for (u64 i = 0; i < *seen_count; i++) {
      if (seen[i] == split) {
         return;
      }
   }
   invariant(*seen_count < TREAP_FINGERTREE_SPLIT_CASE_CAP);
   seen[*seen_count] = split;
   (*seen_count)++;
   treap_fingertree_assert_split_join(tree, ref, split);
}

static void treap_fingertree_assert_rich_shape_contract(void) {
   random_seed(0x243f6a8885a308d3ULL);
   list_t tree = list_new(&treap_fingertree_vtable);
   list_t ref = list_new(&reference_vtable);
   treap_fingertree_insert_contract_shape(&tree, &ref);

   list_verify(&tree);
   assert_list(&ref, &tree);

   tree_t *shape = list_tree(&tree);
   invariant(shape != NULL);
   invariant(shape->root != NULL);
   u64 size = list_size(&tree);
   u64 root_rank = shape->root->s;
   invariant(root_rank > 0);
   invariant(root_rank + 1 < size);

   treap_fingertree_update_and_match(&tree, &ref, 0, 700001);
   treap_fingertree_update_and_match(&tree, &ref, root_rank / 2, 700002);
   treap_fingertree_update_and_match(&tree, &ref, root_rank, 700003);
   treap_fingertree_update_and_match(&tree, &ref, root_rank + 1, 700004);
   treap_fingertree_update_and_match(&tree, &ref, size - 1, 700005);

   list_t clone = list_clone(&tree);
   list_t ref_clone = list_clone(&ref);
   treap_fingertree_update_and_match(&tree, &ref, 1, 800001);
   treap_fingertree_update_and_match(&clone, &ref_clone, size - 2, 800002);
   assert_list(&ref, &tree);
   assert_list(&ref_clone, &clone);

   u64 right_count = size - root_rank - 1;
   u64 split_seen[TREAP_FINGERTREE_SPLIT_CASE_CAP] = {0};
   u64 split_seen_count = 0;
   treap_fingertree_assert_split_case_once(&tree, &ref, root_rank / 2,
                                           split_seen, &split_seen_count);
   treap_fingertree_assert_split_case_once(&tree, &ref, root_rank, split_seen,
                                           &split_seen_count);
   treap_fingertree_assert_split_case_once(&tree, &ref, root_rank + 1,
                                           split_seen, &split_seen_count);
   treap_fingertree_assert_split_case_once(&tree, &ref,
                                           root_rank + 1 + right_count / 2,
                                           split_seen, &split_seen_count);
   treap_fingertree_assert_split_case_once(&tree, &ref, size - 1, split_seen,
                                           &split_seen_count);
   treap_fingertree_assert_split_case_once(&tree, &ref, size, split_seen,
                                           &split_seen_count);

   treap_fingertree_delete_and_match(&tree, &ref, list_tree(&tree)->root->s);
   shape = list_tree(&tree);
   invariant(shape->root != NULL);
   invariant(shape->root->s > 0);
   treap_fingertree_delete_and_match(&tree, &ref, shape->root->s / 2);
   shape = list_tree(&tree);
   invariant(shape->root != NULL);
   invariant(shape->root->s + 1 < list_size(&tree));
   treap_fingertree_delete_and_match(
       &tree, &ref,
       shape->root->s + 1 + (list_size(&tree) - shape->root->s - 1) / 2);
   treap_fingertree_delete_and_match(&tree, &ref, 0);
   treap_fingertree_delete_and_match(&tree, &ref, list_size(&tree) - 1);

   list_free(&ref_clone);
   list_free(&clone);
   list_free(&ref);
   list_free(&tree);
}

static void treap_fingertree_assert_split_join_root_rotation(void) {
   random_seed(1);
   list_t tree = list_new(&treap_fingertree_vtable);
   list_t ref = list_new(&reference_vtable);
   static const u64 positions[] = {0, 1, 0};
   static const u64 values[] = {10, 20, 30};

   for (u64 i = 0; i < sizeof(positions) / sizeof(positions[0]); i++) {
      list_insert(&tree, positions[i], values[i]);
      list_insert(&ref, positions[i], values[i]);
   }

   list_t left;
   list_t right;
   list_t ref_left;
   list_t ref_right;
   list_split(&tree, 1, &left, &right);
   list_split(&ref, 1, &ref_left, &ref_right);

   list_t joined = list_join(&left, &right);
   list_t ref_joined = list_join(&ref_left, &ref_right);
   list_verify(&joined);
   assert_list(&ref_joined, &joined);

   list_free(&ref_joined);
   list_free(&joined);
   list_free(&ref_right);
   list_free(&ref_left);
   list_free(&right);
   list_free(&left);
   list_free(&ref);
   list_free(&tree);
}

static void TestTreapFingerTreeContracts(void) {
   printf("TreapFingerTree contracts:\n");

   random_seed(1);
   list_t tree = list_new(&treap_fingertree_vtable);
   list_t ref = list_new(&reference_vtable);
   static const u64 positions[] = {0, 0, 2};
   static const u64 values[] = {10, 20, 30};
   for (u64 i = 0; i < sizeof(positions) / sizeof(positions[0]); i++) {
      list_insert(&tree, positions[i], values[i]);
      list_insert(&ref, positions[i], values[i]);
   }
   list_verify(&tree);
   assert_list(&ref, &tree);

   list_t clone = list_clone(&tree);
   list_verify(&clone);
   assert_list(&ref, &clone);

#ifndef PERSISTENT
   tree_t *tree_shape = list_tree(&tree);
   tree_t *clone_shape = list_tree(&clone);
   invariant(tree_shape != NULL);
   invariant(clone_shape != NULL);
   invariant(tree_shape->root != NULL);
   invariant(clone_shape->root != NULL);
   invariant(tree_shape->root != clone_shape->root);
#endif

   list_t doubled = list_join(&tree, &tree);
   list_t ref_doubled = list_join(&ref, &ref);
   list_verify(&doubled);
   assert_list(&ref_doubled, &doubled);

   list_t left, right;
   list_t ref_left, ref_right;
   list_split(&tree, 1, &left, &right);
   list_split(&ref, 1, &ref_left, &ref_right);
   list_t left_doubled = list_join(&left, &left);
   list_t ref_left_doubled = list_join(&ref_left, &ref_left);
   list_t right_doubled = list_join(&right, &right);
   list_t ref_right_doubled = list_join(&ref_right, &ref_right);
   list_verify(&left_doubled);
   list_verify(&right_doubled);
   assert_list(&ref_left_doubled, &left_doubled);
   assert_list(&ref_right_doubled, &right_doubled);

   list_free(&ref_right_doubled);
   list_free(&right_doubled);
   list_free(&ref_left_doubled);
   list_free(&left_doubled);
   list_free(&ref_right);
   list_free(&ref_left);
   list_free(&right);
   list_free(&left);
   list_free(&ref_doubled);
   list_free(&doubled);
   list_free(&clone);
   list_free(&ref);
   list_free(&tree);

   assert_aborts(test_treap_fingertree_verify_duplicate_child_child);
   assert_aborts(test_treap_fingertree_verify_duplicate_after_seen_grow_child);
   assert_aborts(test_treap_fingertree_verify_root_size_mismatch_child);
   assert_aborts(test_treap_fingertree_verify_wrong_root_size_child);
   assert_aborts(test_treap_fingertree_verify_wrong_right_spine_size_child);
   assert_aborts(test_treap_fingertree_verify_left_spine_priority_child);
   assert_aborts(test_treap_fingertree_verify_right_spine_priority_child);
   assert_aborts(test_treap_fingertree_verify_wrong_attached_size_child);
   assert_aborts(test_treap_fingertree_verify_wrong_tail_attached_size_child);
   printf("  verifier rejects malformed nodes OK\n");

#ifndef PERSISTENT
   assert_aborts(test_treap_fingertree_clone_duplicate_child_child);
   assert_aborts(test_treap_fingertree_clone_duplicate_after_seen_grow_child);
   assert_aborts(test_treap_fingertree_clone_root_size_mismatch_child);
   assert_aborts(test_treap_fingertree_clone_wrong_root_size_child);
   assert_aborts(test_treap_fingertree_clone_wrong_right_spine_size_child);
   assert_aborts(test_treap_fingertree_clone_left_spine_priority_child);
   assert_aborts(test_treap_fingertree_clone_right_spine_priority_child);
   assert_aborts(test_treap_fingertree_clone_wrong_attached_size_child);
   assert_aborts(test_treap_fingertree_clone_wrong_tail_attached_size_child);
   printf("  physical clone rejects malformed nodes OK\n");
#endif

   treap_fingertree_assert_rich_shape_contract();
   printf("  mixed-spine split/update/delete contract OK\n");

   treap_fingertree_assert_split_join_root_rotation();
   printf("  split/join root-priority rotation OK\n");

   printf("  representation-aware clone and self-join OK\n");
}

static void assert_distribution_sequence(distribution_t *dist,
                                         const u64 *bounds, const u64 *expected,
                                         u64 count) {
   invariant(dist != NULL);
   invariant(bounds != NULL);
   invariant(expected != NULL);
   for (u64 i = 0; i < count; i++) {
      assert_equal(expected[i], dist_less_than(dist, bounds[i]));
   }
}

static void assert_distribution_global_prng_purity(const dist_spec_t *spec,
                                                   const u64 *bounds,
                                                   int count) {
   dist_spec_require(spec);
   invariant(bounds != NULL);
   invariant(count > 0);

   u64 global_seed = random_derive_seed(0x517cc1b727220a95ULL, spec->name);
   random_seed(global_seed);
   u64 expected_next = random_uint64();

   random_seed(global_seed);
   distribution_t *dist =
       dist_create(spec, random_derive_seed(global_seed, "distribution"));
   for (int i = 0; i < count; i++) {
      (void)dist_less_than(dist, bounds[i]);
   }
   dist_destroy(dist);

   assert_equal(expected_next, random_uint64());
}

static void assert_distribution_singleton_purity(const dist_spec_t *spec,
                                                 const u64 *bounds, int count) {
   dist_spec_require(spec);
   invariant(bounds != NULL);
   invariant(count > 0);

   u64 seed = random_derive_seed(0x9e3779b97f4a7c15ULL, spec->name);
   distribution_t *sampled = dist_create(spec, seed);
   distribution_t *fresh = dist_create(spec, seed);

   for (int i = 0; i < 3; i++) {
      assert_equal(0, dist_less_than(sampled, 1));
   }
   for (int i = 0; i < 3; i++) {
      assert_equal(0, sampled->less_than(sampled, 1));
   }
   for (int i = 0; i < count; i++) {
      if (bounds[i] == 1) {
         continue;
      }
      assert_equal(dist_less_than(fresh, bounds[i]),
                   dist_less_than(sampled, bounds[i]));
   }

   dist_destroy(sampled);
   dist_destroy(fresh);
}

static void assert_global_uniform_distribution_contract(void) {
   distribution_t *global = uniform_from_global();
   dist_require(global);
   invariant(global == uniform_from_global());

   u64 seed = 0x6a09e667f3bcc909ULL;
   random_seed(seed);
   u64 expected_next = random_uint64();

   random_seed(seed);
   for (int i = 0; i < 8; i++) {
      assert_equal(0, dist_less_than(global, 1));
   }
   assert_equal(expected_next, random_uint64());

   random_seed(seed);
   for (int i = 0; i < 8; i++) {
      assert_equal(0, global->less_than(global, 1));
   }
   assert_equal(expected_next, random_uint64());

   static const u64 bounds[] = {2, 17, 1000000, UINT64_MAX};
   for (u64 i = 0; i < sizeof(bounds) / sizeof(bounds[0]); i++) {
      seed = random_derive_u64(0x6a09e667f3bcc909ULL, bounds[i]);
      random_seed(seed);
      u64 expected = random_less_than(bounds[i]);
      random_seed(seed);
      assert_equal(expected, dist_less_than(global, bounds[i]));
   }

   dist_destroy(global);
   random_seed(seed);
   u64 expected = random_less_than(97);
   random_seed(seed);
   assert_equal(expected, dist_less_than(global, 97));
}

static u64 test_distribution_unreachable_less_than(distribution_t *self,
                                                   u64 n) {
   (void)self;
   (void)n;
   bst_unreachable();
}

static u64 test_distribution_returns_bound(distribution_t *self, u64 n) {
   (void)self;
   return n;
}

static distribution_t *test_distribution_null_factory(u64 seed) {
   (void)seed;
   return NULL;
}

static void test_dist_create_null_spec_child(void) {
   (void)dist_create(NULL, 0);
}

static void test_dist_create_null_factory_result_child(void) {
   const dist_spec_t spec = {
       .name = "NullFactoryResult",
       .factory = test_distribution_null_factory,
   };
   (void)dist_create(&spec, 0);
}

static void test_dist_spec_whitespace_name_child(void) {
   const dist_spec_t spec = {
       .name = "Not A Token",
       .factory = uniform_new,
   };
   dist_spec_require(&spec);
}

static void test_dist_less_than_zero_bound_child(void) {
   distribution_t *dist = minimum_new(0);
   (void)dist_less_than(dist, 0);
}

static void test_dist_less_than_out_of_range_child(void) {
   distribution_t dist = {
       .less_than = test_distribution_returns_bound,
   };
   (void)dist_less_than(&dist, 17);
}

static void assert_distribution_wrapper_contracts(void) {
   dist_destroy(NULL);

   distribution_t singleton = {
       .less_than = test_distribution_unreachable_less_than,
   };
   assert_equal(0, dist_less_than(&singleton, 1));
   dist_destroy(&singleton);

   invariant(dist_name_is_data_token("Uniform"));
   invariant(dist_name_is_data_token("Zipf/Experimental"));
   invariant(!dist_name_is_data_token(NULL));
   invariant(!dist_name_is_data_token(""));
   invariant(!dist_name_is_data_token("Not A Token"));
   invariant(!dist_name_is_data_token("Trailing\n"));
   invariant(!dist_name_is_data_token("Bell\aToken"));
   invariant(!dist_name_is_data_token("Delete\177Token"));
   invariant(!dist_name_is_data_token("High\200Token"));
   assert_aborts(test_dist_create_null_spec_child);
   assert_aborts(test_dist_create_null_factory_result_child);
   assert_aborts(test_dist_spec_whitespace_name_child);
   assert_aborts(test_dist_less_than_zero_bound_child);
   assert_aborts(test_dist_less_than_out_of_range_child);
}

static void TestDistributions(const char *dist_filter) {
#ifdef DEBUG
   dist_debug_verify_scaling();
#endif

   static const u64 bounds[] = {
       1,
       2,
       3,
       17,
       1024,
       1000000,
       (1ULL << 32) - 1,
       (1ULL << 32),
       (1ULL << 63),
       UINT64_MAX,
   };
   static const int num_bounds = sizeof(bounds) / sizeof(bounds[0]);
   static const int samples = 256;

   printf("Distributions:\n");
   assert_distribution_wrapper_contracts();
   printf("  distribution wrapper contracts OK\n");
   assert_global_uniform_distribution_contract();
   printf("  global uniform contract OK\n");

   for (int d = 0; d < dist_count; d++) {
      const dist_spec_t *spec = dist_all[d];
      if (!name_matches(dist_filter, spec->name)) {
         continue;
      }

      printf("  %s: ", spec->name);
      fflush(stdout);

      for (int n = 0; n < num_bounds; n++) {
         u64 seed = 0x9E3779B97F4A7C15ULL + (u64)d * 0x100000001B3ULL + (u64)n;
         distribution_t *a = dist_create(spec, seed);
         distribution_t *b = dist_create(spec, seed);

         for (int i = 0; i < samples; i++) {
            u64 x = dist_less_than(a, bounds[n]);
            u64 y = dist_less_than(b, bounds[n]);
            assert_equal(x, y);
            if (x >= bounds[n]) {
               fprintf(stderr, "%s produced %llu for bound %llu\n", spec->name,
                       (unsigned long long)x, (unsigned long long)bounds[n]);
               abort();
            }
         }

         dist_destroy(a);
         dist_destroy(b);
         printf(".");
         fflush(stdout);
      }

      assert_distribution_global_prng_purity(spec, bounds, num_bounds);
      assert_distribution_singleton_purity(spec, bounds, num_bounds);
      printf(" OK\n");
   }

   printf("Distribution sequences:\n");
   for (int d = 0; d < dist_count; d++) {
      const dist_spec_t *spec = dist_all[d];
      if (!name_matches(dist_filter, spec->name)) {
         continue;
      }

      distribution_t *dist = dist_create(spec, 0xA0761D6478BD642FULL);

      if (strcmp(spec->name, "Uniform") == 0) {
         static const u64 expected[] = {
             0,
             0,
             2,
             6,
             681,
             811275,
             4043788505ULL,
             2407570528ULL,
             1445842227722397450ULL,
             2164980864886650162ULL,
         };
         assert_distribution_sequence(dist, bounds, expected, num_bounds);
      } else if (strcmp(spec->name, "Normal") == 0) {
         static const u64 expected[] = {
             0,
             0,
             1,
             5,
             769,
             531310,
             1385250695ULL,
             1708985660ULL,
             5102637898018424832ULL,
             9854542962262695935ULL,
         };
         assert_distribution_sequence(dist, bounds, expected, num_bounds);
      } else if (strcmp(spec->name, "Skewed") == 0) {
         static const u64 expected[] = {
             0,
             1,
             2,
             11,
             684,
             737433,
             2989413135ULL,
             3284082816ULL,
             6323709990587697152ULL,
             13199977031121281023ULL,
         };
         assert_distribution_sequence(dist, bounds, expected, num_bounds);
      } else if (strcmp(spec->name, "Zipf") == 0) {
         static const u64 expected[] = {
             0,
             0,
             0,
             0,
             0,
             0,
             0,
             17179,
             4962174155827869ULL,
             25917675423561920ULL,
         };
         assert_distribution_sequence(dist, bounds, expected, num_bounds);
      } else if (strcmp(spec->name, "Minimum") == 0) {
         for (int i = 0; i < num_bounds; i++) {
            assert_equal(0, dist_less_than(dist, bounds[i]));
         }
      } else if (strcmp(spec->name, "Maximum") == 0) {
         for (int i = 0; i < num_bounds; i++) {
            assert_equal(bounds[i] - 1, dist_less_than(dist, bounds[i]));
         }
      } else if (strcmp(spec->name, "Median") == 0) {
         for (int i = 0; i < num_bounds; i++) {
            assert_equal(bounds[i] / 2, dist_less_than(dist, bounds[i]));
         }
      } else if (strcmp(spec->name, "Ascending") == 0) {
         static const u64 expected[] = {0, 1, 2, 3, 4, 0, 1, 2};
         for (u64 i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
            assert_equal(expected[i], dist_less_than(dist, 5));
         }
         distribution_t *changing = dist_create(spec, 0x517cc1b727220a95ULL);
         static const u64 changing_bounds[] = {3, 3, 2, 2, 5, 5};
         static const u64 changing_expected[] = {0, 1, 0, 1, 0, 1};
         assert_distribution_sequence(
             changing, changing_bounds, changing_expected,
             sizeof(changing_bounds) / sizeof(changing_bounds[0]));
         dist_destroy(changing);
      } else if (strcmp(spec->name, "Descending") == 0) {
         static const u64 expected[] = {4, 3, 2, 1, 0, 4, 3, 2};
         for (u64 i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
            assert_equal(expected[i], dist_less_than(dist, 5));
         }
         distribution_t *changing = dist_create(spec, 0x517cc1b727220a95ULL);
         static const u64 changing_bounds[] = {3, 3, 2, 2, 5, 5};
         static const u64 changing_expected[] = {2, 1, 0, 1, 0, 4};
         assert_distribution_sequence(
             changing, changing_bounds, changing_expected,
             sizeof(changing_bounds) / sizeof(changing_bounds[0]));
         dist_destroy(changing);
      } else if (strcmp(spec->name, "Queue") == 0) {
         static const u64 expected[] = {0, 4, 0, 4, 0, 4, 0, 4};
         for (u64 i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
            assert_equal(expected[i], dist_less_than(dist, 5));
         }
         distribution_t *changing = dist_create(spec, 0x517cc1b727220a95ULL);
         static const u64 changing_bounds[] = {1, 5, 1, 5};
         static const u64 changing_expected[] = {0, 0, 0, 4};
         assert_distribution_sequence(
             changing, changing_bounds, changing_expected,
             sizeof(changing_bounds) / sizeof(changing_bounds[0]));
         dist_destroy(changing);
      } else {
         bst_unreachable();
      }

      dist_destroy(dist);
   }
   printf("  deterministic sequences OK\n");
}

enum {
   TEST_NODE_SEEN_GROW_DUPLICATE_COUNT = NODE_SEEN_INLINE_CAP + 16,
};

static void test_tree_verify_duplicate_child_child(void) {
   node_t child = {0};
   node_t root = {.l = &child, .r = &child, .s = 1};
   (void)tree_verify_size(&root, 3);
}

static void test_tree_verify_reachable_duplicate_child_child(void) {
   node_t child = {0};
   node_t root = {.l = &child, .r = &child, .s = 1};
   (void)tree_verify_reachable_size(&root);
}

static void test_tree_verify_reachable_wrong_left_size_child(void) {
   node_t child = {0};
   node_t root = {.l = &child, .s = 0};
   (void)tree_verify_reachable_size(&root);
}

static node_t *
test_tree_verify_sized_seen_grow_duplicate_tree(node_t *nodes, u64 count,
                                                u64 *reported_size) {
   invariant(nodes != NULL);
   invariant(reported_size != NULL);
   invariant(count >= TEST_NODE_SEEN_GROW_DUPLICATE_COUNT);
   invariant(count < UINT64_MAX);
   bst_array_zero(nodes, count, sizeof(nodes[0]));
   for (u64 i = 0; i + 1 < count; i++) {
      nodes[i].r = &nodes[i + 1];
   }
   nodes[count - 1].l = &nodes[0];
   nodes[count - 1].s = 1;
   *reported_size = count + 1;
   return &nodes[0];
}

static void test_tree_verify_duplicate_after_seen_grow_child(void) {
   node_t nodes[TEST_NODE_SEEN_GROW_DUPLICATE_COUNT];
   u64 reported_size = 0;
   node_t *root = test_tree_verify_sized_seen_grow_duplicate_tree(
       nodes, TEST_NODE_SEEN_GROW_DUPLICATE_COUNT, &reported_size);
   (void)tree_verify_size(root, reported_size);
}

static void test_tree_verify_reachable_duplicate_after_seen_grow_child(void) {
   node_t nodes[TEST_NODE_SEEN_GROW_DUPLICATE_COUNT];
   u64 reported_size = 0;
   node_t *root = test_tree_verify_sized_seen_grow_duplicate_tree(
       nodes, TEST_NODE_SEEN_GROW_DUPLICATE_COUNT, &reported_size);
   (void)reported_size;
   (void)tree_verify_reachable_size(root);
}

static void test_tree_verify_count_node(node_t *node, void *ctx) {
   invariant(node != NULL);
   u64 *count = (u64 *)ctx;
   invariant(count != NULL);
   invariant(*count < UINT64_MAX);
   (*count)++;
}

static void test_tree_verify_null_nonzero_child(void) {
   (void)tree_verify_size(NULL, 1);
}

static void test_tree_verify_root_zero_child(void) {
   node_t root = {0};
   (void)tree_verify_size(&root, 0);
}

static void test_tree_verify_left_size_out_of_bounds_child(void) {
   node_t root = {.s = 1};
   (void)tree_verify_size(&root, 1);
}

static void test_tree_verify_wrong_right_size_child(void) {
   node_t child = {0};
   node_t root = {
       .r = &child,
       .s = 1,
   };
   (void)tree_verify_size(&root, 2);
}

static void test_tree_verify_each_node_null_verify_child(void) {
   tree_verify_each_node(NULL, NULL, NULL);
}

static void test_tree_verify_mutate_node_x(node_t *node, void *ctx) {
   (void)ctx;
   invariant(node != NULL);
   node->x++;
}

static void test_tree_verify_size_each_node_mutating_child(void) {
   node_t root = {.x = 1};
   (void)tree_verify_size_each_node(&root, 1, test_tree_verify_mutate_node_x,
                                    NULL);
}

static void test_tree_verify_reachable_size_each_node_mutating_child(void) {
   node_t root = {.x = 1};
   (void)tree_verify_reachable_size_each_node(
       &root, test_tree_verify_mutate_node_x, NULL);
}

static void test_tree_verify_each_node_mutating_child(void) {
   node_t root = {.x = 1};
   tree_verify_each_node(&root, test_tree_verify_mutate_node_x, NULL);
}

static void test_tree_node_size_duplicate_child_child(void) {
   node_t child = {0};
   node_t root = {.l = &child, .r = &child, .s = 1};
   (void)node_size(&root);
}

static node_t *test_node_seen_grow_duplicate_tree(node_t *nodes, u64 count) {
   invariant(nodes != NULL);
   invariant(count >= TEST_NODE_SEEN_GROW_DUPLICATE_COUNT);
   bst_array_zero(nodes, count, sizeof(nodes[0]));
   for (u64 i = 0; i + 1 < count; i++) {
      nodes[i].r = &nodes[i + 1];
   }
   nodes[count - 1].l = &nodes[0];
   return &nodes[0];
}

static void test_tree_node_size_duplicate_after_seen_grow_child(void) {
   node_t nodes[TEST_NODE_SEEN_GROW_DUPLICATE_COUNT];
   (void)node_size(test_node_seen_grow_duplicate_tree(
       nodes, TEST_NODE_SEEN_GROW_DUPLICATE_COUNT));
}

static void test_tree_path_metrics_duplicate_after_seen_grow_child(void) {
   node_t nodes[TEST_NODE_SEEN_GROW_DUPLICATE_COUNT];
   (void)nodePathMetrics(test_node_seen_grow_duplicate_tree(
       nodes, TEST_NODE_SEEN_GROW_DUPLICATE_COUNT));
}

static void test_tree_path_metrics_duplicate_child_child(void) {
   node_t child = {0};
   node_t root = {.l = &child, .r = &child, .s = 1};
   (void)nodePathMetrics(&root);
}

static void test_node_path_metrics_empty_total_child(void) {
   nodePathMetricsRequire((node_path_metrics_t){
       .nodes = 0,
       .total_depth = 1,
       .maximum_depth = 0,
   });
}

static void test_node_path_metrics_singleton_total_child(void) {
   (void)nodeAveragePathLengthFromMetrics((node_path_metrics_t){
       .nodes = 1,
       .total_depth = 1,
       .maximum_depth = 0,
   });
}

static void test_node_path_metrics_multi_zero_max_child(void) {
   (void)nodeAveragePathLengthFromMetrics((node_path_metrics_t){
       .nodes = 2,
       .total_depth = 1,
       .maximum_depth = 0,
   });
}

static void test_node_path_metrics_total_too_shallow_child(void) {
   nodePathMetricsRequire((node_path_metrics_t){
       .nodes = 3,
       .total_depth = 1,
       .maximum_depth = 1,
   });
}

static void test_node_path_metrics_total_too_shallow_for_count_child(void) {
   nodePathMetricsRequire((node_path_metrics_t){
       .nodes = 4,
       .total_depth = 3,
       .maximum_depth = 2,
   });
}

static void test_node_path_metrics_total_too_shallow_for_height_child(void) {
   nodePathMetricsRequire((node_path_metrics_t){
       .nodes = 4,
       .total_depth = 3,
       .maximum_depth = 3,
   });
}

static void
test_node_path_metrics_total_too_shallow_for_count_and_height_child(void) {
   nodePathMetricsRequire((node_path_metrics_t){
       .nodes = 5,
       .total_depth = 6,
       .maximum_depth = 3,
   });
}

static void test_node_path_metrics_height_too_low_child(void) {
   nodePathMetricsRequire((node_path_metrics_t){
       .nodes = 4,
       .total_depth = 4,
       .maximum_depth = 1,
   });
}

static void test_node_path_metrics_total_too_deep_child(void) {
   nodePathMetricsRequire((node_path_metrics_t){
       .nodes = 3,
       .total_depth = 4,
       .maximum_depth = 2,
   });
}

static void test_node_path_metrics_total_too_deep_for_height_child(void) {
   nodePathMetricsRequire((node_path_metrics_t){
       .nodes = 4,
       .total_depth = 6,
       .maximum_depth = 2,
   });
}

static void test_node_path_metrics_max_depth_too_large_child(void) {
   (void)nodeMaximumPathLengthFromMetrics((node_path_metrics_t){
       .nodes = 3,
       .total_depth = 6,
       .maximum_depth = 3,
   });
}

static void test_node_path_metrics_height_overflow_child(void) {
   u64 nodes = (u64)INT_MAX + 3ULL;
   (void)nodeMaximumPathLengthFromMetrics((node_path_metrics_t){
       .nodes = nodes,
       .total_depth = nodes - 1,
       .maximum_depth = (u64)INT_MAX + 1ULL,
   });
}

static void test_tree_height_exceeds_duplicate_child_child(void) {
   node_t child = {0};
   node_t root = {.l = &child, .r = &child, .s = 1};
   (void)nodeHeightExceeds(&root, 10);
}

static void test_tree_height_exceeds_duplicate_below_bound_child(void) {
   node_t shared = {0};
   node_t too_tall = {.l = &shared, .r = &shared, .s = 1};
   node_t root = {.r = &too_tall};
   (void)nodeHeightExceeds(&root, 0);
}

static void test_tree_height_exceeds_duplicate_after_seen_grow_child(void) {
   node_t nodes[TEST_NODE_SEEN_GROW_DUPLICATE_COUNT];
   (void)nodeHeightExceeds(test_node_seen_grow_duplicate_tree(
                               nodes, TEST_NODE_SEEN_GROW_DUPLICATE_COUNT),
                           INT_MAX);
}

static void test_tree_height_duplicate_child_child(void) {
   node_t child = {0};
   node_t root = {.l = &child, .r = &child, .s = 1};
   (void)height(&root);
}

static void test_tree_height_duplicate_after_seen_grow_child(void) {
   node_t nodes[TEST_NODE_SEEN_GROW_DUPLICATE_COUNT];
   (void)height(test_node_seen_grow_duplicate_tree(
       nodes, TEST_NODE_SEEN_GROW_DUPLICATE_COUNT));
}

static void test_node_maximum_path_length_duplicate_child_child(void) {
   node_t child = {0};
   node_t root = {.l = &child, .r = &child, .s = 1};
   (void)nodeMaximumPathLength(&root);
}

static void
test_node_maximum_path_length_duplicate_after_seen_grow_child(void) {
   node_t nodes[TEST_NODE_SEEN_GROW_DUPLICATE_COUNT];
   (void)nodeMaximumPathLength(test_node_seen_grow_duplicate_tree(
       nodes, TEST_NODE_SEEN_GROW_DUPLICATE_COUNT));
}

static void test_tree_clone_duplicate_child_child(void) {
   node_t child = {0};
   node_t root = {.l = &child, .r = &child, .s = 1};
   tree_t tree = {.root = &root, .size = 3};
   (void)tree_clone_physical(&tree);
}

static void test_tree_clone_duplicate_after_seen_grow_child(void) {
   node_t nodes[TEST_NODE_SEEN_GROW_DUPLICATE_COUNT];
   u64 reported_size = 0;
   node_t *root = test_tree_verify_sized_seen_grow_duplicate_tree(
       nodes, TEST_NODE_SEEN_GROW_DUPLICATE_COUNT, &reported_size);
   tree_t tree = {.root = root, .size = reported_size};
   (void)tree_clone_physical(&tree);
}

static void test_tree_clone_physical_root_size_mismatch_child(void) {
   node_t root = {0};
   tree_t tree = {.root = &root, .size = 0};
   (void)tree_clone_physical(&tree);
}

static void test_tree_clone_root_size_mismatch_child(void) {
   node_t root = {0};
   tree_t tree = {.root = &root, .size = 0};
   (void)tree_clone(&tree);
}

static void test_tree_clone_wrong_left_size_child(void) {
   node_t child = {0};
   node_t root = {.l = &child, .s = 0};
   tree_t tree = {.root = &root, .size = 2};
   (void)tree_clone_physical(&tree);
}

static void test_tree_clone_wrong_right_size_child(void) {
   node_t child = {0};
   node_t root = {
       .r = &child,
       .s = 1,
   };
   tree_t tree = {.root = &root, .size = 2};
   (void)tree_clone_physical(&tree);
}

static void test_tree_join_internal_left_zero_size_child(void) {
   node_t node = {0};
   (void)tree_join_internal(&node, NULL, 0, 0);
}

static void test_tree_join_internal_right_zero_size_child(void) {
   node_t node = {0};
   (void)tree_join_internal(NULL, &node, 0, 0);
}

static void test_tree_join_internal_total_size_overflow_child(void) {
   node_t left = {0};
   node_t right = {0};
   (void)tree_join_internal(&left, &right, UINT64_MAX, 1);
}

static void test_tree_join_internal_left_size_without_child_child(void) {
   node_t left = {.s = 1};
   (void)tree_join_internal(&left, NULL, 2, 0);
}

static void test_tree_join_internal_right_size_without_child_child(void) {
   node_t right = {0};
   (void)tree_join_internal(NULL, &right, 0, 2);
}

static void test_tree_join_internal_linked_left_size_without_child_child(void) {
   node_t left = {.s = 1};
   node_t right_tail = {0};
   node_t right_mid = {.r = &right_tail};
   node_t right = {.r = &right_mid};
   (void)tree_join_internal(&left, &right, 2, 3);
}

static void
test_tree_join_internal_linked_right_size_without_child_child(void) {
   node_t left_tail = {0};
   node_t left_mid = {.r = &left_tail};
   node_t left = {.r = &left_mid};
   node_t right = {0};
   (void)tree_join_internal(&left, &right, 3, 2);
}

#ifdef DEBUG
static void test_tree_join_internal_overlap_child(void) {
   node_t shared = {0};
   node_t left = {
       .r = &shared,
       .s = 0,
   };
   node_t right = {
       .l = &shared,
       .s = 1,
   };
   (void)tree_join_internal(&left, &right, 2, 2);
}
#endif

static void test_tree_free_nodes_duplicate_child_child(void) {
   node_t child = {0};
   node_t root = {.l = &child, .r = &child, .s = 1};
   free_nodes(&root);
}

#ifdef DEBUG
static void test_tree_duplicate_child_visit(u64 value, void *ctx) {
   (void)value;
   (void)ctx;
}

static void test_tree_inorder_duplicate_child_child(void) {
   node_t child = {0};
   node_t root = {.l = &child, .r = &child, .s = 1};
   inorder(&root, test_tree_duplicate_child_visit, NULL);
}

static void test_tree_inorder_duplicate_after_seen_grow_child(void) {
   node_t nodes[TEST_NODE_SEEN_GROW_DUPLICATE_COUNT];
   inorder(test_node_seen_grow_duplicate_tree(
               nodes, TEST_NODE_SEEN_GROW_DUPLICATE_COUNT),
           test_tree_duplicate_child_visit, NULL);
}
#endif

static void test_tree_lookup_null_tree_child(void) {
   (void)tree_lookup(NULL, 0);
}

static void test_tree_lookup_empty_tree_child(void) {
   tree_t tree = {0};
   (void)tree_lookup(&tree, 0);
}

static void test_tree_lookup_left_size_without_child_child(void) {
   node_t root = {.s = 1};
   tree_t tree = {.root = &root, .size = 2};
   (void)tree_lookup(&tree, 1);
}

static void test_tree_update_null_tree_child(void) {
   tree_update_base(NULL, 0, 0);
}

static void test_tree_update_empty_tree_child(void) {
   tree_t tree = {0};
   tree_update_base(&tree, 0, 0);
}

static void test_tree_update_zero_left_size_with_child_child(void) {
   node_t left = {0};
   node_t root = {.l = &left};
   tree_t tree = {.root = &root, .size = 1};
   tree_update_base(&tree, 0, 0);
}

static void test_tree_insert_null_tree_child(void) {
   tree_insert_base(NULL, 0, 0);
}

static void test_tree_insert_after_end_child(void) {
   tree_t tree = {0};
   tree_insert_base(&tree, 1, 0);
}

static void test_tree_insert_internal_null_pointer_child(void) {
   tree_insert_internal(NULL, 0, 0, 0);
}

static void test_tree_insert_internal_empty_nonzero_size_child(void) {
   node_t *root = NULL;
   tree_insert_internal(&root, 1, 0, 0);
}

static void test_tree_insert_internal_after_end_child(void) {
   node_t *root = NULL;
   tree_insert_internal(&root, 0, 1, 0);
}

static void test_tree_insert_internal_right_size_without_child_child(void) {
   node_t root = {0};
   node_t *root_ptr = &root;
   tree_insert_internal(&root_ptr, 2, 0, 0);
}

static void test_tree_delete_null_tree_child(void) {
   (void)tree_delete_base(NULL, 0);
}

static void test_tree_delete_empty_tree_child(void) {
   tree_t tree = {0};
   (void)tree_delete_base(&tree, 0);
}

static void test_tree_delete_internal_null_pointer_child(void) {
   (void)tree_delete_internal(NULL, 0, 0);
}

static void test_tree_delete_internal_null_root_child(void) {
   node_t *root = NULL;
   (void)tree_delete_internal(&root, 0, 0);
}

static void test_tree_delete_internal_after_end_child(void) {
   node_t root = {0};
   node_t *root_ptr = &root;
   (void)tree_delete_internal(&root_ptr, 1, 1);
}

static void test_tree_delete_internal_wrong_size_child(void) {
   node_t root = {.s = 1};
   node_t *root_ptr = &root;
   (void)tree_delete_internal(&root_ptr, 1, 0);
}

static void test_tree_delete_internal_left_size_without_child_child(void) {
   node_t root = {.s = 1};
   node_t *root_ptr = &root;
   (void)tree_delete_internal(&root_ptr, 2, 1);
}

static void test_tree_delete_min_sized_left_size_without_child_child(void) {
   node_t root = {.s = 1};
   node_t *root_ptr = &root;
   (void)tree_deleteMin_sized(&root_ptr, 2);
}

static void test_tree_delete_min_sized_right_size_without_child_child(void) {
   node_t root = {0};
   node_t *root_ptr = &root;
   (void)tree_deleteMin_sized(&root_ptr, 2);
}

static void test_tree_delete_max_sized_left_size_without_child_child(void) {
   node_t root = {.s = 1};
   node_t *root_ptr = &root;
   (void)tree_deleteMax_sized(&root_ptr, 2);
}

static void test_tree_delete_max_sized_right_size_without_child_child(void) {
   node_t root = {0};
   node_t *root_ptr = &root;
   (void)tree_deleteMax_sized(&root_ptr, 2);
}

static void test_tree_split_null_tree_child(void) {
   (void)tree_split_base(NULL, 0);
}

static void test_tree_split_after_end_child(void) {
   tree_t tree = {0};
   (void)tree_split_base(&tree, 1);
}

static void test_tree_split_roots_null_nonzero_child(void) {
   (void)tree_split_roots(NULL, 1);
}

static void test_tree_split_roots_after_end_child(void) {
   node_t root = {0};
   (void)tree_split_roots(&root, 2);
}

static void test_tree_split_roots_left_size_without_child_child(void) {
   node_t root = {.s = 1};
   (void)tree_split_roots(&root, 0);
}

static void test_tree_split_roots_sized_null_nonzero_child(void) {
   (void)tree_split_roots_sized(NULL, 1, 0);
}

static void test_tree_split_roots_sized_after_end_child(void) {
   node_t root = {0};
   (void)tree_split_roots_sized(&root, 2, 1);
}

#ifdef DEBUG
static void test_tree_split_roots_sized_wrong_reachable_size_child(void) {
   node_t root = {0};
   (void)tree_split_roots_sized(&root, 1, 2);
}
#endif

static void test_tree_join_null_left_child(void) {
   tree_t right = {0};
   (void)tree_join_base(NULL, &right);
}

static void test_tree_join_null_right_child(void) {
   tree_t left = {0};
   (void)tree_join_base(&left, NULL);
}

static void test_tree_join_same_tree_child(void) {
   tree_t tree = {0};
   (void)tree_join_base(&tree, &tree);
}

static void test_tree_dissolve_null_child(void) {
   (void)tree_dissolve(NULL, 0);
}

static void test_tree_dissolve_zero_size_child(void) {
   node_t root = {0};
   (void)tree_dissolve(&root, 0);
}

static void test_tree_dissolve_rank_out_of_bounds_child(void) {
   node_t root = {.s = 1};
   (void)tree_dissolve(&root, 1);
}

static void test_tree_dissolve_left_size_without_child_child(void) {
   node_t root = {.s = 1};
   (void)tree_dissolve(&root, 2);
}

static void test_tree_dissolve_zero_left_size_with_child_child(void) {
   node_t left = {0};
   node_t root = {.l = &left};
   (void)tree_dissolve(&root, 1);
}

static void test_tree_dissolve_right_size_without_child_child(void) {
   node_t root = {0};
   (void)tree_dissolve(&root, 2);
}

#ifdef PERSISTENT
static void test_tree_dissolve_shared_child(void) {
   node_t root = {.refs = 1};
   (void)tree_dissolve(&root, 1);
}
#endif

static void test_tree_appendR_raw_null_pointer_child(void) {
   node_t node = {0};
   tree_appendR_raw(NULL, &node);
}

static void test_tree_appendL_raw_null_pointer_child(void) {
   node_t node = {0};
   tree_appendL_raw(NULL, &node);
}

static void test_tree_appendR_raw_reachable_append_root_child(void) {
   tree_t tree = right_chain_of_size(3);
   tree_appendR_raw(&tree.root, tree.root->r);
}

static void test_tree_appendL_raw_reachable_append_root_child(void) {
   tree_t tree = left_chain_of_size(3);
   tree_appendL_raw(&tree.root, tree.root->l);
}

static void
test_tree_replacedByRightSubtree_sized_left_size_without_child_child(void) {
   node_t root = {.s = 1};
   node_t *root_ptr = &root;
   (void)tree_replacedByRightSubtree_sized(&root_ptr, 2);
}

static void
test_tree_replacedByRightSubtree_sized_right_size_without_child_child(void) {
   node_t root = {0};
   node_t *root_ptr = &root;
   (void)tree_replacedByRightSubtree_sized(&root_ptr, 2);
}

static void
test_tree_replacedByLeftSubtree_sized_left_size_without_child_child(void) {
   node_t root = {.s = 1};
   node_t *root_ptr = &root;
   (void)tree_replacedByLeftSubtree_sized(&root_ptr, 2);
}

static void
test_tree_replacedByLeftSubtree_sized_right_size_without_child_child(void) {
   node_t root = {0};
   node_t *root_ptr = &root;
   (void)tree_replacedByLeftSubtree_sized(&root_ptr, 2);
}

static void test_tree_partition_sized_null_child(void) {
   (void)tree_partition_sized(NULL, 0, 0);
}

static void test_tree_partition_sized_zero_size_child(void) {
   node_t root = {0};
   (void)tree_partition_sized(&root, 0, 0);
}

static void test_tree_partition_sized_after_end_child(void) {
   node_t root = {0};
   (void)tree_partition_sized(&root, 1, 1);
}

static void test_tree_partition_sized_rank_out_of_bounds_child(void) {
   node_t root = {.s = 1};
   (void)tree_partition_sized(&root, 1, 0);
}

static void test_tree_partition_sized_left_size_without_child_child(void) {
   node_t root = {.s = 1};
   (void)tree_partition_sized(&root, 2, 1);
}

static void test_tree_partition_sized_zero_left_size_with_child_child(void) {
   node_t left = {0};
   node_t root = {.l = &left};
   (void)tree_partition_sized(&root, 1, 0);
}

static void test_tree_partition_sized_right_size_without_child_child(void) {
   node_t root = {0};
   (void)tree_partition_sized(&root, 2, 0);
}

static void test_tree_partition_null_child(void) {
   (void)tree_partition(NULL, 0);
}

typedef struct {
   u64 nodes;
   u64 maximum_depth;
   u64 minimum_total_depth;
   u64 maximum_total_depth;
} node_path_metric_bounds_case_t;

typedef struct {
   bool present;
   u64 minimum_total_depth;
   u64 maximum_total_depth;
} test_small_path_metric_bounds_t;

static void
assert_node_path_metric_bounds_case(node_path_metric_bounds_case_t test) {
   invariant(test.nodes > 0);
   invariant(test.maximum_depth < test.nodes);
   invariant(test.minimum_total_depth <= test.maximum_total_depth);

   __uint128_t minimum_total_depth = nodePathMetricsMinimumTotalDepthForHeight(
       test.nodes, test.maximum_depth);
   __uint128_t maximum_total_depth = nodePathMetricsMaximumTotalDepthForHeight(
       test.nodes, test.maximum_depth);
   invariant(minimum_total_depth <= UINT64_MAX);
   invariant(maximum_total_depth <= UINT64_MAX);
   assert_equal(test.minimum_total_depth, (u64)minimum_total_depth);
   assert_equal(test.maximum_total_depth, (u64)maximum_total_depth);

   nodePathMetricsRequire((node_path_metrics_t){
       .nodes = test.nodes,
       .total_depth = test.minimum_total_depth,
       .maximum_depth = test.maximum_depth,
   });
   nodePathMetricsRequire((node_path_metrics_t){
       .nodes = test.nodes,
       .total_depth = test.maximum_total_depth,
       .maximum_depth = test.maximum_depth,
   });
}

static void assert_node_path_metric_bounds(void) {
   const node_path_metric_bounds_case_t tests[] = {
       {1, 0, 0, 0},         {2, 1, 1, 1},         {3, 1, 2, 2},
       {3, 2, 3, 3},         {4, 2, 4, 5},         {4, 3, 6, 6},
       {5, 2, 6, 6},         {5, 3, 7, 9},         {5, 4, 10, 10},
       {6, 2, 8, 8},         {6, 3, 9, 11},        {6, 4, 11, 14},
       {6, 5, 15, 15},       {7, 2, 10, 10},       {7, 3, 11, 14},
       {7, 4, 13, 17},       {7, 5, 16, 20},       {7, 6, 21, 21},
       {8, 3, 13, 17},       {8, 4, 15, 21},       {8, 5, 18, 24},
       {8, 6, 22, 27},       {8, 7, 28, 28},       {64, 63, 2016, 2016},
       {65, 64, 2080, 2080}, {66, 64, 2081, 2144},
   };
   const u64 count = sizeof(tests) / sizeof(tests[0]);
   for (u64 i = 0; i < count; i++) {
      assert_node_path_metric_bounds_case(tests[i]);
   }
}

static void
test_small_path_metric_bounds_update(test_small_path_metric_bounds_t *bounds,
                                     u64 total_depth) {
   invariant(bounds != NULL);
   if (!bounds->present) {
      bounds->present = true;
      bounds->minimum_total_depth = total_depth;
      bounds->maximum_total_depth = total_depth;
      return;
   }

   bounds->minimum_total_depth = MIN(bounds->minimum_total_depth, total_depth);
   bounds->maximum_total_depth = MAX(bounds->maximum_total_depth, total_depth);
}

static void assert_node_path_metric_bounds_are_exact_for_small_shapes(void) {
   enum {
      TEST_SMALL_PATH_METRIC_MAX_NODES = 10,
   };
   test_small_path_metric_bounds_t
       bounds[TEST_SMALL_PATH_METRIC_MAX_NODES + 1]
             [TEST_SMALL_PATH_METRIC_MAX_NODES + 1] = {0};
   bounds[1][0] = (test_small_path_metric_bounds_t){
       .present = true,
       .minimum_total_depth = 0,
       .maximum_total_depth = 0,
   };

   for (u64 nodes = 2; nodes <= TEST_SMALL_PATH_METRIC_MAX_NODES; nodes++) {
      for (u64 left_nodes = 0; left_nodes < nodes; left_nodes++) {
         u64 right_nodes = size_after_removing_one(nodes) - left_nodes;
         int left_height_min = left_nodes == 0 ? -1 : 0;
         int left_height_max = left_nodes == 0 ? -1 : (int)left_nodes - 1;
         int right_height_min = right_nodes == 0 ? -1 : 0;
         int right_height_max = right_nodes == 0 ? -1 : (int)right_nodes - 1;

         for (int left_height = left_height_min; left_height <= left_height_max;
              left_height++) {
            test_small_path_metric_bounds_t left = {0};
            if (left_height >= 0) {
               left = bounds[left_nodes][(u64)left_height];
               if (!left.present) {
                  continue;
               }
            }

            for (int right_height = right_height_min;
                 right_height <= right_height_max; right_height++) {
               test_small_path_metric_bounds_t right = {0};
               if (right_height >= 0) {
                  right = bounds[right_nodes][(u64)right_height];
                  if (!right.present) {
                     continue;
                  }
               }

               int maximum_child_height = MAX(left_height, right_height);
               invariant(maximum_child_height >= 0);
               u64 maximum_depth = (u64)(maximum_child_height + 1);
               u64 child_depth_increment = size_sum(left_nodes, right_nodes);
               test_small_path_metric_bounds_update(
                   &bounds[nodes][maximum_depth],
                   size_sum(size_sum(left.minimum_total_depth,
                                     right.minimum_total_depth),
                            child_depth_increment));
               test_small_path_metric_bounds_update(
                   &bounds[nodes][maximum_depth],
                   size_sum(size_sum(left.maximum_total_depth,
                                     right.maximum_total_depth),
                            child_depth_increment));
            }
         }
      }
   }

   for (u64 nodes = 1; nodes <= TEST_SMALL_PATH_METRIC_MAX_NODES; nodes++) {
      for (u64 maximum_depth = 0; maximum_depth < nodes; maximum_depth++) {
         test_small_path_metric_bounds_t exact = bounds[nodes][maximum_depth];
         bool feasible =
             nodes <= nodePathMetricsMaximumNodesForHeight(maximum_depth);
         invariant(exact.present == feasible);
         if (!exact.present) {
            continue;
         }

         __uint128_t minimum_total_depth =
             nodePathMetricsMinimumTotalDepthForHeight(nodes, maximum_depth);
         __uint128_t maximum_total_depth =
             nodePathMetricsMaximumTotalDepthForHeight(nodes, maximum_depth);
         invariant(minimum_total_depth <= UINT64_MAX);
         invariant(maximum_total_depth <= UINT64_MAX);
         assert_equal(exact.minimum_total_depth, (u64)minimum_total_depth);
         assert_equal(exact.maximum_total_depth, (u64)maximum_total_depth);
      }
   }
}

static void TestTreePathMetrics(void) {
   printf("Tree path metrics:\n");

   assert_equal((((u64)1) << 63) - 1, nodePathMetricsMaximumNodesForHeight(62));
   assert_equal(UINT64_MAX, nodePathMetricsMaximumNodesForHeight(63));
   assert_equal(UINT64_MAX, nodePathMetricsMaximumNodesForHeight(64));
   assert_equal((((u64)1) << 63) - 1,
                nodePathMetricsSpineExtraCapacityAtDepth(63));
   assert_equal(UINT64_MAX, nodePathMetricsSpineExtraCapacityAtDepth(64));

   assert_node_path_metric_bounds();
   assert_node_path_metric_bounds_are_exact_for_small_shapes();
   printf("  path metric feasibility bounds OK\n");

   node_path_metrics_t empty = nodePathMetrics(NULL);
   assert_equal(0, empty.nodes);
   assert_equal(0, empty.total_depth);
   assert_equal(0, empty.maximum_depth);
   assert_path_metrics_equal(empty, tree_verify_size_path_metrics(NULL, 0));
   assert_double_equal(0.0, nodeAveragePathLength(NULL));
   assert_int_equal(-1, nodeMaximumPathLength(NULL));
   assert_int_equal(-1, height(NULL));
   invariant(!nodeHeightExceeds(NULL, -1));
   invariant(!nodeHeightExceeds(NULL, 0));
   assert_equal(0, node_size(NULL));
   assert_equal(0, tree_verify_size(NULL, 0));
   assert_equal(0, tree_verify_reachable_size(NULL));
   u64 verify_count = 0;
   assert_equal(0, tree_verify_size_each_node(
                       NULL, 0, test_tree_verify_count_node, &verify_count));
   assert_equal(0, verify_count);
   assert_equal(0, tree_verify_reachable_size_each_node(
                       NULL, test_tree_verify_count_node, &verify_count));
   assert_equal(0, verify_count);
   tree_verify_each_node(NULL, test_tree_verify_count_node, &verify_count);
   assert_equal(0, verify_count);
   inorder_sequence_ctx_t empty_order = {0};
   inorder(NULL, assert_inorder_sequence, &empty_order);
   assert_equal(0, empty_order.count);
   tree_t empty_tree = {0};
   tree_t empty_clone = tree_clone(&empty_tree);
   assert_equal(0, empty_clone.size);
   invariant(empty_clone.root == NULL);
   tree_split_t empty_split = tree_split_base(&empty_tree, 0);
   tree_t empty_left = empty_split.left;
   tree_t empty_right = empty_split.right;
   assert_equal(0, empty_left.size);
   assert_equal(0, empty_right.size);
   invariant(empty_left.root == NULL);
   invariant(empty_right.root == NULL);
   tree_t empty_joined = tree_join_base(&empty_left, &empty_right);
   assert_equal(0, empty_joined.size);
   invariant(empty_joined.root == NULL);
   tree_split_result_t empty_root_split = tree_split_roots(NULL, 0);
   invariant(empty_root_split.left == NULL);
   invariant(empty_root_split.right == NULL);

   tree_t one = right_chain_of_size(1);
   node_path_metrics_t singleton = nodePathMetrics(one.root);
   assert_equal(1, singleton.nodes);
   assert_equal(0, singleton.total_depth);
   assert_equal(0, singleton.maximum_depth);
   assert_path_metrics_equal(singleton,
                             tree_verify_size_path_metrics(one.root, one.size));
   assert_double_equal(0.0, nodeAveragePathLength(one.root));
   assert_int_equal(0, nodeMaximumPathLength(one.root));
   assert_int_equal(0, height(one.root));
   invariant(nodeHeightExceeds(one.root, -1));
   invariant(!nodeHeightExceeds(one.root, 0));
   invariant(!nodeHeightExceeds(one.root, 1));
   assert_equal(1, node_size(one.root));
   assert_equal(one.size, tree_verify_size(one.root, one.size));
   assert_equal(one.size, tree_verify_reachable_size(one.root));
   verify_count = 0;
   assert_equal(one.size, tree_verify_size_each_node(
                              one.root, one.size, test_tree_verify_count_node,
                              &verify_count));
   assert_equal(one.size, verify_count);
   verify_count = 0;
   assert_equal(one.size,
                tree_verify_reachable_size_each_node(
                    one.root, test_tree_verify_count_node, &verify_count));
   assert_equal(one.size, verify_count);
   verify_count = 0;
   tree_verify_each_node(one.root, test_tree_verify_count_node, &verify_count);
   assert_equal(one.size, verify_count);
   inorder_sequence_ctx_t one_order = {0};
   inorder(one.root, assert_inorder_sequence, &one_order);
   assert_equal(1, one_order.count);
   free_nodes(one.root);

   tree_t singleton_min = right_chain_of_size(1);
   node_t *only_min = tree_deleteMin(&singleton_min.root);
   singleton_min.size = size_after_removing_one(singleton_min.size);
   assert_equal(0, singleton_min.size);
   invariant(singleton_min.root == NULL);
   assert_equal(0, only_min->x);
   invariant(only_min->l == NULL);
   invariant(only_min->r == NULL);
   assert_equal(0, tree_verify_size(singleton_min.root, singleton_min.size));
   free_nodes(only_min);

   tree_t singleton_max = right_chain_of_size(1);
   node_t *only_max = tree_deleteMax(&singleton_max.root);
   singleton_max.size = size_after_removing_one(singleton_max.size);
   assert_equal(0, singleton_max.size);
   invariant(singleton_max.root == NULL);
   assert_equal(0, only_max->x);
   invariant(only_max->l == NULL);
   invariant(only_max->r == NULL);
   assert_equal(0, tree_verify_size(singleton_max.root, singleton_max.size));
   free_nodes(only_max);

   const u64 extrema_size = NODE_STACK_INLINE_CAP * 2 + 5;
   tree_t left_chain = left_chain_of_size(extrema_size);
   node_t *min = tree_deleteMin(&left_chain.root);
   left_chain.size = size_after_removing_one(left_chain.size);
   assert_equal(0, min->x);
   invariant(min->l == NULL);
   invariant(min->r == NULL);
   assert_equal(left_chain.size,
                tree_verify_size(left_chain.root, left_chain.size));
   inorder_sequence_ctx_t left_after_min = {.expected = 1};
   inorder(left_chain.root, assert_inorder_sequence, &left_after_min);
   assert_equal(left_chain.size, left_after_min.count);
   free_nodes(min);
   free_nodes(left_chain.root);

   tree_t left_max_source = left_chain_of_size(7);
   node_t *left_chain_max = tree_deleteMax(&left_max_source.root);
   left_max_source.size = size_after_removing_one(left_max_source.size);
   assert_equal(6, left_chain_max->x);
   invariant(left_chain_max->l == NULL);
   invariant(left_chain_max->r == NULL);
   assert_equal(0, left_chain_max->s);
   assert_equal(left_max_source.size,
                tree_verify_size(left_max_source.root, left_max_source.size));
   assert_equal(left_max_source.size,
                tree_verify_reachable_size(left_max_source.root));
   free_nodes(left_chain_max);
   free_nodes(left_max_source.root);

   tree_t right_chain = right_chain_of_size(extrema_size);
   node_t *max = tree_deleteMax(&right_chain.root);
   right_chain.size = size_after_removing_one(right_chain.size);
   assert_equal(extrema_size - 1, max->x);
   invariant(max->l == NULL);
   invariant(max->r == NULL);
   assert_equal(right_chain.size,
                tree_verify_size(right_chain.root, right_chain.size));
   inorder_sequence_ctx_t right_after_max = {0};
   inorder(right_chain.root, assert_inorder_sequence, &right_after_max);
   assert_equal(right_chain.size, right_after_max.count);
   free_nodes(max);
   free_nodes(right_chain.root);

   tree_t sized_min_source = left_chain_of_size(8);
   node_t *sized_min =
       tree_deleteMin_sized(&sized_min_source.root, sized_min_source.size);
   sized_min_source.size = size_after_removing_one(sized_min_source.size);
   assert_equal(0, sized_min->x);
   invariant(sized_min->l == NULL);
   invariant(sized_min->r == NULL);
   assert_equal(sized_min_source.size,
                tree_verify_size(sized_min_source.root, sized_min_source.size));
   free_nodes(sized_min);
   free_nodes(sized_min_source.root);

   tree_t sized_max_source = right_chain_of_size(8);
   node_t *sized_max =
       tree_deleteMax_sized(&sized_max_source.root, sized_max_source.size);
   sized_max_source.size = size_after_removing_one(sized_max_source.size);
   assert_equal(7, sized_max->x);
   invariant(sized_max->l == NULL);
   invariant(sized_max->r == NULL);
   assert_equal(sized_max_source.size,
                tree_verify_size(sized_max_source.root, sized_max_source.size));
   free_nodes(sized_max);
   free_nodes(sized_max_source.root);

   tree_t direct_split_source = right_chain_of_size(6);
   tree_split_result_t direct_split =
       tree_split_roots(direct_split_source.root, 2);
   tree_t direct_split_left = {.root = direct_split.left, .size = 2};
   tree_t direct_split_right = {.root = direct_split.right, .size = 4};
   u64 direct_split_left_values[] = {0, 1};
   u64 direct_split_right_values[] = {2, 3, 4, 5};
   assert_tree_values(direct_split_left, direct_split_left_values,
                      sizeof(direct_split_left_values) /
                          sizeof(direct_split_left_values[0]));
   assert_tree_values(direct_split_right, direct_split_right_values,
                      sizeof(direct_split_right_values) /
                          sizeof(direct_split_right_values[0]));
   free_nodes(direct_split_left.root);
   free_nodes(direct_split_right.root);

   tree_t direct_sized_split_source = right_chain_of_size(6);
   tree_split_result_t direct_sized_split = tree_split_roots_sized(
       direct_sized_split_source.root, 2, direct_sized_split_source.size);
   tree_t direct_sized_split_left = {
       .root = direct_sized_split.left,
       .size = 2,
   };
   tree_t direct_sized_split_right = {
       .root = direct_sized_split.right,
       .size = 4,
   };
   assert_tree_values(direct_sized_split_left, direct_split_left_values,
                      sizeof(direct_split_left_values) /
                          sizeof(direct_split_left_values[0]));
   assert_tree_values(direct_sized_split_right, direct_split_right_values,
                      sizeof(direct_split_right_values) /
                          sizeof(direct_split_right_values[0]));
   free_nodes(direct_sized_split_left.root);
   free_nodes(direct_sized_split_right.root);

   tree_t join_internal_right = right_chain_of_size(2);
   node_t *join_internal_right_root = tree_join_internal(
       NULL, join_internal_right.root, 0, join_internal_right.size);
   tree_t join_internal_right_tree = {
       .root = join_internal_right_root,
       .size = join_internal_right.size,
   };
   u64 join_internal_right_values[] = {0, 1};
   assert_tree_values(join_internal_right_tree, join_internal_right_values,
                      sizeof(join_internal_right_values) /
                          sizeof(join_internal_right_values[0]));
   free_nodes(join_internal_right_tree.root);

   tree_t join_internal_left = right_chain_of_size(2);
   node_t *join_internal_left_root = tree_join_internal(
       join_internal_left.root, NULL, join_internal_left.size, 0);
   tree_t join_internal_left_tree = {
       .root = join_internal_left_root,
       .size = join_internal_left.size,
   };
   u64 join_internal_left_values[] = {0, 1};
   assert_tree_values(join_internal_left_tree, join_internal_left_values,
                      sizeof(join_internal_left_values) /
                          sizeof(join_internal_left_values[0]));
   free_nodes(join_internal_left_tree.root);

   tree_t join_internal_small_left = right_chain_of_size(1);
   tree_t join_internal_large_right = right_chain_of_size(3);
   node_t *join_internal_right_biased_root = tree_join_internal(
       join_internal_small_left.root, join_internal_large_right.root,
       join_internal_small_left.size, join_internal_large_right.size);
   tree_t join_internal_right_biased = {
       .root = join_internal_right_biased_root,
       .size = tree_list_join_size(join_internal_small_left.size,
                                   join_internal_large_right.size),
   };
   u64 join_internal_right_biased_values[] = {0, 0, 1, 2};
   assert_tree_values(join_internal_right_biased,
                      join_internal_right_biased_values,
                      sizeof(join_internal_right_biased_values) /
                          sizeof(join_internal_right_biased_values[0]));
   free_nodes(join_internal_right_biased.root);

   tree_t join_internal_large_left = right_chain_of_size(3);
   tree_t join_internal_small_right = right_chain_of_size(1);
   node_t *join_internal_left_biased_root = tree_join_internal(
       join_internal_large_left.root, join_internal_small_right.root,
       join_internal_large_left.size, join_internal_small_right.size);
   tree_t join_internal_left_biased = {
       .root = join_internal_left_biased_root,
       .size = tree_list_join_size(join_internal_large_left.size,
                                   join_internal_small_right.size),
   };
   u64 join_internal_left_biased_values[] = {0, 1, 2, 0};
   assert_tree_values(join_internal_left_biased,
                      join_internal_left_biased_values,
                      sizeof(join_internal_left_biased_values) /
                          sizeof(join_internal_left_biased_values[0]));
   free_nodes(join_internal_left_biased.root);

   node_t dissolve_leaf = {.x = 41};
   node_t *dissolve_leaf_root = tree_dissolve(&dissolve_leaf, 1);
   invariant(dissolve_leaf_root == NULL);

   node_t dissolve_left_child = {.x = 10};
   node_t dissolve_left_root = {
       .l = &dissolve_left_child,
       .s = 1,
       .x = 11,
   };
   node_t *dissolve_left_result = tree_dissolve(&dissolve_left_root, 2);
   invariant(dissolve_left_result == &dissolve_left_child);
   assert_equal(1, tree_verify_size(dissolve_left_result, 1));
   inorder_sequence_ctx_t dissolve_left_order = {.expected = 10};
   inorder(dissolve_left_result, assert_inorder_sequence, &dissolve_left_order);
   assert_equal(1, dissolve_left_order.count);

   node_t dissolve_right_child = {.x = 13};
   node_t dissolve_right_root = {
       .r = &dissolve_right_child,
       .x = 12,
   };
   node_t *dissolve_right_result = tree_dissolve(&dissolve_right_root, 2);
   invariant(dissolve_right_result == &dissolve_right_child);
   assert_equal(1, tree_verify_size(dissolve_right_result, 1));
   inorder_sequence_ctx_t dissolve_right_order = {.expected = 13};
   inorder(dissolve_right_result, assert_inorder_sequence,
           &dissolve_right_order);
   assert_equal(1, dissolve_right_order.count);

   node_t dissolve_right_left = {.x = 0};
   node_t dissolve_right_tail = {.x = 3};
   node_t dissolve_right_subtree = {
       .r = &dissolve_right_tail,
       .x = 2,
   };
   node_t dissolve_right_biased_root = {
       .l = &dissolve_right_left,
       .r = &dissolve_right_subtree,
       .s = 1,
       .x = 1,
   };
   node_t *dissolve_right_biased_result =
       tree_dissolve(&dissolve_right_biased_root, 4);
   invariant(dissolve_right_biased_result == &dissolve_right_subtree);
   assert_equal(3, tree_verify_size(dissolve_right_biased_result, 3));
   tree_t dissolve_right_biased_tree = {
       .root = dissolve_right_biased_result,
       .size = 3,
   };
   u64 dissolve_right_biased_values[] = {0, 2, 3};
   assert_tree_values(dissolve_right_biased_tree, dissolve_right_biased_values,
                      sizeof(dissolve_right_biased_values) /
                          sizeof(dissolve_right_biased_values[0]));

   node_t dissolve_left_head = {.x = 0};
   node_t dissolve_left_subtree = {
       .l = &dissolve_left_head,
       .s = 1,
       .x = 1,
   };
   node_t dissolve_left_right = {.x = 3};
   node_t dissolve_left_biased_root = {
       .l = &dissolve_left_subtree,
       .r = &dissolve_left_right,
       .s = 2,
       .x = 2,
   };
   node_t *dissolve_left_biased_result =
       tree_dissolve(&dissolve_left_biased_root, 4);
   invariant(dissolve_left_biased_result == &dissolve_left_subtree);
   assert_equal(3, tree_verify_size(dissolve_left_biased_result, 3));
   tree_t dissolve_left_biased_tree = {
       .root = dissolve_left_biased_result,
       .size = 3,
   };
   u64 dissolve_left_biased_values[] = {0, 1, 3};
   assert_tree_values(dissolve_left_biased_tree, dissolve_left_biased_values,
                      sizeof(dissolve_left_biased_values) /
                          sizeof(dissolve_left_biased_values[0]));

   node_t *append_empty_right = NULL;
   node_t *append_right_node = tree_allocate((node_t){.x = 11});
   tree_appendR_raw(&append_empty_right, append_right_node);
   invariant(append_empty_right == append_right_node);
   assert_equal(1, tree_verify_reachable_size(append_empty_right));
   free_nodes(append_empty_right);

   node_t *append_empty_left = NULL;
   node_t *append_left_node = tree_allocate((node_t){.x = 12});
   tree_appendL_raw(&append_empty_left, append_left_node);
   invariant(append_empty_left == append_left_node);
   assert_equal(1, tree_verify_reachable_size(append_empty_left));
   free_nodes(append_empty_left);

   tree_t append_right = right_chain_of_size(3);
   tree_appendR_raw(&append_right.root, NULL);
   assert_equal(3, tree_verify_size(append_right.root, append_right.size));
   node_t *append_right_tail = tree_allocate((node_t){.x = 99});
   tree_appendR_raw(&append_right.root, append_right_tail);
   append_right.size = size_after_inserting_one(append_right.size);
   assert_equal(append_right.size,
                tree_verify_size(append_right.root, append_right.size));
   assert_equal(99, tree_lookup(&append_right, append_right.size - 1));
   free_nodes(append_right.root);

   tree_t append_left = right_chain_of_size(1);
   tree_appendL_raw(&append_left.root, NULL);
   assert_equal(1, tree_verify_size(append_left.root, append_left.size));
   node_t *append_left_head = tree_allocate((node_t){.x = 77});
   tree_appendL_raw(&append_left.root, append_left_head);
   append_left.root->s = size_after_inserting_one(append_left.root->s);
   append_left.size = size_after_inserting_one(append_left.size);
   assert_equal(append_left.size,
                tree_verify_size(append_left.root, append_left.size));
   assert_equal(77, tree_lookup(&append_left, 0));
   free_nodes(append_left.root);

   node_t *replace_right_left = tree_allocate((node_t){.x = 0});
   node_t *replace_right_tail = tree_allocate((node_t){.x = 3});
   node_t *replace_right_right = tree_allocate((node_t){
       .r = replace_right_tail,
       .x = 2,
   });
   node_t *replace_right_root = tree_allocate((node_t){
       .l = replace_right_left,
       .r = replace_right_right,
       .s = 1,
       .x = 1,
   });
   node_t *right_replacement = replace_right_root;
   node_t *right_detached = tree_replacedByRightSubtree(&right_replacement);
   assert_equal(2, tree_verify_size(right_detached, 2));
   assert_equal(2, tree_verify_size(right_replacement, 2));
   invariant(right_detached->r == NULL);
   inorder_sequence_ctx_t right_detached_order = {0};
   inorder(right_detached, assert_inorder_sequence, &right_detached_order);
   assert_equal(2, right_detached_order.count);
   inorder_sequence_ctx_t right_replacement_order = {.expected = 2};
   inorder(right_replacement, assert_inorder_sequence,
           &right_replacement_order);
   assert_equal(2, right_replacement_order.count);
   free_nodes(right_detached);
   free_nodes(right_replacement);

   node_t *sized_replace_right_left = tree_allocate((node_t){.x = 10});
   node_t *sized_replace_right_tail = tree_allocate((node_t){.x = 13});
   node_t *sized_replace_right_right = tree_allocate((node_t){
       .r = sized_replace_right_tail,
       .x = 12,
   });
   node_t *sized_replace_right_root = tree_allocate((node_t){
       .l = sized_replace_right_left,
       .r = sized_replace_right_right,
       .s = 1,
       .x = 11,
   });
   node_t *sized_right_replacement = sized_replace_right_root;
   node_t *sized_right_detached =
       tree_replacedByRightSubtree_sized(&sized_right_replacement, 4);
   assert_equal(2, tree_verify_size(sized_right_detached, 2));
   assert_equal(2, tree_verify_size(sized_right_replacement, 2));
   invariant(sized_right_detached->r == NULL);
   free_nodes(sized_right_detached);
   free_nodes(sized_right_replacement);

   node_t *replace_left_tail = tree_allocate((node_t){.x = 1});
   node_t *replace_left_left = tree_allocate((node_t){
       .r = replace_left_tail,
       .x = 0,
   });
   node_t *replace_left_right = tree_allocate((node_t){.x = 3});
   node_t *replace_left_root = tree_allocate((node_t){
       .l = replace_left_left,
       .r = replace_left_right,
       .s = 2,
       .x = 2,
   });
   node_t *left_replacement = replace_left_root;
   node_t *left_detached = tree_replacedByLeftSubtree(&left_replacement);
   assert_equal(2, tree_verify_size(left_replacement, 2));
   assert_equal(2, tree_verify_size(left_detached, 2));
   invariant(left_detached->l == NULL);
   inorder_sequence_ctx_t left_replacement_order = {0};
   inorder(left_replacement, assert_inorder_sequence, &left_replacement_order);
   assert_equal(2, left_replacement_order.count);
   inorder_sequence_ctx_t left_detached_order = {.expected = 2};
   inorder(left_detached, assert_inorder_sequence, &left_detached_order);
   assert_equal(2, left_detached_order.count);
   free_nodes(left_replacement);
   free_nodes(left_detached);

   node_t *sized_replace_left_tail = tree_allocate((node_t){.x = 21});
   node_t *sized_replace_left_left = tree_allocate((node_t){
       .r = sized_replace_left_tail,
       .x = 20,
   });
   node_t *sized_replace_left_right = tree_allocate((node_t){.x = 23});
   node_t *sized_replace_left_root = tree_allocate((node_t){
       .l = sized_replace_left_left,
       .r = sized_replace_left_right,
       .s = 2,
       .x = 22,
   });
   node_t *sized_left_replacement = sized_replace_left_root;
   node_t *sized_left_detached =
       tree_replacedByLeftSubtree_sized(&sized_left_replacement, 4);
   assert_equal(2, tree_verify_size(sized_left_replacement, 2));
   assert_equal(2, tree_verify_size(sized_left_detached, 2));
   invariant(sized_left_detached->l == NULL);
   free_nodes(sized_left_replacement);
   free_nodes(sized_left_detached);

   const u64 chain_size = 32768;
   tree_t chain = right_chain_of_size(chain_size);
   node_path_metrics_t path = nodePathMetrics(chain.root);
   u64 expected_depth = chain_size - 1;
   u64 expected_total = chain_size * expected_depth / 2;

   assert_equal(chain_size, path.nodes);
   assert_equal(expected_total, path.total_depth);
   assert_equal(expected_depth, path.maximum_depth);
   assert_path_metrics_equal(
       path, tree_verify_size_path_metrics(chain.root, chain.size));
   assert_double_equal((double)expected_total / (double)chain_size,
                       nodeAveragePathLength(chain.root));
   assert_int_equal((int)expected_depth, nodeMaximumPathLength(chain.root));
   assert_int_equal((int)expected_depth, height(chain.root));
   invariant(nodeHeightExceeds(chain.root, (int)expected_depth - 1));
   invariant(!nodeHeightExceeds(chain.root, (int)expected_depth));
   assert_equal(chain_size, node_size(chain.root));
   assert_equal(chain.size, tree_verify_size(chain.root, chain.size));
   assert_equal(chain.size, tree_verify_reachable_size(chain.root));
   verify_count = 0;
   assert_equal(chain.size, tree_verify_size_each_node(
                                chain.root, chain.size,
                                test_tree_verify_count_node, &verify_count));
   assert_equal(chain.size, verify_count);
   verify_count = 0;
   assert_equal(chain.size,
                tree_verify_reachable_size_each_node(
                    chain.root, test_tree_verify_count_node, &verify_count));
   assert_equal(chain.size, verify_count);
   inorder_sequence_ctx_t chain_order = {0};
   inorder(chain.root, assert_inorder_sequence, &chain_order);
   assert_equal(chain_size, chain_order.count);

   tree_t clone = tree_clone(&chain);
   assert_equal(chain.size, clone.size);
   assert_equal(clone.size, tree_verify_size(clone.root, clone.size));
   assert_equal(chain_size, node_size(clone.root));
   inorder_sequence_ctx_t clone_order = {0};
   inorder(clone.root, assert_inorder_sequence, &clone_order);
   assert_equal(chain_size, clone_order.count);
#ifdef PERSISTENT
   invariant(clone.root == chain.root);
   invariant(tree_shared(chain.root));
#else
   invariant(clone.root != chain.root);
#endif
   free_nodes(clone.root);
   assert_equal(chain.size, tree_verify_size(chain.root, chain.size));

   free_nodes(chain.root);

   tree_t split_source = right_chain_of_size(17);
   tree_split_t split = tree_split_base(&split_source, 7);
   tree_t split_left = split.left;
   tree_t split_right = split.right;
   assert_equal(7, split_left.size);
   assert_equal(10, split_right.size);
#ifndef PERSISTENT
   assert_equal(0, split_source.size);
   invariant(split_source.root == NULL);
#endif
   assert_equal(split_left.size,
                tree_verify_size(split_left.root, split_left.size));
   assert_equal(split_right.size,
                tree_verify_size(split_right.root, split_right.size));
   inorder_sequence_ctx_t left_order = {0};
   inorder(split_left.root, assert_inorder_sequence, &left_order);
   assert_equal(split_left.size, left_order.count);
   inorder_sequence_ctx_t right_order = {.expected = 7};
   inorder(split_right.root, assert_inorder_sequence, &right_order);
   assert_equal(split_right.size, right_order.count);
#ifdef PERSISTENT
   assert_equal(split_source.size,
                tree_verify_size(split_source.root, split_source.size));
#endif

   tree_t split_joined = tree_join_base(&split_left, &split_right);
   assert_equal(17, split_joined.size);
#ifndef PERSISTENT
   assert_equal(0, split_left.size);
   assert_equal(0, split_right.size);
   invariant(split_left.root == NULL);
   invariant(split_right.root == NULL);
#endif
   assert_equal(split_joined.size,
                tree_verify_size(split_joined.root, split_joined.size));
   inorder_sequence_ctx_t joined_order = {0};
   inorder(split_joined.root, assert_inorder_sequence, &joined_order);
   assert_equal(split_joined.size, joined_order.count);
   free_nodes(split_joined.root);
#ifdef PERSISTENT
   free_nodes(split_left.root);
   free_nodes(split_right.root);
   free_nodes(split_source.root);
#endif

   tree_t none_source = right_chain_of_size(9);
   tree_split_t none_split = tree_split_base(&none_source, 0);
   tree_t none_left = none_split.left;
   tree_t none_right = none_split.right;
   assert_equal(0, none_left.size);
   assert_equal(9, none_right.size);
#ifndef PERSISTENT
   assert_equal(0, none_source.size);
   invariant(none_source.root == NULL);
#endif
   invariant(none_left.root == NULL);
   assert_equal(none_right.size,
                tree_verify_size(none_right.root, none_right.size));
   inorder_sequence_ctx_t none_right_order = {0};
   inorder(none_right.root, assert_inorder_sequence, &none_right_order);
   assert_equal(none_right.size, none_right_order.count);
   tree_t none_joined = tree_join_base(&none_left, &none_right);
   assert_equal(9, none_joined.size);
   assert_equal(none_joined.size,
                tree_verify_size(none_joined.root, none_joined.size));
   inorder_sequence_ctx_t none_joined_order = {0};
   inorder(none_joined.root, assert_inorder_sequence, &none_joined_order);
   assert_equal(none_joined.size, none_joined_order.count);
   free_nodes(none_joined.root);
#ifdef PERSISTENT
   free_nodes(none_left.root);
   free_nodes(none_right.root);
   free_nodes(none_source.root);
#endif

   tree_t all_source = right_chain_of_size(9);
   tree_split_t all_split = tree_split_base(&all_source, all_source.size);
   tree_t all_left = all_split.left;
   tree_t all_right = all_split.right;
   assert_equal(9, all_left.size);
   assert_equal(0, all_right.size);
#ifndef PERSISTENT
   assert_equal(0, all_source.size);
   invariant(all_source.root == NULL);
#endif
   assert_equal(all_left.size, tree_verify_size(all_left.root, all_left.size));
   invariant(all_right.root == NULL);
   inorder_sequence_ctx_t all_left_order = {0};
   inorder(all_left.root, assert_inorder_sequence, &all_left_order);
   assert_equal(all_left.size, all_left_order.count);
   tree_t all_joined = tree_join_base(&all_left, &all_right);
   assert_equal(9, all_joined.size);
   assert_equal(all_joined.size,
                tree_verify_size(all_joined.root, all_joined.size));
   inorder_sequence_ctx_t all_joined_order = {0};
   inorder(all_joined.root, assert_inorder_sequence, &all_joined_order);
   assert_equal(all_joined.size, all_joined_order.count);
   free_nodes(all_joined.root);
#ifdef PERSISTENT
   free_nodes(all_left.root);
   free_nodes(all_right.root);
   free_nodes(all_source.root);
#endif

   tree_t partition_source = perfect_ranked_tree(5);
   u64 pivot = 19;
   partition_source.root = tree_partition_sized(partition_source.root,
                                                partition_source.size, pivot);
   assert_equal(partition_source.size,
                tree_verify_size(partition_source.root, partition_source.size));
   invariant(partition_source.root != NULL);
   assert_equal(pivot, partition_source.root->s);
   assert_equal(pivot, partition_source.root->x);
   inorder_sequence_ctx_t partition_order = {0};
   inorder(partition_source.root, assert_inorder_sequence, &partition_order);
   assert_equal(partition_source.size, partition_order.count);
   free_nodes(partition_source.root);

   assert_aborts(test_tree_verify_null_nonzero_child);
   assert_aborts(test_tree_verify_root_zero_child);
   assert_aborts(test_tree_verify_left_size_out_of_bounds_child);
   assert_aborts(test_tree_verify_wrong_right_size_child);
   assert_aborts(test_tree_verify_each_node_null_verify_child);
   assert_aborts(test_tree_verify_size_each_node_mutating_child);
   assert_aborts(test_tree_verify_reachable_size_each_node_mutating_child);
   assert_aborts(test_tree_verify_each_node_mutating_child);
   assert_aborts(test_tree_verify_duplicate_child_child);
   assert_aborts(test_tree_verify_reachable_duplicate_child_child);
   assert_aborts(test_tree_verify_reachable_wrong_left_size_child);
   assert_aborts(test_tree_verify_duplicate_after_seen_grow_child);
   assert_aborts(test_tree_verify_reachable_duplicate_after_seen_grow_child);
   assert_aborts(test_tree_node_size_duplicate_child_child);
   assert_aborts(test_tree_node_size_duplicate_after_seen_grow_child);
   assert_aborts(test_tree_path_metrics_duplicate_child_child);
   assert_aborts(test_tree_path_metrics_duplicate_after_seen_grow_child);
   assert_aborts(test_node_path_metrics_empty_total_child);
   assert_aborts(test_node_path_metrics_singleton_total_child);
   assert_aborts(test_node_path_metrics_multi_zero_max_child);
   assert_aborts(test_node_path_metrics_total_too_shallow_child);
   assert_aborts(test_node_path_metrics_total_too_shallow_for_count_child);
   assert_aborts(test_node_path_metrics_total_too_shallow_for_height_child);
   assert_aborts(
       test_node_path_metrics_total_too_shallow_for_count_and_height_child);
   assert_aborts(test_node_path_metrics_height_too_low_child);
   assert_aborts(test_node_path_metrics_total_too_deep_child);
   assert_aborts(test_node_path_metrics_total_too_deep_for_height_child);
   assert_aborts(test_node_path_metrics_max_depth_too_large_child);
   assert_aborts(test_node_path_metrics_height_overflow_child);
   assert_aborts(test_tree_height_exceeds_duplicate_child_child);
#ifdef DEBUG
   assert_aborts(test_tree_height_exceeds_duplicate_below_bound_child);
#endif
   assert_aborts(test_tree_height_exceeds_duplicate_after_seen_grow_child);
   assert_aborts(test_tree_height_duplicate_child_child);
   assert_aborts(test_tree_height_duplicate_after_seen_grow_child);
   assert_aborts(test_node_maximum_path_length_duplicate_child_child);
   assert_aborts(test_node_maximum_path_length_duplicate_after_seen_grow_child);
   assert_aborts(test_tree_clone_duplicate_child_child);
   assert_aborts(test_tree_clone_duplicate_after_seen_grow_child);
   assert_aborts(test_tree_clone_physical_root_size_mismatch_child);
   assert_aborts(test_tree_clone_root_size_mismatch_child);
   assert_aborts(test_tree_clone_wrong_left_size_child);
   assert_aborts(test_tree_clone_wrong_right_size_child);
   assert_aborts(test_tree_join_internal_left_zero_size_child);
   assert_aborts(test_tree_join_internal_right_zero_size_child);
   assert_aborts(test_tree_join_internal_total_size_overflow_child);
   assert_aborts(test_tree_join_internal_left_size_without_child_child);
   assert_aborts(test_tree_join_internal_right_size_without_child_child);
   assert_aborts(test_tree_join_internal_linked_left_size_without_child_child);
   assert_aborts(test_tree_join_internal_linked_right_size_without_child_child);
#ifdef DEBUG
   assert_aborts(test_tree_join_internal_overlap_child);
#endif
   assert_aborts(test_tree_free_nodes_duplicate_child_child);
#ifdef DEBUG
   assert_aborts(test_tree_inorder_duplicate_child_child);
   assert_aborts(test_tree_inorder_duplicate_after_seen_grow_child);
#endif
   assert_aborts(test_tree_lookup_null_tree_child);
   assert_aborts(test_tree_lookup_empty_tree_child);
   assert_aborts(test_tree_lookup_left_size_without_child_child);
   assert_aborts(test_tree_update_null_tree_child);
   assert_aborts(test_tree_update_empty_tree_child);
   assert_aborts(test_tree_update_zero_left_size_with_child_child);
   assert_aborts(test_tree_insert_null_tree_child);
   assert_aborts(test_tree_insert_after_end_child);
   assert_aborts(test_tree_insert_internal_null_pointer_child);
   assert_aborts(test_tree_insert_internal_empty_nonzero_size_child);
   assert_aborts(test_tree_insert_internal_after_end_child);
   assert_aborts(test_tree_insert_internal_right_size_without_child_child);
   assert_aborts(test_tree_delete_null_tree_child);
   assert_aborts(test_tree_delete_empty_tree_child);
   assert_aborts(test_tree_delete_internal_null_pointer_child);
   assert_aborts(test_tree_delete_internal_null_root_child);
   assert_aborts(test_tree_delete_internal_after_end_child);
   assert_aborts(test_tree_delete_internal_wrong_size_child);
   assert_aborts(test_tree_delete_internal_left_size_without_child_child);
   assert_aborts(test_tree_delete_min_sized_left_size_without_child_child);
   assert_aborts(test_tree_delete_min_sized_right_size_without_child_child);
   assert_aborts(test_tree_delete_max_sized_left_size_without_child_child);
   assert_aborts(test_tree_delete_max_sized_right_size_without_child_child);
   assert_aborts(test_tree_split_null_tree_child);
   assert_aborts(test_tree_split_after_end_child);
   assert_aborts(test_tree_split_roots_null_nonzero_child);
   assert_aborts(test_tree_split_roots_after_end_child);
   assert_aborts(test_tree_split_roots_left_size_without_child_child);
   assert_aborts(test_tree_split_roots_sized_null_nonzero_child);
   assert_aborts(test_tree_split_roots_sized_after_end_child);
#ifdef DEBUG
   assert_aborts(test_tree_split_roots_sized_wrong_reachable_size_child);
#endif
   assert_aborts(test_tree_join_null_left_child);
   assert_aborts(test_tree_join_null_right_child);
   assert_aborts(test_tree_join_same_tree_child);
   assert_aborts(test_tree_dissolve_null_child);
   assert_aborts(test_tree_dissolve_zero_size_child);
   assert_aborts(test_tree_dissolve_rank_out_of_bounds_child);
   assert_aborts(test_tree_dissolve_left_size_without_child_child);
   assert_aborts(test_tree_dissolve_zero_left_size_with_child_child);
   assert_aborts(test_tree_dissolve_right_size_without_child_child);
#ifdef PERSISTENT
   assert_aborts(test_tree_dissolve_shared_child);
#endif
   assert_aborts(test_tree_appendR_raw_null_pointer_child);
   assert_aborts(test_tree_appendL_raw_null_pointer_child);
#ifdef DEBUG
   assert_aborts(test_tree_appendR_raw_reachable_append_root_child);
   assert_aborts(test_tree_appendL_raw_reachable_append_root_child);
#endif
   assert_aborts(
       test_tree_replacedByRightSubtree_sized_left_size_without_child_child);
   assert_aborts(
       test_tree_replacedByRightSubtree_sized_right_size_without_child_child);
   assert_aborts(
       test_tree_replacedByLeftSubtree_sized_left_size_without_child_child);
   assert_aborts(
       test_tree_replacedByLeftSubtree_sized_right_size_without_child_child);
   assert_aborts(test_tree_partition_sized_null_child);
   assert_aborts(test_tree_partition_sized_zero_size_child);
   assert_aborts(test_tree_partition_sized_after_end_child);
   assert_aborts(test_tree_partition_sized_rank_out_of_bounds_child);
   assert_aborts(test_tree_partition_sized_left_size_without_child_child);
   assert_aborts(test_tree_partition_sized_zero_left_size_with_child_child);
   assert_aborts(test_tree_partition_sized_right_size_without_child_child);
   assert_aborts(test_tree_partition_null_child);
   printf("  public tree entrypoint guards OK\n");

   printf("  degenerate trees OK\n");
}

static void test_suffix_size_after_prefix_underflow_child(void) {
   (void)suffix_size_after_prefix(7, 8);
}

static void test_suffix_size_after_rank_out_of_bounds_child(void) {
   (void)suffix_size_after_rank(7, 7);
}

static void test_sequence_split_inputs_after_end_child(void) {
   (void)sequence_split_inputs_require(1, 0);
}

static void test_sequence_insert_inputs_after_end_child(void) {
   (void)sequence_insert_inputs_require(1, 0);
}

static void test_sequence_insert_position_count_full_child(void) {
   (void)sequence_insert_position_count(UINT64_MAX);
}

static void test_sequence_insert_inputs_full_child(void) {
   (void)sequence_insert_inputs_require(0, UINT64_MAX);
}

static void test_sequence_rank_inputs_empty_child(void) {
   sequence_rank_inputs_require(0, 0);
}

static void test_sequence_rank_inputs_after_end_child(void) {
   sequence_rank_inputs_require(7, 7);
}

static void test_sequence_delete_inputs_empty_child(void) {
   (void)sequence_delete_inputs_require(0, 0);
}

static void test_sequence_delete_inputs_after_end_child(void) {
   (void)sequence_delete_inputs_require(7, 7);
}

static void test_size_sum_overflow_child(void) {
   (void)size_sum(UINT64_MAX, 1);
}

static void test_size_after_inserting_one_overflow_child(void) {
   (void)size_after_inserting_one(UINT64_MAX);
}

static void test_size_after_removing_one_empty_child(void) {
   (void)size_after_removing_one(0);
}

static void test_sizeL_null_child(void) {
   (void)sizeL(NULL);
}

static void test_sizeR_null_child(void) {
   (void)sizeR(NULL, 1);
}

static void test_sizeR_out_of_bounds_child(void) {
   node_t node = {.s = 1};
   (void)sizeR(&node, 1);
}

static void test_tree_size_from_child_sizes_overflow_child(void) {
   (void)tree_size_from_child_sizes(UINT64_MAX, 0);
}

static void test_tree_size_from_child_sizes_exact_overflow_child(void) {
   (void)tree_size_from_child_sizes(UINT64_MAX - 1, 1);
}

static void test_tree_handle_require_root_size_mismatch_child(void) {
   node_t root = {0};
   tree_handle_require((tree_t){.root = &root, .size = 0});
   bst_unreachable();
}

static void test_tree_sized_handle_require_left_size_out_of_bounds_child(void) {
   node_t root = {.s = 1};
   tree_sized_handle_require((tree_t){.root = &root, .size = 1});
   bst_unreachable();
}

static void test_tree_sized_handle_require_left_size_without_child_child(void) {
   node_t root = {.s = 1};
   tree_sized_handle_require((tree_t){.root = &root, .size = 2});
   bst_unreachable();
}

static void
test_tree_sized_handle_require_right_size_without_child_child(void) {
   node_t root = {0};
   tree_sized_handle_require((tree_t){.root = &root, .size = 2});
   bst_unreachable();
}

static void test_tree_root_size_require_root_size_mismatch_child(void) {
   node_t root = {0};
   tree_root_size_require(&root, 0);
}

static void test_tree_sized_root_require_root_size_mismatch_child(void) {
   node_t root = {0};
   tree_sized_root_require(&root, 0);
}

static void test_tree_sized_root_require_left_size_out_of_bounds_child(void) {
   node_t root = {.s = 1};
   tree_sized_root_require(&root, 1);
}

static void test_tree_nonempty_root_require_empty_child(void) {
   tree_nonempty_root_require(NULL, 0);
}

static void
test_tree_nonempty_root_require_left_size_out_of_bounds_child(void) {
   node_t root = {.s = 1};
   tree_nonempty_root_require(&root, 1);
}

static void test_tree_split_inputs_after_end_child(void) {
   tree_split_inputs_require(NULL, 1, 0);
}

static void test_tree_split_inputs_root_size_mismatch_child(void) {
   node_t root = {0};
   tree_split_inputs_require(&root, 0, 0);
}

static void test_tree_split_inputs_left_size_out_of_bounds_child(void) {
   node_t root = {.s = 1};
   tree_split_inputs_require(&root, 0, 1);
}

static void test_tree_split_inputs_left_size_without_child_child(void) {
   node_t root = {.s = 1};
   tree_split_inputs_require(&root, 0, 2);
}

static void test_tree_split_inputs_right_size_without_child_child(void) {
   node_t root = {0};
   tree_split_inputs_require(&root, 0, 2);
}

static void test_tree_insert_inputs_after_end_child(void) {
   tree_insert_inputs_require(NULL, 1, 0);
}

static void test_tree_insert_inputs_root_size_mismatch_child(void) {
   node_t root = {0};
   tree_insert_inputs_require(&root, 0, 0);
}

static void test_tree_insert_inputs_left_size_out_of_bounds_child(void) {
   node_t root = {.s = 1};
   tree_insert_inputs_require(&root, 0, 1);
}

static void test_tree_insert_inputs_left_size_without_child_child(void) {
   node_t root = {.s = 1};
   tree_insert_inputs_require(&root, 0, 2);
}

static void test_tree_insert_inputs_right_size_without_child_child(void) {
   node_t root = {0};
   tree_insert_inputs_require(&root, 0, 2);
}

static void test_tree_insert_inputs_full_child(void) {
   node_t root = {0};
   tree_insert_inputs_require(&root, 0, UINT64_MAX);
}

static void test_tree_rank_inputs_empty_child(void) {
   tree_rank_inputs_require(NULL, 0, 0);
}

static void test_tree_rank_inputs_after_end_child(void) {
   node_t root = {0};
   tree_rank_inputs_require(&root, 1, 1);
}

static void test_tree_rank_inputs_left_size_out_of_bounds_child(void) {
   node_t root = {.s = 1};
   tree_rank_inputs_require(&root, 0, 1);
}

static void test_tree_rank_inputs_left_size_without_child_child(void) {
   node_t root = {.s = 1};
   tree_rank_inputs_require(&root, 1, 2);
}

static void test_tree_rank_inputs_right_size_without_child_child(void) {
   node_t root = {0};
   tree_rank_inputs_require(&root, 0, 2);
}

static void test_tree_delete_inputs_empty_child(void) {
   tree_delete_inputs_require(NULL, 0, 0);
}

static void test_tree_delete_inputs_after_end_child(void) {
   node_t root = {0};
   tree_delete_inputs_require(&root, 1, 1);
}

static void test_tree_delete_inputs_left_size_out_of_bounds_child(void) {
   node_t root = {.s = 1};
   tree_delete_inputs_require(&root, 0, 1);
}

static void test_tree_delete_inputs_left_size_without_child_child(void) {
   node_t root = {.s = 1};
   tree_delete_inputs_require(&root, 1, 2);
}

static void test_tree_delete_inputs_right_size_without_child_child(void) {
   node_t root = {0};
   tree_delete_inputs_require(&root, 0, 2);
}

static void test_tree_join_inputs_root_size_mismatch_child(void) {
   node_t right = {0};
   tree_join_inputs_require(NULL, &right, 1, 1);
}

static void test_tree_join_inputs_left_size_out_of_bounds_child(void) {
   node_t left = {.s = 1};
   tree_join_inputs_require(&left, NULL, 1, 0);
}

static void test_tree_join_inputs_right_size_out_of_bounds_child(void) {
   node_t right = {.s = 1};
   tree_join_inputs_require(NULL, &right, 0, 1);
}

static void test_tree_join_inputs_left_size_without_child_child(void) {
   node_t left = {.s = 1};
   tree_join_inputs_require(&left, NULL, 2, 0);
}

static void test_tree_join_inputs_right_size_without_child_child(void) {
   node_t right = {0};
   tree_join_inputs_require(NULL, &right, 0, 2);
}

static void test_tree_join_inputs_total_size_overflow_child(void) {
   node_t left = {0};
   node_t right = {0};
   tree_join_inputs_require(&left, &right, UINT64_MAX, 1);
}

static void test_tree_build_inputs_null_pivot_child(void) {
   tree_build_inputs_require(NULL, NULL, NULL, 0, 0);
}

static void test_tree_build_inputs_root_size_mismatch_child(void) {
   node_t pivot = {0};
   node_t right = {0};
   tree_build_inputs_require(NULL, &pivot, &right, 1, 1);
}

static void test_tree_build_inputs_left_size_out_of_bounds_child(void) {
   node_t left = {.s = 1};
   node_t pivot = {0};
   tree_build_inputs_require(&left, &pivot, NULL, 1, 0);
}

static void test_tree_build_inputs_right_size_out_of_bounds_child(void) {
   node_t pivot = {0};
   node_t right = {.s = 1};
   tree_build_inputs_require(NULL, &pivot, &right, 0, 1);
}

static void test_tree_build_inputs_left_size_without_child_child(void) {
   node_t left = {.s = 1};
   node_t pivot = {0};
   tree_build_inputs_require(&left, &pivot, NULL, 2, 0);
}

static void test_tree_build_inputs_right_size_without_child_child(void) {
   node_t pivot = {0};
   node_t right = {0};
   tree_build_inputs_require(NULL, &pivot, &right, 0, 2);
}

static void test_tree_build_inputs_total_size_overflow_child(void) {
   node_t left = {0};
   node_t pivot = {0};
   node_t right = {0};
   tree_build_inputs_require(&left, &pivot, &right, UINT64_MAX - 1, 1);
}

static void test_rank_from_end_out_of_bounds_child(void) {
   (void)rank_from_end(7, 7);
}

static void test_rank_of_last_empty_child(void) {
   (void)rank_of_last(0);
}

static void test_rank_in_right_subtree_underflow_child(void) {
   (void)rank_in_right_subtree(7, 7);
}

static void test_rank_after_predecessor_overflow_child(void) {
   node_t subtree = {.s = 1};
   (void)rank_after_predecessor(UINT64_MAX - 1, &subtree);
}

static void test_rank_after_predecessor_max_child(void) {
   node_t subtree = {0};
   (void)rank_after_predecessor(UINT64_MAX, &subtree);
}

static void test_rank_after_predecessor_null_child(void) {
   (void)rank_after_predecessor(0, NULL);
}

static void test_rank_after_predecessor_exact_overflow_child(void) {
   node_t subtree = {.s = 1};
   (void)rank_after_predecessor(UINT64_MAX - 2, &subtree);
}

static void test_rank_after_prefix_overflow_child(void) {
   node_t subtree = {.s = 1};
   (void)rank_after_prefix(UINT64_MAX, &subtree);
}

static void test_rank_after_prefix_null_child(void) {
   (void)rank_after_prefix(0, NULL);
}

static void test_rank_after_prefix_exact_overflow_child(void) {
   node_t subtree = {.s = 1};
   (void)rank_after_prefix(UINT64_MAX - 1, &subtree);
}

static void test_isLeaf_null_child(void) {
   (void)isLeaf(NULL);
}

static void test_insertL_null_child(void) {
   (void)insertL(NULL);
}

static void test_insertL_overflow_child(void) {
   node_t node = {.s = UINT64_MAX};
   (void)insertL(&node);
}

static void test_insertR_null_node_child(void) {
   u64 rank = 1;
   (void)insertR(NULL, &rank);
}

static void test_insertR_null_rank_child(void) {
   node_t node = {0};
   (void)insertR(&node, NULL);
}

static void test_insertR_not_right_child(void) {
   node_t node = {.s = 2};
   u64 rank = 2;
   (void)insertR(&node, &rank);
}

static void test_deleteL_null_child(void) {
   (void)deleteL(NULL);
}

static void test_deleteL_empty_child(void) {
   node_t node = {0};
   (void)deleteL(&node);
}

static void test_deleteR_null_node_child(void) {
   u64 rank = 1;
   (void)deleteR(NULL, &rank);
}

static void test_deleteR_null_rank_child(void) {
   node_t node = {0};
   (void)deleteR(&node, NULL);
}

static void test_deleteR_not_right_child(void) {
   node_t node = {.s = 2};
   u64 rank = 2;
   (void)deleteR(&node, &rank);
}

static void test_tree_pathLeft_null_pointer_child(void) {
   tree_pathLeft(NULL);
}

static void test_tree_pathLeft_null_link_child(void) {
   node_t **cursor = NULL;
   tree_pathLeft(&cursor);
}

static void test_tree_pathLeft_null_node_child(void) {
   node_t *root = NULL;
   node_t **cursor = &root;
   tree_pathLeft(&cursor);
}

static void test_tree_pathLeft_missing_child_child(void) {
   node_t root = {0};
   node_t *root_ptr = &root;
   node_t **cursor = &root_ptr;
   tree_pathLeft(&cursor);
}

static void test_tree_pathLeft_zero_size_left_child(void) {
   node_t child = {0};
   node_t root = {.l = &child};
   node_t *root_ptr = &root;
   node_t **cursor = &root_ptr;
   tree_pathLeft(&cursor);
}

static void test_tree_pathLeft_overflow_child(void) {
   node_t child = {0};
   node_t root = {.l = &child, .s = UINT64_MAX};
   node_t *root_ptr = &root;
   node_t **cursor = &root_ptr;
   tree_pathLeft(&cursor);
}

static void test_tree_pathRight_null_pointer_child(void) {
   u64 rank = 1;
   tree_pathRight(NULL, &rank);
}

static void test_tree_pathRight_null_link_child(void) {
   node_t **cursor = NULL;
   u64 rank = 1;
   tree_pathRight(&cursor, &rank);
}

static void test_tree_pathRight_null_node_child(void) {
   node_t *root = NULL;
   node_t **cursor = &root;
   u64 rank = 1;
   tree_pathRight(&cursor, &rank);
}

static void test_tree_pathRight_null_rank_child(void) {
   node_t child = {0};
   node_t root = {.r = &child};
   node_t *root_ptr = &root;
   node_t **cursor = &root_ptr;
   tree_pathRight(&cursor, NULL);
}

static void test_tree_pathRight_not_right_child(void) {
   node_t child = {0};
   node_t root = {.r = &child, .s = 2};
   node_t *root_ptr = &root;
   node_t **cursor = &root_ptr;
   u64 rank = 2;
   tree_pathRight(&cursor, &rank);
}

static void test_tree_pathRight_missing_child_child(void) {
   node_t root = {0};
   node_t *root_ptr = &root;
   node_t **cursor = &root_ptr;
   u64 rank = 1;
   tree_pathRight(&cursor, &rank);
}

static void test_tree_pathRight_left_size_without_child_child(void) {
   node_t child = {0};
   node_t root = {.r = &child, .s = 1};
   node_t *root_ptr = &root;
   node_t **cursor = &root_ptr;
   u64 rank = 2;
   tree_pathRight(&cursor, &rank);
}

static void test_tree_pathLeftSized_right_size_without_child_child(void) {
   node_t child = {0};
   node_t root = {.l = &child, .s = 1};
   node_t *root_ptr = &root;
   node_t **cursor = &root_ptr;
   u64 size = 3;
   tree_pathLeftSized(&cursor, &size);
}

static void test_tree_pathRightSized_left_size_without_child_child(void) {
   node_t child = {.s = 1};
   node_t root = {.r = &child, .s = 0};
   node_t *root_ptr = &root;
   node_t **cursor = &root_ptr;
   u64 size = 3;
   u64 rank = 1;
   tree_pathRightSized(&cursor, &size, &rank);
}

static void test_tree_attach_null_pointer_child(void) {
   tree_attach(NULL, 0);
}

static void test_tree_attach_occupied_child(void) {
   node_t node = {0};
   node_t *root = &node;
   tree_attach(&root, 0);
}

static void test_tree_attachL_null_child(void) {
   tree_attachL(NULL, 0);
}

static void test_tree_attachL_existing_child_child(void) {
   node_t child = {0};
   node_t root = {.l = &child};
   tree_attachL(&root, 0);
}

static void test_tree_attachL_overflow_child(void) {
   node_t root = {.s = UINT64_MAX};
   tree_attachL(&root, 0);
}

static void test_tree_attachR_null_child(void) {
   tree_attachR(NULL, 0);
}

static void test_tree_attachR_existing_child_child(void) {
   node_t child = {0};
   node_t root = {.r = &child};
   tree_attachR(&root, 0);
}

static void test_tree_attachLL_missing_left_child(void) {
   node_t root = {0};
   tree_attachLL(&root, 0);
}

static void test_tree_attachLL_existing_child_child(void) {
   node_t grandchild = {0};
   node_t child = {.l = &grandchild};
   node_t root = {.l = &child, .s = 1};
   tree_attachLL(&root, 0);
}

static void test_tree_attachLL_root_overflow_child(void) {
   node_t child = {0};
   node_t root = {.l = &child, .s = UINT64_MAX};
   tree_attachLL(&root, 0);
}

static void test_tree_attachLL_child_overflow_child(void) {
   node_t child = {.s = UINT64_MAX};
   node_t root = {.l = &child, .s = 1};
   tree_attachLL(&root, 0);
}

static void test_tree_attachRR_missing_right_child(void) {
   node_t root = {0};
   tree_attachRR(&root, 0);
}

static void test_tree_attachRR_existing_child_child(void) {
   node_t grandchild = {0};
   node_t child = {.r = &grandchild};
   node_t root = {.r = &child};
   tree_attachRR(&root, 0);
}

static void test_tree_attachLR_missing_left_child(void) {
   node_t root = {0};
   tree_attachLR(&root, 0);
}

static void test_tree_attachLR_existing_child_child(void) {
   node_t grandchild = {0};
   node_t child = {.r = &grandchild};
   node_t root = {.l = &child, .s = 1};
   tree_attachLR(&root, 0);
}

static void test_tree_attachLR_root_overflow_child(void) {
   node_t child = {0};
   node_t root = {.l = &child, .s = UINT64_MAX};
   tree_attachLR(&root, 0);
}

static void test_tree_attachRL_missing_right_child(void) {
   node_t root = {0};
   tree_attachRL(&root, 0);
}

static void test_tree_attachRL_existing_child_child(void) {
   node_t grandchild = {0};
   node_t child = {.l = &grandchild};
   node_t root = {.r = &child};
   tree_attachRL(&root, 0);
}

static void test_tree_attachRL_child_overflow_child(void) {
   node_t child = {.s = UINT64_MAX};
   node_t root = {.r = &child};
   tree_attachRL(&root, 0);
}

static void test_treeRotateL_null_pointer_child(void) {
   treeRotateL(NULL);
}

static void test_treeRotateL_null_node_child(void) {
   node_t *root = NULL;
   treeRotateL(&root);
}

static void test_treeRotateL_missing_right_child(void) {
   node_t root = {0};
   node_t *root_ptr = &root;
   treeRotateL(&root_ptr);
}

static void test_treeRotateR_null_pointer_child(void) {
   treeRotateR(NULL);
}

static void test_treeRotateR_null_node_child(void) {
   node_t *root = NULL;
   treeRotateR(&root);
}

static void test_treeRotateR_missing_left_child(void) {
   node_t root = {0};
   node_t *root_ptr = &root;
   treeRotateR(&root_ptr);
}

static void test_treeRotateLR_missing_left_child(void) {
   node_t root = {0};
   node_t *root_ptr = &root;
   treeRotateLR(&root_ptr);
}

static void test_treeRotateLR_missing_middle_child(void) {
   node_t left = {0};
   node_t root = {.l = &left, .s = 1};
   node_t *root_ptr = &root;
   treeRotateLR(&root_ptr);
}

static void test_treeRotateRL_missing_right_child(void) {
   node_t root = {0};
   node_t *root_ptr = &root;
   treeRotateRL(&root_ptr);
}

static void test_treeRotateRL_missing_middle_child(void) {
   node_t right = {0};
   node_t root = {.r = &right};
   node_t *root_ptr = &root;
   treeRotateRL(&root_ptr);
}

static void test_treeRotateLSized_left_size_without_child_child(void) {
   node_t right = {0};
   node_t root = {.r = &right, .s = 1};
   node_t *root_ptr = &root;
   treeRotateLSized(&root_ptr, 3);
}

static void test_treeRotateRSized_zero_left_size_with_child_child(void) {
   node_t left = {0};
   node_t root = {.l = &left};
   node_t *root_ptr = &root;
   treeRotateRSized(&root_ptr, 2);
}

static void test_treeRotateLRSized_middle_left_size_without_child_child(void) {
   node_t middle = {0};
   node_t left = {
       .r = &middle,
       .s = 1,
   };
   node_t root = {
       .l = &left,
       .s = 2,
   };
   node_t *root_ptr = &root;
   treeRotateLRSized(&root_ptr, 3);
}

static void test_treeRotateRLSized_middle_right_size_without_child_child(void) {
   node_t middle = {0};
   node_t right = {.l = &middle};
   node_t root = {
       .r = &right,
       .s = 0,
   };
   node_t *root_ptr = &root;
   treeRotateRLSized(&root_ptr, 3);
}

static void TestRankBoundaries(void) {
   printf("Rank boundary helpers:\n");

   assert_equal(7, suffix_size_after_prefix(13, 6));
   assert_equal(0, suffix_size_after_prefix(13, 13));
   assert_equal(6, suffix_size_after_rank(13, 6));
   assert_equal(0, suffix_size_after_rank(13, 12));
   assert_equal(UINT64_MAX - 1, suffix_size_after_rank(UINT64_MAX, 0));
   assert_equal(7, sequence_split_inputs_require(6, 13));
   assert_equal(0, sequence_split_inputs_require(13, 13));
   assert_equal(14, sequence_insert_position_count(13));
   assert_equal(UINT64_MAX, sequence_insert_position_count(UINT64_MAX - 1));
   assert_equal(14, sequence_insert_inputs_require(6, 13));
   assert_equal(UINT64_MAX, sequence_insert_inputs_require(0, UINT64_MAX - 1));
   sequence_rank_inputs_require(6, 13);
   sequence_rank_inputs_require(12, 13);
   assert_equal(12, sequence_delete_inputs_require(6, 13));

   assert_equal(13, size_sum(6, 7));
   assert_equal(UINT64_MAX, size_sum(UINT64_MAX, 0));

   node_t child = {0};
   node_t accessor = {.l = &child, .s = 2};
   assert_equal(2, sizeL(&accessor));
   assert_equal(4, sizeR(&accessor, 7));
   invariant(!isLeaf(&accessor));
   invariant(isLeaf(&child));

   assert_equal(1, tree_size_from_child_sizes(0, 0));
   assert_equal(13, tree_size_from_child_sizes(6, 6));
   assert_equal(UINT64_MAX, tree_size_from_child_sizes(UINT64_MAX - 1, 0));
   invariant(tree_root_size_is_valid(NULL, 0));
   invariant(tree_root_size_is_valid(&child, 1));
   invariant(!tree_root_size_is_valid(&child, 0));
   invariant(!tree_root_size_is_valid(NULL, 1));
   tree_root_size_require(NULL, 0);
   tree_root_size_require(&child, 1);
   tree_sized_root_require(NULL, 0);
   tree_sized_root_require(&child, 1);
   tree_nonempty_root_require(&child, 1);
   assert_equal(0, tree_split_inputs_require(NULL, 0, 0));
   assert_equal(1, tree_split_inputs_require(&child, 0, 1));
   assert_equal(0, tree_split_inputs_require(&child, 1, 1));
   assert_equal(1, tree_insert_inputs_require(NULL, 0, 0));
   assert_equal(2, tree_insert_inputs_require(&child, 0, 1));
   assert_equal(2, tree_insert_inputs_require(&child, 1, 1));
   tree_rank_inputs_require(&child, 0, 1);
   assert_equal(0, tree_delete_inputs_require(&child, 0, 1));
   assert_equal(0, tree_join_inputs_require(NULL, NULL, 0, 0));
   assert_equal(1, tree_join_inputs_require(&child, NULL, 1, 0));
   node_t pivot = {0};
   assert_equal(1, tree_build_inputs_require(NULL, &pivot, NULL, 0, 0));
   assert_equal(2, tree_build_inputs_require(&child, &pivot, NULL, 1, 0));
   invariant(tree_handle_is_valid((tree_t){0}));
   invariant(tree_handle_is_valid((tree_t){.root = &child, .size = 1}));
   invariant(!tree_handle_is_valid((tree_t){.root = &child, .size = 0}));
   invariant(!tree_handle_is_valid((tree_t){.root = NULL, .size = 1}));
   node_t invalid_rank_root = {.s = 1};
   invariant(tree_handle_is_valid((tree_t){
       .root = &invalid_rank_root,
       .size = 1,
   }));
   invariant(!tree_sized_handle_is_valid((tree_t){
       .root = &invalid_rank_root,
       .size = 1,
   }));
   node_t missing_left_root = {.s = 1};
   invariant(!tree_sized_handle_is_valid((tree_t){
       .root = &missing_left_root,
       .size = 2,
   }));
   node_t missing_right_root = {0};
   invariant(!tree_sized_handle_is_valid((tree_t){
       .root = &missing_right_root,
       .size = 2,
   }));
   tree_handle_require((tree_t){0});
   tree_handle_require((tree_t){.root = &child, .size = 1});
   tree_handle_require((tree_t){.root = &invalid_rank_root, .size = 1});
   tree_sized_handle_require((tree_t){0});
   tree_sized_handle_require((tree_t){.root = &child, .size = 1});

   assert_equal(13, size_after_inserting_one(12));
   assert_equal(UINT64_MAX, size_after_inserting_one(UINT64_MAX - 1));
   assert_equal(12, size_after_removing_one(13));
   assert_equal(12, rank_of_last(13));

   assert_equal(12, rank_from_end(13, 0));
   assert_equal(0, rank_from_end(13, 12));

   assert_equal(0, rank_in_right_subtree(1, 0));
   assert_equal(7, rank_in_right_subtree(13, 5));
   assert_equal(UINT64_MAX - 1, rank_in_right_subtree(UINT64_MAX, 0));

   node_t subtree = {.s = 4};
   assert_equal(10, rank_after_predecessor(5, &subtree));
   assert_equal(9, rank_after_prefix(5, &subtree));
   node_t zero_subtree = {0};
   assert_equal(UINT64_MAX - 1,
                rank_after_predecessor(UINT64_MAX - 2, &zero_subtree));
   assert_equal(UINT64_MAX - 1,
                rank_after_prefix(UINT64_MAX - 1, &zero_subtree));

   node_t path = {.s = 2};
   invariant(insertL(&path) == &path.l);
   assert_equal(3, path.s);
   u64 insert_rank = 6;
   invariant(insertR(&path, &insert_rank) == &path.r);
   assert_equal(2, insert_rank);
   invariant(deleteL(&path) == &path.l);
   assert_equal(2, path.s);
   u64 delete_rank = 5;
   invariant(deleteR(&path, &delete_rank) == &path.r);
   assert_equal(2, delete_rank);

   node_t path_left_child = {0};
   node_t path_left_root = {.l = &path_left_child, .s = 1};
   node_t *path_left_root_ptr = &path_left_root;
   node_t **path_left_cursor = &path_left_root_ptr;
   tree_pathLeft(&path_left_cursor);
   invariant(path_left_cursor == &path_left_root.l);
   assert_equal(2, path_left_root.s);

   node_t path_right_left = {0};
   node_t path_right_child = {0};
   node_t path_right_root = {
       .l = &path_right_left,
       .r = &path_right_child,
       .s = 1,
   };
   node_t *path_right_root_ptr = &path_right_root;
   node_t **path_right_cursor = &path_right_root_ptr;
   u64 path_right_rank = 4;
   tree_pathRight(&path_right_cursor, &path_right_rank);
   invariant(path_right_cursor == &path_right_root.r);
   assert_equal(2, path_right_rank);
   assert_equal(1, path_right_root.s);

   node_t path_left_sized_child = {0};
   node_t path_left_sized_right = {0};
   node_t path_left_sized_root = {
       .l = &path_left_sized_child,
       .r = &path_left_sized_right,
       .s = 1,
   };
   node_t *path_left_sized_root_ptr = &path_left_sized_root;
   node_t **path_left_sized_cursor = &path_left_sized_root_ptr;
   u64 path_left_sized_size = 3;
   tree_pathLeftSized(&path_left_sized_cursor, &path_left_sized_size);
   invariant(path_left_sized_cursor == &path_left_sized_root.l);
   assert_equal(1, path_left_sized_size);
   assert_equal(2, path_left_sized_root.s);

   node_t path_right_sized_left = {0};
   node_t path_right_sized_tail = {0};
   node_t path_right_sized_child = {.r = &path_right_sized_tail};
   node_t path_right_sized_root = {
       .l = &path_right_sized_left,
       .r = &path_right_sized_child,
       .s = 1,
   };
   node_t *path_right_sized_root_ptr = &path_right_sized_root;
   node_t **path_right_sized_cursor = &path_right_sized_root_ptr;
   u64 path_right_sized_size = 4;
   u64 path_right_sized_rank = 3;
   tree_pathRightSized(&path_right_sized_cursor, &path_right_sized_size,
                       &path_right_sized_rank);
   invariant(path_right_sized_cursor == &path_right_sized_root.r);
   assert_equal(2, path_right_sized_size);
   assert_equal(1, path_right_sized_rank);
   assert_equal(1, path_right_sized_root.s);

   node_t *attached_root = NULL;
   tree_attach(&attached_root, 100);
   invariant(attached_root != NULL);
   assert_equal(100, attached_root->x);
   assert_equal(0, attached_root->s);
   free_nodes(attached_root);

   node_t attach_left = {0};
   tree_attachL(&attach_left, 101);
   assert_equal(1, attach_left.s);
   invariant(attach_left.l != NULL);
   assert_equal(101, attach_left.l->x);
   free_nodes(attach_left.l);

   node_t attach_right = {0};
   tree_attachR(&attach_right, 102);
   assert_equal(0, attach_right.s);
   invariant(attach_right.r != NULL);
   assert_equal(102, attach_right.r->x);
   free_nodes(attach_right.r);

   node_t attach_ll_left = {0};
   node_t attach_ll = {.l = &attach_ll_left, .s = 1};
   tree_attachLL(&attach_ll, 103);
   assert_equal(2, attach_ll.s);
   assert_equal(1, attach_ll_left.s);
   invariant(attach_ll_left.l != NULL);
   assert_equal(103, attach_ll_left.l->x);
   free_nodes(attach_ll_left.l);

   node_t attach_rr_right = {0};
   node_t attach_rr = {.r = &attach_rr_right};
   tree_attachRR(&attach_rr, 104);
   assert_equal(0, attach_rr.s);
   assert_equal(0, attach_rr_right.s);
   invariant(attach_rr_right.r != NULL);
   assert_equal(104, attach_rr_right.r->x);
   free_nodes(attach_rr_right.r);

   node_t attach_lr_left = {0};
   node_t attach_lr = {.l = &attach_lr_left, .s = 1};
   tree_attachLR(&attach_lr, 105);
   assert_equal(2, attach_lr.s);
   assert_equal(0, attach_lr_left.s);
   invariant(attach_lr_left.r != NULL);
   assert_equal(105, attach_lr_left.r->x);
   free_nodes(attach_lr_left.r);

   node_t attach_rl_right = {0};
   node_t attach_rl = {.r = &attach_rl_right};
   tree_attachRL(&attach_rl, 106);
   assert_equal(0, attach_rl.s);
   assert_equal(1, attach_rl_right.s);
   invariant(attach_rl_right.l != NULL);
   assert_equal(106, attach_rl_right.l->x);
   free_nodes(attach_rl_right.l);

   node_t p = {.s = 2};
   node_t r = {.s = 3};
   p.r = &r;
   node_t *rotated_left = rotateL(&p);
   invariant(rotated_left == &r);
   invariant(r.l == &p);
   assert_equal(6, r.s);
   assert_equal(2, p.s);

   node_t q = {.s = 5};
   node_t l = {.s = 2};
   q.l = &l;
   node_t *rotated_right = rotateR(&q);
   invariant(rotated_right == &l);
   invariant(l.r == &q);
   assert_equal(2, l.s);
   assert_equal(2, q.s);

   node_t lr_root = {.s = 2};
   node_t lr_left = {.s = 0};
   node_t lr_middle = {.s = 0};
   lr_root.l = &lr_left;
   lr_left.r = &lr_middle;
   node_t *rotated_lr = rotateLR(&lr_root);
   invariant(rotated_lr == &lr_middle);
   invariant(lr_middle.l == &lr_left);
   invariant(lr_middle.r == &lr_root);
   assert_equal(1, lr_middle.s);
   assert_equal(0, lr_left.s);
   assert_equal(0, lr_root.s);

   node_t rl_root = {.s = 0};
   node_t rl_right = {.s = 1};
   node_t rl_middle = {.s = 0};
   rl_root.r = &rl_right;
   rl_right.l = &rl_middle;
   node_t *rotated_rl = rotateRL(&rl_root);
   invariant(rotated_rl == &rl_middle);
   invariant(rl_middle.l == &rl_root);
   invariant(rl_middle.r == &rl_right);
   assert_equal(1, rl_middle.s);
   assert_equal(0, rl_root.s);
   assert_equal(0, rl_right.s);

   node_t tree_rotate_l_right = {0};
   node_t tree_rotate_l_root = {.r = &tree_rotate_l_right};
   node_t *tree_rotate_l = &tree_rotate_l_root;
   treeRotateL(&tree_rotate_l);
   invariant(tree_rotate_l == &tree_rotate_l_right);
   invariant(tree_rotate_l_right.l == &tree_rotate_l_root);
   assert_equal(1, tree_rotate_l_right.s);
   assert_equal(0, tree_rotate_l_root.s);
   assert_equal(2, tree_verify_reachable_size(tree_rotate_l));

   node_t tree_rotate_r_left = {0};
   node_t tree_rotate_r_root = {.l = &tree_rotate_r_left, .s = 1};
   node_t *tree_rotate_r = &tree_rotate_r_root;
   treeRotateR(&tree_rotate_r);
   invariant(tree_rotate_r == &tree_rotate_r_left);
   invariant(tree_rotate_r_left.r == &tree_rotate_r_root);
   assert_equal(0, tree_rotate_r_left.s);
   assert_equal(0, tree_rotate_r_root.s);
   assert_equal(2, tree_verify_reachable_size(tree_rotate_r));

   node_t tree_rotate_lr_left = {0};
   node_t tree_rotate_lr_middle = {0};
   node_t tree_rotate_lr_root = {.l = &tree_rotate_lr_left, .s = 2};
   tree_rotate_lr_left.r = &tree_rotate_lr_middle;
   node_t *tree_rotate_lr = &tree_rotate_lr_root;
   treeRotateLR(&tree_rotate_lr);
   invariant(tree_rotate_lr == &tree_rotate_lr_middle);
   invariant(tree_rotate_lr_middle.l == &tree_rotate_lr_left);
   invariant(tree_rotate_lr_middle.r == &tree_rotate_lr_root);
   assert_equal(1, tree_rotate_lr_middle.s);
   assert_equal(0, tree_rotate_lr_left.s);
   assert_equal(0, tree_rotate_lr_root.s);
   assert_equal(3, tree_verify_reachable_size(tree_rotate_lr));

   node_t tree_rotate_rl_right = {.s = 1};
   node_t tree_rotate_rl_middle = {0};
   node_t tree_rotate_rl_root = {.r = &tree_rotate_rl_right};
   tree_rotate_rl_right.l = &tree_rotate_rl_middle;
   node_t *tree_rotate_rl = &tree_rotate_rl_root;
   treeRotateRL(&tree_rotate_rl);
   invariant(tree_rotate_rl == &tree_rotate_rl_middle);
   invariant(tree_rotate_rl_middle.l == &tree_rotate_rl_root);
   invariant(tree_rotate_rl_middle.r == &tree_rotate_rl_right);
   assert_equal(1, tree_rotate_rl_middle.s);
   assert_equal(0, tree_rotate_rl_root.s);
   assert_equal(0, tree_rotate_rl_right.s);
   assert_equal(3, tree_verify_reachable_size(tree_rotate_rl));

   node_t sized_rotate_l_right = {0};
   node_t sized_rotate_l_root = {.r = &sized_rotate_l_right};
   node_t *sized_rotate_l = &sized_rotate_l_root;
   treeRotateLSized(&sized_rotate_l, 2);
   invariant(sized_rotate_l == &sized_rotate_l_right);
   invariant(sized_rotate_l_right.l == &sized_rotate_l_root);
   assert_equal(2, tree_verify_size(sized_rotate_l, 2));

   node_t sized_rotate_r_left = {0};
   node_t sized_rotate_r_root = {.l = &sized_rotate_r_left, .s = 1};
   node_t *sized_rotate_r = &sized_rotate_r_root;
   treeRotateRSized(&sized_rotate_r, 2);
   invariant(sized_rotate_r == &sized_rotate_r_left);
   invariant(sized_rotate_r_left.r == &sized_rotate_r_root);
   assert_equal(2, tree_verify_size(sized_rotate_r, 2));

   node_t sized_rotate_lr_left = {0};
   node_t sized_rotate_lr_middle = {0};
   node_t sized_rotate_lr_root = {.l = &sized_rotate_lr_left, .s = 2};
   sized_rotate_lr_left.r = &sized_rotate_lr_middle;
   node_t *sized_rotate_lr = &sized_rotate_lr_root;
   treeRotateLRSized(&sized_rotate_lr, 3);
   invariant(sized_rotate_lr == &sized_rotate_lr_middle);
   invariant(sized_rotate_lr_middle.l == &sized_rotate_lr_left);
   invariant(sized_rotate_lr_middle.r == &sized_rotate_lr_root);
   assert_equal(3, tree_verify_size(sized_rotate_lr, 3));

   node_t sized_rotate_rl_right = {.s = 1};
   node_t sized_rotate_rl_middle = {0};
   node_t sized_rotate_rl_root = {.r = &sized_rotate_rl_right};
   sized_rotate_rl_right.l = &sized_rotate_rl_middle;
   node_t *sized_rotate_rl = &sized_rotate_rl_root;
   treeRotateRLSized(&sized_rotate_rl, 3);
   invariant(sized_rotate_rl == &sized_rotate_rl_middle);
   invariant(sized_rotate_rl_middle.l == &sized_rotate_rl_root);
   invariant(sized_rotate_rl_middle.r == &sized_rotate_rl_right);
   assert_equal(3, tree_verify_size(sized_rotate_rl, 3));

   assert_aborts(test_suffix_size_after_prefix_underflow_child);
   assert_aborts(test_suffix_size_after_rank_out_of_bounds_child);
   assert_aborts(test_sequence_split_inputs_after_end_child);
   assert_aborts(test_sequence_insert_inputs_after_end_child);
   assert_aborts(test_sequence_insert_position_count_full_child);
   assert_aborts(test_sequence_insert_inputs_full_child);
   assert_aborts(test_sequence_rank_inputs_empty_child);
   assert_aborts(test_sequence_rank_inputs_after_end_child);
   assert_aborts(test_sequence_delete_inputs_empty_child);
   assert_aborts(test_sequence_delete_inputs_after_end_child);
   assert_aborts(test_size_sum_overflow_child);
   assert_aborts(test_size_after_inserting_one_overflow_child);
   assert_aborts(test_size_after_removing_one_empty_child);
   assert_aborts(test_sizeL_null_child);
   assert_aborts(test_sizeR_null_child);
   assert_aborts(test_sizeR_out_of_bounds_child);
   assert_aborts(test_tree_size_from_child_sizes_overflow_child);
   assert_aborts(test_tree_size_from_child_sizes_exact_overflow_child);
   assert_aborts(test_tree_root_size_require_root_size_mismatch_child);
   assert_aborts(test_tree_sized_root_require_root_size_mismatch_child);
   assert_aborts(test_tree_sized_root_require_left_size_out_of_bounds_child);
   assert_aborts(test_tree_sized_handle_require_left_size_without_child_child);
   assert_aborts(test_tree_sized_handle_require_right_size_without_child_child);
   assert_aborts(test_tree_nonempty_root_require_empty_child);
   assert_aborts(test_tree_nonempty_root_require_left_size_out_of_bounds_child);
   assert_aborts(test_tree_split_inputs_after_end_child);
   assert_aborts(test_tree_split_inputs_root_size_mismatch_child);
   assert_aborts(test_tree_split_inputs_left_size_out_of_bounds_child);
   assert_aborts(test_tree_split_inputs_left_size_without_child_child);
   assert_aborts(test_tree_split_inputs_right_size_without_child_child);
   assert_aborts(test_tree_insert_inputs_after_end_child);
   assert_aborts(test_tree_insert_inputs_root_size_mismatch_child);
   assert_aborts(test_tree_insert_inputs_left_size_out_of_bounds_child);
   assert_aborts(test_tree_insert_inputs_left_size_without_child_child);
   assert_aborts(test_tree_insert_inputs_right_size_without_child_child);
   assert_aborts(test_tree_insert_inputs_full_child);
   assert_aborts(test_tree_rank_inputs_empty_child);
   assert_aborts(test_tree_rank_inputs_after_end_child);
   assert_aborts(test_tree_rank_inputs_left_size_out_of_bounds_child);
   assert_aborts(test_tree_rank_inputs_left_size_without_child_child);
   assert_aborts(test_tree_rank_inputs_right_size_without_child_child);
   assert_aborts(test_tree_delete_inputs_empty_child);
   assert_aborts(test_tree_delete_inputs_after_end_child);
   assert_aborts(test_tree_delete_inputs_left_size_out_of_bounds_child);
   assert_aborts(test_tree_delete_inputs_left_size_without_child_child);
   assert_aborts(test_tree_delete_inputs_right_size_without_child_child);
   assert_aborts(test_tree_join_inputs_root_size_mismatch_child);
   assert_aborts(test_tree_join_inputs_left_size_out_of_bounds_child);
   assert_aborts(test_tree_join_inputs_right_size_out_of_bounds_child);
   assert_aborts(test_tree_join_inputs_left_size_without_child_child);
   assert_aborts(test_tree_join_inputs_right_size_without_child_child);
   assert_aborts(test_tree_join_inputs_total_size_overflow_child);
   assert_aborts(test_tree_build_inputs_null_pivot_child);
   assert_aborts(test_tree_build_inputs_root_size_mismatch_child);
   assert_aborts(test_tree_build_inputs_left_size_out_of_bounds_child);
   assert_aborts(test_tree_build_inputs_right_size_out_of_bounds_child);
   assert_aborts(test_tree_build_inputs_left_size_without_child_child);
   assert_aborts(test_tree_build_inputs_right_size_without_child_child);
   assert_aborts(test_tree_build_inputs_total_size_overflow_child);
   assert_aborts(test_tree_handle_require_root_size_mismatch_child);
   assert_aborts(test_tree_sized_handle_require_left_size_out_of_bounds_child);
   assert_aborts(test_rank_of_last_empty_child);
   assert_aborts(test_rank_from_end_out_of_bounds_child);
   assert_aborts(test_rank_in_right_subtree_underflow_child);
   assert_aborts(test_rank_after_predecessor_overflow_child);
   assert_aborts(test_rank_after_predecessor_max_child);
   assert_aborts(test_rank_after_predecessor_null_child);
   assert_aborts(test_rank_after_predecessor_exact_overflow_child);
   assert_aborts(test_rank_after_prefix_overflow_child);
   assert_aborts(test_rank_after_prefix_null_child);
   assert_aborts(test_rank_after_prefix_exact_overflow_child);
   assert_aborts(test_isLeaf_null_child);
   assert_aborts(test_insertL_null_child);
   assert_aborts(test_insertL_overflow_child);
   assert_aborts(test_insertR_null_node_child);
   assert_aborts(test_insertR_null_rank_child);
   assert_aborts(test_insertR_not_right_child);
   assert_aborts(test_deleteL_null_child);
   assert_aborts(test_deleteL_empty_child);
   assert_aborts(test_deleteR_null_node_child);
   assert_aborts(test_deleteR_null_rank_child);
   assert_aborts(test_deleteR_not_right_child);
   assert_aborts(test_tree_pathLeft_null_pointer_child);
   assert_aborts(test_tree_pathLeft_null_link_child);
   assert_aborts(test_tree_pathLeft_null_node_child);
   assert_aborts(test_tree_pathLeft_missing_child_child);
   assert_aborts(test_tree_pathLeft_zero_size_left_child);
   assert_aborts(test_tree_pathLeft_overflow_child);
   assert_aborts(test_tree_pathRight_null_pointer_child);
   assert_aborts(test_tree_pathRight_null_link_child);
   assert_aborts(test_tree_pathRight_null_node_child);
   assert_aborts(test_tree_pathRight_null_rank_child);
   assert_aborts(test_tree_pathRight_not_right_child);
   assert_aborts(test_tree_pathRight_missing_child_child);
   assert_aborts(test_tree_pathRight_left_size_without_child_child);
   assert_aborts(test_tree_pathLeftSized_right_size_without_child_child);
   assert_aborts(test_tree_pathRightSized_left_size_without_child_child);
   assert_aborts(test_tree_attach_null_pointer_child);
   assert_aborts(test_tree_attach_occupied_child);
   assert_aborts(test_tree_attachL_null_child);
   assert_aborts(test_tree_attachL_existing_child_child);
   assert_aborts(test_tree_attachL_overflow_child);
   assert_aborts(test_tree_attachR_null_child);
   assert_aborts(test_tree_attachR_existing_child_child);
   assert_aborts(test_tree_attachLL_missing_left_child);
   assert_aborts(test_tree_attachLL_existing_child_child);
   assert_aborts(test_tree_attachLL_root_overflow_child);
   assert_aborts(test_tree_attachLL_child_overflow_child);
   assert_aborts(test_tree_attachRR_missing_right_child);
   assert_aborts(test_tree_attachRR_existing_child_child);
   assert_aborts(test_tree_attachLR_missing_left_child);
   assert_aborts(test_tree_attachLR_existing_child_child);
   assert_aborts(test_tree_attachLR_root_overflow_child);
   assert_aborts(test_tree_attachRL_missing_right_child);
   assert_aborts(test_tree_attachRL_existing_child_child);
   assert_aborts(test_tree_attachRL_child_overflow_child);
   assert_aborts(test_treeRotateL_null_pointer_child);
   assert_aborts(test_treeRotateL_null_node_child);
   assert_aborts(test_treeRotateL_missing_right_child);
   assert_aborts(test_treeRotateR_null_pointer_child);
   assert_aborts(test_treeRotateR_null_node_child);
   assert_aborts(test_treeRotateR_missing_left_child);
   assert_aborts(test_treeRotateLR_missing_left_child);
   assert_aborts(test_treeRotateLR_missing_middle_child);
   assert_aborts(test_treeRotateRL_missing_right_child);
   assert_aborts(test_treeRotateRL_missing_middle_child);
   assert_aborts(test_treeRotateLSized_left_size_without_child_child);
   assert_aborts(test_treeRotateRSized_zero_left_size_with_child_child);
   assert_aborts(test_treeRotateLRSized_middle_left_size_without_child_child);
   assert_aborts(test_treeRotateRLSized_middle_right_size_without_child_child);

   printf("  rank translations and rotations OK\n");
}

#ifdef PERSISTENT
static void test_tree_randomize_shared_child_child(void) {
   node_t child = {.refs = 1};
   node_t root = {
       .l = &child,
       .s = 1,
   };
   tree_t tree = {
       .root = &root,
       .size = 2,
   };
   distribution_t *dist = minimum_new(0);
   (void)tree_randomize(tree, dist);
}
#endif

static void test_tree_randomize_root_size_mismatch_child(void) {
   distribution_t *dist = minimum_new(0);
   tree_t tree = {.size = 1};
   (void)tree_randomize(tree, dist);
}

static void TestTreeRandomize(void) {
   printf("Tree randomize:\n");

   distribution_t *dist = minimum_new(0);
   tree_t empty = assert_tree_randomize_preserves_order((tree_t){0}, dist);
   assert_equal(0, empty.size);
   invariant(empty.root == NULL);
   dist_destroy(dist);

#ifdef MEASUREMENTS
   tree_t singleton = right_chain_of_size(1);
   dist = minimum_new(0);
   measurement_reset_all();
   singleton = assert_tree_randomize_preserves_order(singleton, dist);
   measurement_snapshot_t measurements = measurement_snapshot();
   assert_equal(0, measurements.partition_count);
   assert_equal(0, measurements.partition_depth);
   dist_destroy(dist);
   free_nodes(singleton.root);

   tree_t pair = right_chain_of_size(2);
   dist = minimum_new(0);
   measurement_reset_all();
   pair = assert_tree_randomize_preserves_order(pair, dist);
   measurements = measurement_snapshot();
   assert_equal(1, measurements.partition_count);
   assert_equal(0, measurements.partition_depth);
   dist_destroy(dist);
   free_nodes(pair.root);
#endif

   const u64 chain_size = 32768;
   tree_t chain = right_chain_of_size(chain_size);
   dist = minimum_new(0);
   tree_t randomized = assert_tree_randomize_preserves_order(chain, dist);
   dist_destroy(dist);

   assert_equal(chain_size, randomized.size);
   assert_equal(randomized.size,
                tree_verify_size(randomized.root, randomized.size));
   assert_equal(chain_size, node_size(randomized.root));
   invariant(randomized.root != NULL);
   assert_equal(0, randomized.root->s);
   assert_equal(chain_size - 1, nodePathMetrics(randomized.root).maximum_depth);
   inorder_sequence_ctx_t order = {0};
   inorder(randomized.root, assert_inorder_sequence, &order);
   assert_equal(chain_size, order.count);

   free_nodes(randomized.root);

   chain = right_chain_of_size(257);
   dist = uniform_new(0x243f6a8885a308d3ULL);
   randomized = assert_tree_randomize_preserves_order(chain, dist);
   dist_destroy(dist);

   assert_equal(257, randomized.size);
   assert_equal(randomized.size,
                tree_verify_size(randomized.root, randomized.size));
   assert_equal(257, node_size(randomized.root));
   invariant(randomized.root != NULL);
   invariant(randomized.root->s > 0);
   invariant(randomized.root->s + 1 < randomized.size);
   order = (inorder_sequence_ctx_t){0};
   inorder(randomized.root, assert_inorder_sequence, &order);
   assert_equal(randomized.size, order.count);
   free_nodes(randomized.root);

   arena_t arena;
   arena_init(&arena, 8);
   arena_t *previous_arena = arena_install(&arena);
   tree_t arena_chain = right_chain_of_size(257);
   u64 allocated_before_randomize = test_arena_allocated_slots(&arena);
   dist = uniform_new(0x243f6a8885a308d3ULL);
   arena_chain = assert_tree_randomize_preserves_order(arena_chain, dist);
   assert_equal(allocated_before_randomize, test_arena_allocated_slots(&arena));
   dist_destroy(dist);
   free_nodes(arena_chain.root);
   arena_restore(previous_arena);
   arena_destroy(&arena);

#ifdef PERSISTENT
   fflush(NULL);
   {
      pid_t pid = fork();
      invariant(pid >= 0);
      if (pid == 0) {
         FILE *err = freopen("/dev/null", "w", stderr);
         invariant(err != NULL);
         tree_t shared = right_chain_of_size(17);
         tree_t clone = tree_clone(&shared);
         distribution_t *shared_dist = uniform_new(1);
         (void)clone;
         (void)tree_randomize(shared, shared_dist);
         _exit(0);
      }
      assert_child_aborted(pid);
   }
   assert_aborts(test_tree_randomize_shared_child_child);
#endif
   assert_aborts(test_tree_randomize_root_size_mismatch_child);

   chain = right_chain_of_size(chain_size);
   dist = maximum_new(0);
   randomized = assert_tree_randomize_preserves_order(chain, dist);
   dist_destroy(dist);

   assert_equal(chain_size, randomized.size);
   assert_equal(randomized.size,
                tree_verify_size(randomized.root, randomized.size));
   assert_equal(chain_size, node_size(randomized.root));
   invariant(randomized.root != NULL);
   assert_equal(chain_size - 1, randomized.root->s);
   assert_equal(chain_size - 1, nodePathMetrics(randomized.root).maximum_depth);
   order = (inorder_sequence_ctx_t){0};
   inorder(randomized.root, assert_inorder_sequence, &order);
   assert_equal(chain_size, order.count);

   free_nodes(randomized.root);
   printf("  shape randomization and degenerate extrema pass OK\n");
}

static u64 test_node_y_rank(node_t *p) {
   return p == NULL ? 0 : p->y;
}

static void test_max_rank_heap_null_rank_child(void) {
   verifyMaxRankHeap(NULL, NULL);
}

static void test_max_rank_heap_sized_null_rank_child(void) {
   verifyMaxRankHeapSized(NULL, 0, NULL);
}

static void test_max_rank_heap_sized_null_nonzero_child(void) {
   verifyMaxRankHeapSized(NULL, 1, test_node_y_rank);
}

static void test_max_rank_heap_sized_root_zero_child(void) {
   node_t root = {0};
   verifyMaxRankHeapSized(&root, 0, test_node_y_rank);
}

static node_t test_rank_duplicate_child_tree(node_t *child) {
   invariant(child != NULL);
   *child = (node_t){.y = 0};
   return (node_t){
       .l = child,
       .r = child,
       .s = 1,
       .y = 1,
   };
}

static void test_max_rank_heap_duplicate_child_child(void) {
   node_t child;
   node_t root = test_rank_duplicate_child_tree(&child);
   verifyMaxRankHeap(&root, test_node_y_rank);
}

static void test_max_rank_heap_sized_duplicate_child_child(void) {
   node_t child;
   node_t root = test_rank_duplicate_child_tree(&child);
   verifyMaxRankHeapSized(&root, 3, test_node_y_rank);
}

static void test_max_rank_heap_duplicate_after_seen_grow_child(void) {
   node_t nodes[TEST_NODE_SEEN_GROW_DUPLICATE_COUNT];
   node_t *root = test_node_seen_grow_duplicate_tree(
       nodes, TEST_NODE_SEEN_GROW_DUPLICATE_COUNT);
   verifyMaxRankHeap(root, test_node_y_rank);
}

static void test_redblack_rank_duplicate_child_child(void) {
   node_t child;
   node_t root = test_rank_duplicate_child_tree(&child);
   verifyRedBlackRanks(&root);
}

static void test_redblack_rank_sized_duplicate_child_child(void) {
   node_t child;
   node_t root = test_rank_duplicate_child_tree(&child);
   verifyRedBlackRanksSized(&root, 3);
}

static void test_redblack_rank_sized_null_nonzero_child(void) {
   verifyRedBlackRanksSized(NULL, 1);
}

static void test_redblack_rank_sized_root_zero_child(void) {
   node_t root = {0};
   verifyRedBlackRanksSized(&root, 0);
}

static void test_redblack_height_duplicate_child_child(void) {
   node_t child;
   node_t root = test_rank_duplicate_child_tree(&child);
   (void)verifyRedBlackHeight(&root, 3);
}

static void test_redblack_height_wrong_left_size_child(void) {
   node_t child = {.y = 0};
   node_t root = {
       .l = &child,
       .s = 2,
       .y = 1,
   };
   (void)verifyRedBlackHeight(&root, 3);
}

static void test_redblack_height_duplicate_after_seen_grow_child(void) {
   node_t nodes[TEST_NODE_SEEN_GROW_DUPLICATE_COUNT];
   u64 reported_size = 0;
   node_t *root = test_tree_verify_sized_seen_grow_duplicate_tree(
       nodes, TEST_NODE_SEEN_GROW_DUPLICATE_COUNT, &reported_size);
   (void)verifyRedBlackHeight(root, reported_size);
}

static void test_avl_rank_duplicate_child_child(void) {
   node_t child;
   node_t root = test_rank_duplicate_child_tree(&child);
   (void)verifyAVLRanks(&root, 3);
}

static void test_avl_weak_rank_duplicate_child_child(void) {
   node_t child;
   node_t root = test_rank_duplicate_child_tree(&child);
   verifyAVLWeakRanks(&root);
}

static void test_avl_weak_rank_sized_duplicate_child_child(void) {
   node_t child;
   node_t root = test_rank_duplicate_child_tree(&child);
   verifyAVLWeakRanksSized(&root, 3);
}

static void test_avl_weak_rank_sized_null_nonzero_child(void) {
   verifyAVLWeakRanksSized(NULL, 1);
}

static void test_avl_weak_rank_sized_root_zero_child(void) {
   node_t root = {0};
   verifyAVLWeakRanksSized(&root, 0);
}

static void test_avl_weak_height_duplicate_child_child(void) {
   node_t child;
   node_t root = test_rank_duplicate_child_tree(&child);
   verifyAVLWeakHeight(&root, 3);
}

static void test_avl_relaxed_rank_duplicate_child_child(void) {
   node_t child;
   node_t root = test_rank_duplicate_child_tree(&child);
   verifyAVLRelaxedRanks(&root);
}

static void test_avl_relaxed_rank_duplicate_after_seen_grow_child(void) {
   node_t nodes[TEST_NODE_SEEN_GROW_DUPLICATE_COUNT];
   node_t *root = test_node_seen_grow_duplicate_tree(
       nodes, TEST_NODE_SEEN_GROW_DUPLICATE_COUNT);
   verifyAVLRelaxedRanks(root);
}

static void test_avl_relaxed_rank_wrong_left_size_child(void) {
   node_t child = {.y = 0};
   node_t root = {
       .l = &child,
       .s = 0,
       .y = 1,
   };
   verifyAVLRelaxedRanks(&root);
}

static void test_avl_relaxed_rank_sized_duplicate_child_child(void) {
   node_t child;
   node_t root = test_rank_duplicate_child_tree(&child);
   verifyAVLRelaxedRanksSized(&root, 3);
}

static void test_avl_relaxed_rank_sized_duplicate_after_seen_grow_child(void) {
   node_t nodes[TEST_NODE_SEEN_GROW_DUPLICATE_COUNT];
   u64 reported_size = 0;
   node_t *root = test_tree_verify_sized_seen_grow_duplicate_tree(
       nodes, TEST_NODE_SEEN_GROW_DUPLICATE_COUNT, &reported_size);
   verifyAVLRelaxedRanksSized(root, reported_size);
}

static void test_redblack_relaxed_rank_duplicate_child_child(void) {
   node_t child;
   node_t root = test_rank_duplicate_child_tree(&child);
   verifyRedBlackRelaxedRanks(&root);
}

static void test_redblack_relaxed_rank_sized_duplicate_child_child(void) {
   node_t child;
   node_t root = test_rank_duplicate_child_tree(&child);
   verifyRedBlackRelaxedRanksSized(&root, 3);
}

static void test_redblack_relaxed_rank_sized_null_nonzero_child(void) {
   verifyRedBlackRelaxedRanksSized(NULL, 1);
}

static void test_redblack_relaxed_rank_sized_root_zero_child(void) {
   node_t root = {0};
   verifyRedBlackRelaxedRanksSized(&root, 0);
}

static void test_redblack_relaxed_height_duplicate_child_child(void) {
   node_t child;
   node_t root = test_rank_duplicate_child_tree(&child);
   verifyRedBlackRelaxedHeight(&root);
}

static void test_redblack_relaxed_height_wrong_left_size_child(void) {
   node_t child = {.y = 0};
   node_t root = {
       .l = &child,
       .s = 0,
       .y = 1,
   };
   verifyRedBlackRelaxedHeight(&root);
}

static void test_redblack_relaxed_height_sized_duplicate_child_child(void) {
   node_t child;
   node_t root = test_rank_duplicate_child_tree(&child);
   verifyRedBlackRelaxedHeightSized(&root, 3);
}

static void test_redblack_relaxed_height_duplicate_after_seen_grow_child(void) {
   node_t nodes[TEST_NODE_SEEN_GROW_DUPLICATE_COUNT];
   node_t *root = test_node_seen_grow_duplicate_tree(
       nodes, TEST_NODE_SEEN_GROW_DUPLICATE_COUNT);
   verifyRedBlackRelaxedHeight(root);
}

static void test_redblack_relaxed_duplicate_child_child(void) {
   node_t child;
   node_t root = test_rank_duplicate_child_tree(&child);
   verifyRedBlackRelaxed(&root);
}

static void test_redblack_relaxed_duplicate_after_seen_grow_child(void) {
   node_t nodes[TEST_NODE_SEEN_GROW_DUPLICATE_COUNT];
   node_t *root = test_node_seen_grow_duplicate_tree(
       nodes, TEST_NODE_SEEN_GROW_DUPLICATE_COUNT);
   verifyRedBlackRelaxed(root);
}

static void test_redblack_relaxed_wrong_left_size_child(void) {
   node_t child = {.y = 0};
   node_t root = {
       .l = &child,
       .s = 0,
       .y = 1,
   };
   verifyRedBlackRelaxed(&root);
}

static void test_redblack_relaxed_sized_duplicate_child_child(void) {
   node_t child;
   node_t root = test_rank_duplicate_child_tree(&child);
   verifyRedBlackRelaxedSized(&root, 3);
}

static void test_redblack_relaxed_sized_duplicate_after_seen_grow_child(void) {
   node_t nodes[TEST_NODE_SEEN_GROW_DUPLICATE_COUNT];
   u64 reported_size = 0;
   node_t *root = test_tree_verify_sized_seen_grow_duplicate_tree(
       nodes, TEST_NODE_SEEN_GROW_DUPLICATE_COUNT, &reported_size);
   verifyRedBlackRelaxedSized(root, reported_size);
}

static void test_rank_height_null_nonzero_child(void) {
   (void)verifyRedBlackHeight(NULL, 1);
}

static void test_avl_rank_null_nonzero_child(void) {
   (void)verifyAVLRanks(NULL, 1);
}

static void test_avl_weak_height_null_nonzero_child(void) {
   verifyAVLWeakHeight(NULL, 1);
}

static void test_avl_relaxed_rank_sized_null_nonzero_child(void) {
   verifyAVLRelaxedRanksSized(NULL, 1);
}

static void test_redblack_relaxed_height_sized_null_nonzero_child(void) {
   verifyRedBlackRelaxedHeightSized(NULL, 1);
}

static void test_redblack_relaxed_sized_null_nonzero_child(void) {
   verifyRedBlackRelaxedSized(NULL, 1);
}

static void test_rank_height_root_zero_child(void) {
   node_t root = {0};
   (void)verifyRedBlackHeight(&root, 0);
}

static void test_rank_of_too_large_child(void) {
   node_t node = {.y = (u64)INT_MAX + 1ULL};
   (void)rank_of(&node);
}

static void test_rank_ordinal_too_negative_child(void) {
   (void)rank_ordinal(-2);
}

static void test_rank_above_int_max_child(void) {
   (void)rank_above(INT_MAX);
}

static void test_rank_difference_null_parent_child(void) {
   node_t child = {0};
   (void)rankDifference(NULL, &child);
}

static void test_rank_difference_child_above_parent_child(void) {
   node_t parent = {.y = 0};
   node_t child = {.y = 1};
   (void)rankDifference(&parent, &child);
}

static void test_rank_difference_null_child_too_far_child(void) {
   node_t parent = {.y = (u64)INT_MAX};
   (void)rankDifference(&parent, NULL);
}

static void test_rank_distance_left_too_negative_child(void) {
   (void)rank_distance(-2, 0);
}

static void test_rank_distance_right_too_negative_child(void) {
   (void)rank_distance(0, -2);
}

static void test_rank_at_most_one_above_rank_too_negative_child(void) {
   (void)rank_at_most_one_above(-2, 0);
}

static void test_rank_at_most_one_above_base_too_negative_child(void) {
   (void)rank_at_most_one_above(0, -2);
}

static void test_rank_twice_negative_child(void) {
   (void)rank_twice(-1);
}

static void test_rank_twice_overflow_child(void) {
   (void)rank_twice(INT_MAX);
}

static void test_rank_twice_plus_one_negative_child(void) {
   (void)rank_twice_plus_one(-1);
}

static void test_rank_twice_plus_one_overflow_child(void) {
   (void)rank_twice_plus_one(INT_MAX);
}

static void test_has_zero_child_null_child(void) {
   (void)hasZeroChild(NULL);
}

static void test_promote_null_child(void) {
   promote(NULL);
}

static void test_promote_overflow_child(void) {
   node_t node = {.y = (u64)INT_MAX};
   promote(&node);
}

static void test_demote_null_child(void) {
   demote(NULL);
}

static void test_demote_zero_child(void) {
   node_t node = {0};
   demote(&node);
}

static void test_rb_join_deleteMin_nested_left_size_without_child_child(void) {
   node_t left = {.y = 0};
   node_t right_left = {
       .s = 1,
       .y = 0,
   };
   node_t right = {
       .l = &right_left,
       .s = 2,
       .y = 1,
   };
   (void)rb_join(&left, &right, 1, 3);
}

static void test_rb_join_deleteMax_nested_right_size_without_child_child(void) {
   node_t right = {.y = 0};
   node_t left_right = {.y = 0};
   node_t left = {
       .r = &left_right,
       .s = 0,
       .y = 1,
   };
   (void)rb_join(&left, &right, 3, 1);
}

static void
test_llrb_join_deleteMin_nested_left_size_without_child_child(void) {
   node_t left_root = {.y = 0};
   node_t right_left = {
       .s = 1,
       .y = 0,
   };
   node_t right_root = {
       .l = &right_left,
       .s = 2,
       .y = 1,
   };
   llrb_t left_impl = {.tree = {.root = &left_root, .size = 1}};
   llrb_t right_impl = {.tree = {.root = &right_root, .size = 3}};
   list_t left = {.impl = &left_impl, .vt = &llrb_vtable};
   list_t right = {.impl = &right_impl, .vt = &llrb_vtable};
   (void)list_join(&left, &right);
}

static void
test_llrb_join_deleteMax_nested_right_size_without_child_child(void) {
   node_t right_root = {.y = 0};
   node_t left_right = {.y = 0};
   node_t left_root = {
       .r = &left_right,
       .s = 0,
       .y = 1,
   };
   llrb_t left_impl = {.tree = {.root = &left_root, .size = 3}};
   llrb_t right_impl = {.tree = {.root = &right_root, .size = 1}};
   list_t left = {.impl = &left_impl, .vt = &llrb_vtable};
   list_t right = {.impl = &right_impl, .vt = &llrb_vtable};
   (void)list_join(&left, &right);
}

static void test_aa_join_deleteMin_nested_left_size_without_child_child(void) {
   node_t left_root = {.y = 0};
   node_t right_left = {
       .s = 1,
       .y = 0,
   };
   node_t right_root = {
       .l = &right_left,
       .s = 2,
       .y = 1,
   };
   aa_t left_impl = {.tree = {.root = &left_root, .size = 1}};
   aa_t right_impl = {.tree = {.root = &right_root, .size = 3}};
   list_t left = {.impl = &left_impl, .vt = &aa_vtable};
   list_t right = {.impl = &right_impl, .vt = &aa_vtable};
   (void)aa_vtable.join_fn(&left, &right);
}

static void test_aa_join_deleteMax_nested_right_size_without_child_child(void) {
   node_t right_root = {.y = 0};
   node_t left_right = {.y = 0};
   node_t left_root = {
       .r = &left_right,
       .s = 0,
       .y = 1,
   };
   aa_t left_impl = {.tree = {.root = &left_root, .size = 3}};
   aa_t right_impl = {.tree = {.root = &right_root, .size = 1}};
   list_t left = {.impl = &left_impl, .vt = &aa_vtable};
   list_t right = {.impl = &right_impl, .vt = &aa_vtable};
   (void)aa_vtable.join_fn(&left, &right);
}

static void
test_avlw_join_deleteMin_nested_left_size_without_child_child(void) {
   node_t left = {.y = 0};
   node_t right_left = {
       .s = 1,
       .y = 0,
   };
   node_t right = {
       .l = &right_left,
       .s = 2,
       .y = 1,
   };
   (void)avlw_join(&left, &right, 1, 3);
}

static void
test_avlw_join_deleteMax_nested_right_size_without_child_child(void) {
   node_t right = {.y = 0};
   node_t left_right = {.y = 0};
   node_t left = {
       .r = &left_right,
       .s = 0,
       .y = 1,
   };
   (void)avlw_join(&left, &right, 3, 1);
}

static void test_avl_td_insert_nested_right_size_without_child_child(void) {
   node_t right = {.y = 0};
   node_t root = {
       .r = &right,
       .s = 0,
       .y = 1,
   };
   avl_topdown_t impl = {.tree = {.root = &root, .size = 3}};
   list_t list = {.impl = &impl, .vt = &avl_topdown_vtable};
   avl_topdown_vtable.insert_fn(&list, 1, 42);
}

static void
test_avlw_td_join_deleteMin_nested_left_size_without_child_child(void) {
   node_t left_root = {.y = 0};
   node_t right_left = {
       .s = 1,
       .y = 0,
   };
   node_t right_root = {
       .l = &right_left,
       .s = 2,
       .y = 1,
   };
   avl_weak_topdown_t left_impl = {.tree = {.root = &left_root, .size = 1}};
   avl_weak_topdown_t right_impl = {.tree = {.root = &right_root, .size = 3}};
   list_t left = {.impl = &left_impl, .vt = &avl_weak_topdown_vtable};
   list_t right = {.impl = &right_impl, .vt = &avl_weak_topdown_vtable};
   (void)avl_weak_topdown_vtable.join_fn(&left, &right);
}

static void
test_avlw_td_join_deleteMax_nested_right_size_without_child_child(void) {
   node_t right_root = {.y = 0};
   node_t left_right = {.y = 0};
   node_t left_root = {
       .r = &left_right,
       .s = 0,
       .y = 1,
   };
   avl_weak_topdown_t left_impl = {.tree = {.root = &left_root, .size = 3}};
   avl_weak_topdown_t right_impl = {.tree = {.root = &right_root, .size = 1}};
   list_t left = {.impl = &left_impl, .vt = &avl_weak_topdown_vtable};
   list_t right = {.impl = &right_impl, .vt = &avl_weak_topdown_vtable};
   (void)avl_weak_topdown_vtable.join_fn(&left, &right);
}

static void test_rb_td_insert_nested_right_size_without_child_child(void) {
   node_t right = {.y = 0};
   node_t root = {
       .r = &right,
       .s = 0,
       .y = 1,
   };
   redblack_topdown_t impl = {.tree = {.root = &root, .size = 3}};
   list_t list = {.impl = &impl, .vt = &redblack_topdown_vtable};
   redblack_topdown_vtable.insert_fn(&list, 1, 42);
}

static void test_avlw_td_insert_nested_right_size_without_child_child(void) {
   node_t right = {.y = 0};
   node_t root = {
       .r = &right,
       .s = 0,
       .y = 1,
   };
   avl_weak_topdown_t impl = {.tree = {.root = &root, .size = 3}};
   list_t list = {.impl = &impl, .vt = &avl_weak_topdown_vtable};
   avl_weak_topdown_vtable.insert_fn(&list, 1, 42);
}

static void test_treap_bu_join_nested_left_extra_child_child(void) {
   node_t extra = {.y = 0};
   node_t nested = {
       .l = &extra,
       .s = 0,
       .y = 1,
   };
   node_t left_root = {
       .r = &nested,
       .s = 0,
       .y = 10,
   };
   node_t right_root = {.y = 5};
   treap_bottomup_t left_impl = {.tree = {.root = &left_root, .size = 3}};
   treap_bottomup_t right_impl = {.tree = {.root = &right_root, .size = 1}};
   list_t left = {.impl = &left_impl, .vt = &treap_bottomup_vtable};
   list_t right = {.impl = &right_impl, .vt = &treap_bottomup_vtable};
   (void)treap_bottomup_vtable.join_fn(&left, &right);
}

static void test_treap_td_join_nested_left_extra_child_child(void) {
   node_t extra = {.y = 0};
   node_t nested = {
       .l = &extra,
       .s = 0,
       .y = 1,
   };
   node_t left_root = {
       .r = &nested,
       .s = 0,
       .y = 10,
   };
   node_t right_root = {.y = 5};
   treap_topdown_t left_impl = {.tree = {.root = &left_root, .size = 3}};
   treap_topdown_t right_impl = {.tree = {.root = &right_root, .size = 1}};
   list_t left = {.impl = &left_impl, .vt = &treap_topdown_vtable};
   list_t right = {.impl = &right_impl, .vt = &treap_topdown_vtable};
   (void)treap_topdown_vtable.join_fn(&left, &right);
}

static void test_zip_join_nested_left_extra_child_child(void) {
   node_t extra = {.y = 0};
   node_t nested = {
       .l = &extra,
       .s = 0,
       .y = 1,
   };
   node_t left_root = {
       .r = &nested,
       .s = 0,
       .y = 10,
   };
   node_t right_root = {.y = 5};
   zip_t left_impl = {.tree = {.root = &left_root, .size = 3}};
   zip_t right_impl = {.tree = {.root = &right_root, .size = 1}};
   list_t left = {.impl = &left_impl, .vt = &zip_vtable};
   list_t right = {.impl = &right_impl, .vt = &zip_vtable};
   (void)zip_vtable.join_fn(&left, &right);
}

static void test_splay_join_nested_left_extra_child_child(void) {
   node_t extra = {0};
   node_t nested = {
       .l = &extra,
       .s = 0,
   };
   node_t left_root = {
       .r = &nested,
       .s = 0,
   };
   node_t right_root = {0};
   splay_t left_impl = {.tree = {.root = &left_root, .size = 3}};
   splay_t right_impl = {.tree = {.root = &right_root, .size = 1}};
   list_t left = {.impl = &left_impl, .vt = &splay_vtable};
   list_t right = {.impl = &right_impl, .vt = &splay_vtable};
   (void)splay_vtable.join_fn(&left, &right);
}

static void test_splay_select_nested_right_shape_child(void) {
   node_t extra = {0};
   node_t right = {
       .l = &extra,
       .s = 0,
   };
   node_t root = {
       .r = &right,
       .s = 0,
   };
   splay_t impl = {.tree = {.root = &root, .size = 3}};
   list_t list = {.impl = &impl, .vt = &splay_vtable};
   (void)splay_vtable.select_fn(&list, 1);
}

#ifdef DEBUG
static void test_rb_join_wrong_left_size_child(void) {
   node_t left = {0};
   (void)rb_join(&left, NULL, 2, 0);
}

static void test_rb_join_wrong_right_size_child(void) {
   node_t right = {0};
   (void)rb_join(NULL, &right, 0, 2);
}

static void test_rb_join_overlap_child(void) {
   tree_t tree = right_chain_of_size(3);
   (void)rb_join(tree.root, tree.root, tree.size, tree.size);
}

static void test_rb_split_wrong_size_child(void) {
   node_t child = {0};
   node_t root = {
       .r = &child,
       .s = 0,
   };
   (void)rb_split(&root, 3, 1);
}

static void test_rb_relaxed_join_wrong_left_size_child(void) {
   node_t left = {0};
   (void)rb_relaxed_join(&left, NULL, 2, 0);
}

static void test_rb_relaxed_join_wrong_right_size_child(void) {
   node_t right = {0};
   (void)rb_relaxed_join(NULL, &right, 0, 2);
}

static void test_rb_relaxed_join_overlap_child(void) {
   tree_t tree = right_chain_of_size(3);
   (void)rb_relaxed_join(tree.root, tree.root, tree.size, tree.size);
}

static void test_avl_join_wrong_left_size_child(void) {
   node_t left = {0};
   (void)avl_join(&left, NULL, 2, 0);
}

static void test_avl_join_wrong_right_size_child(void) {
   node_t right = {0};
   (void)avl_join(NULL, &right, 0, 2);
}

static void test_avl_deleteMin_nested_left_size_without_child_child(void) {
   node_t left = {.s = 1};
   node_t root = {
       .l = &left,
       .s = 2,
   };
   (void)avl_deleteMin(&root, 3);
}

static void test_avl_deleteMax_nested_right_size_without_child_child(void) {
   node_t right = {0};
   node_t root = {
       .r = &right,
       .s = 0,
   };
   (void)avl_deleteMax(&root, 3);
}

static void test_avl_join_overlap_child(void) {
   tree_t tree = right_chain_of_size(3);
   (void)avl_join(tree.root, tree.root, tree.size, tree.size);
}

static void test_avlr_join_wrong_left_size_child(void) {
   node_t left = {0};
   (void)avlr_join(&left, NULL, 2, 0);
}

static void test_avlr_join_wrong_right_size_child(void) {
   node_t right = {0};
   (void)avlr_join(NULL, &right, 0, 2);
}

static void test_avlr_join_overlap_child(void) {
   tree_t tree = right_chain_of_size(3);
   (void)avlr_join(tree.root, tree.root, tree.size, tree.size);
}

static void test_avlr_split_wrong_size_child(void) {
   node_t child = {0};
   node_t root = {
       .r = &child,
       .s = 0,
   };
   (void)avlr_split(&root, 1, 3);
}

static void test_avlw_split_wrong_size_child(void) {
   node_t child = {0};
   node_t root = {
       .r = &child,
       .s = 0,
   };
   (void)avlw_split(&root, 1, 3);
}

static void test_avlw_join_wrong_left_size_child(void) {
   node_t left = {0};
   (void)avlw_join(&left, NULL, 2, 0);
}

static void test_avlw_join_wrong_right_size_child(void) {
   node_t right = {0};
   (void)avlw_join(NULL, &right, 0, 2);
}

static void test_avlw_join_overlap_child(void) {
   tree_t tree = right_chain_of_size(3);
   (void)avlw_join(tree.root, tree.root, tree.size, tree.size);
}
#endif

static bool redblack_height_fits_size(u64 size, int height) {
   invariant(size > 0);
   invariant(height >= 0);
   invariant(height < 128);
   __uint128_t square = (__uint128_t)size * (__uint128_t)size;
   return square >= ((__uint128_t)1 << height);
}

static int expected_redblack_height_bound(u64 size) {
   invariant(size > 0);
   int height = 0;
   while (height < 127 && redblack_height_fits_size(size, height + 1)) {
      height++;
   }
   return height;
}

static void assert_redblack_height_bound(u64 size) {
   assert_int_equal(expected_redblack_height_bound(size),
                    util_redblack_height_bound(size));
}

static void assert_redblack_height_boundaries(void) {
   for (u64 size = 1; size <= 4096; size++) {
      assert_redblack_height_bound(size);
   }

   u64 values[1 + 64 * 3 + 1];
   u64 count = 0;
   values[count++] = 1;
   for (u64 bit = 0; bit < 64; bit++) {
      u64 power = 1ULL << bit;
      if (power > 1) {
         values[count++] = power - 1;
      }
      values[count++] = power;
      if (power < UINT64_MAX) {
         values[count++] = power + 1;
      }
   }
   values[count++] = UINT64_MAX;
   invariant(count <= sizeof(values) / sizeof(values[0]));

   for (u64 i = 0; i < count; i++) {
      assert_redblack_height_bound(values[i]);
   }
}

static void assert_avl_height_boundaries(void) {
   u64 min_size_prev = 0;
   u64 min_size = 1;
   int height = 0;

   for (;;) {
      assert_int_equal(height, util_avl_height_bound(min_size));
      if (min_size > 1) {
         assert_int_equal(height - 1, util_avl_height_bound(min_size - 1));
      }

      __uint128_t next = (__uint128_t)min_size + min_size_prev + 1;
      if (next > UINT64_MAX) {
         break;
      }

      min_size_prev = min_size;
      min_size = (u64)next;
      height++;
   }

   assert_int_equal(90, height);
   assert_equal(12200160415121876737ULL, min_size);
   assert_int_equal(90, util_avl_height_bound(UINT64_MAX));
}

static void TestRankVerifiers(void) {
   printf("Rank verifiers:\n");

   verifyMaxRankHeap(NULL, test_node_y_rank);
   verifyMaxRankHeapSized(NULL, 0, test_node_y_rank);
   verifyRedBlackRanks(NULL);
   verifyRedBlackRanksSized(NULL, 0);
   assert_int_equal(-1, verifyRedBlackHeight(NULL, 0));
   assert_int_equal(-1, verifyAVLRanks(NULL, 0));
   verifyAVLWeakRanks(NULL);
   verifyAVLWeakRanksSized(NULL, 0);
   verifyAVLWeakHeight(NULL, 0);
   verifyAVLRelaxedRanks(NULL);
   verifyAVLRelaxedRanksSized(NULL, 0);
   verifyRedBlackRelaxedRanks(NULL);
   verifyRedBlackRelaxedRanksSized(NULL, 0);
   verifyRedBlackRelaxedHeight(NULL);
   verifyRedBlackRelaxedHeightSized(NULL, 0);
   verifyRedBlackRelaxed(NULL);
   verifyRedBlackRelaxedSized(NULL, 0);

   assert_aborts(test_max_rank_heap_null_rank_child);
   assert_aborts(test_max_rank_heap_sized_null_rank_child);
   assert_aborts(test_max_rank_heap_sized_null_nonzero_child);
   assert_aborts(test_max_rank_heap_sized_root_zero_child);
   assert_aborts(test_max_rank_heap_duplicate_child_child);
   assert_aborts(test_max_rank_heap_sized_duplicate_child_child);
   assert_aborts(test_max_rank_heap_duplicate_after_seen_grow_child);
   assert_aborts(test_redblack_rank_duplicate_child_child);
   assert_aborts(test_redblack_rank_sized_duplicate_child_child);
   assert_aborts(test_redblack_rank_sized_null_nonzero_child);
   assert_aborts(test_redblack_rank_sized_root_zero_child);
   assert_aborts(test_redblack_height_duplicate_child_child);
   assert_aborts(test_redblack_height_wrong_left_size_child);
   assert_aborts(test_redblack_height_duplicate_after_seen_grow_child);
   assert_aborts(test_avl_rank_duplicate_child_child);
   assert_aborts(test_avl_weak_rank_duplicate_child_child);
   assert_aborts(test_avl_weak_rank_sized_duplicate_child_child);
   assert_aborts(test_avl_weak_rank_sized_null_nonzero_child);
   assert_aborts(test_avl_weak_rank_sized_root_zero_child);
   assert_aborts(test_avl_weak_height_duplicate_child_child);
   assert_aborts(test_avl_relaxed_rank_duplicate_child_child);
   assert_aborts(test_avl_relaxed_rank_duplicate_after_seen_grow_child);
   assert_aborts(test_avl_relaxed_rank_wrong_left_size_child);
   assert_aborts(test_avl_relaxed_rank_sized_duplicate_child_child);
   assert_aborts(test_avl_relaxed_rank_sized_duplicate_after_seen_grow_child);
   assert_aborts(test_redblack_relaxed_rank_duplicate_child_child);
   assert_aborts(test_redblack_relaxed_rank_sized_duplicate_child_child);
   assert_aborts(test_redblack_relaxed_rank_sized_null_nonzero_child);
   assert_aborts(test_redblack_relaxed_rank_sized_root_zero_child);
   assert_aborts(test_redblack_relaxed_height_duplicate_child_child);
   assert_aborts(test_redblack_relaxed_height_wrong_left_size_child);
   assert_aborts(test_redblack_relaxed_height_sized_duplicate_child_child);
   assert_aborts(test_redblack_relaxed_height_duplicate_after_seen_grow_child);
   assert_aborts(test_redblack_relaxed_duplicate_child_child);
   assert_aborts(test_redblack_relaxed_duplicate_after_seen_grow_child);
   assert_aborts(test_redblack_relaxed_wrong_left_size_child);
   assert_aborts(test_redblack_relaxed_sized_duplicate_child_child);
   assert_aborts(test_redblack_relaxed_sized_duplicate_after_seen_grow_child);
   assert_aborts(test_rank_height_null_nonzero_child);
   assert_aborts(test_avl_rank_null_nonzero_child);
   assert_aborts(test_avl_weak_height_null_nonzero_child);
   assert_aborts(test_avl_relaxed_rank_sized_null_nonzero_child);
   assert_aborts(test_redblack_relaxed_height_sized_null_nonzero_child);
   assert_aborts(test_redblack_relaxed_sized_null_nonzero_child);
   assert_aborts(test_rank_height_root_zero_child);
   assert_aborts(test_rb_join_deleteMin_nested_left_size_without_child_child);
   assert_aborts(test_rb_join_deleteMax_nested_right_size_without_child_child);
   assert_aborts(test_llrb_join_deleteMin_nested_left_size_without_child_child);
   assert_aborts(
       test_llrb_join_deleteMax_nested_right_size_without_child_child);
   assert_aborts(test_aa_join_deleteMin_nested_left_size_without_child_child);
   assert_aborts(test_aa_join_deleteMax_nested_right_size_without_child_child);
   assert_aborts(test_avlw_join_deleteMin_nested_left_size_without_child_child);
   assert_aborts(
       test_avlw_join_deleteMax_nested_right_size_without_child_child);
   assert_aborts(test_avl_td_insert_nested_right_size_without_child_child);
   assert_aborts(
       test_avlw_td_join_deleteMin_nested_left_size_without_child_child);
   assert_aborts(
       test_avlw_td_join_deleteMax_nested_right_size_without_child_child);
   assert_aborts(test_rb_td_insert_nested_right_size_without_child_child);
   assert_aborts(test_avlw_td_insert_nested_right_size_without_child_child);
   assert_aborts(test_treap_bu_join_nested_left_extra_child_child);
   assert_aborts(test_treap_td_join_nested_left_extra_child_child);
   assert_aborts(test_zip_join_nested_left_extra_child_child);
   assert_aborts(test_splay_join_nested_left_extra_child_child);
   assert_aborts(test_splay_select_nested_right_shape_child);
#ifdef DEBUG
   assert_aborts(test_rb_join_wrong_left_size_child);
   assert_aborts(test_rb_join_wrong_right_size_child);
   assert_aborts(test_rb_join_overlap_child);
   assert_aborts(test_rb_split_wrong_size_child);
   assert_aborts(test_rb_relaxed_join_wrong_left_size_child);
   assert_aborts(test_rb_relaxed_join_wrong_right_size_child);
   assert_aborts(test_rb_relaxed_join_overlap_child);
   assert_aborts(test_avl_join_wrong_left_size_child);
   assert_aborts(test_avl_join_wrong_right_size_child);
   assert_aborts(test_avl_deleteMin_nested_left_size_without_child_child);
   assert_aborts(test_avl_deleteMax_nested_right_size_without_child_child);
   assert_aborts(test_avl_join_overlap_child);
   assert_aborts(test_avlr_join_wrong_left_size_child);
   assert_aborts(test_avlr_join_wrong_right_size_child);
   assert_aborts(test_avlr_join_overlap_child);
   assert_aborts(test_avlr_split_wrong_size_child);
   assert_aborts(test_avlw_split_wrong_size_child);
   assert_aborts(test_avlw_join_wrong_left_size_child);
   assert_aborts(test_avlw_join_wrong_right_size_child);
   assert_aborts(test_avlw_join_overlap_child);
#endif
   printf("  malformed shape guard OK\n");

   assert_equal(0, rank_ordinal(-1));
   assert_equal(1, rank_ordinal(0));
   assert_equal((u64)INT_MAX + 1ULL, rank_ordinal(INT_MAX));
   assert_equal(0, rank_above(-1));
   assert_equal(1, rank_above(0));
   assert_equal((u64)INT_MAX, rank_above(INT_MAX - 1));
   assert_equal((u64)INT_MAX + 1ULL, rank_distance(INT_MAX, -1));
   invariant(rank_at_most_one_above(0, -1));
   invariant(!rank_at_most_one_above(1, -1));
   invariant(rank_at_most_one_above(INT_MAX, INT_MAX - 1));
   invariant(!rank_at_most_one_above(INT_MAX, INT_MAX - 2));
   assert_int_equal(0, rank_twice(0));
   assert_int_equal(INT_MAX - 1, rank_twice(INT_MAX / 2));
   assert_int_equal(1, rank_twice_plus_one(0));
   assert_int_equal(INT_MAX, rank_twice_plus_one((INT_MAX - 1) / 2));

   node_t parent = {.y = (u64)INT_MAX - 1ULL};
   assert_int_equal(INT_MAX, rankDifference(&parent, NULL));
   node_t child = {.y = (u64)INT_MAX - 1ULL};
   parent.y = (u64)INT_MAX;
   assert_int_equal(1, rankDifference(&parent, &child));
   parent = (node_t){.y = 0};
   child = (node_t){.y = 0};
   invariant(isZeroChild(&parent, &child));
   invariant(!isOneChild(&parent, &child));
   parent.y = 1;
   invariant(isOneChild(&parent, &child));
   invariant(isTwoChild(&parent, NULL));
   parent.y = 2;
   invariant(isThreeChild(&parent, NULL));
   node_t zero_zero = {.y = 0};
   zero_zero.l = &child;
   zero_zero.r = &child;
   invariant(hasZeroChild(&zero_zero));
   invariant(isZeroZero(&zero_zero));
   node_t one_one = {.y = 1};
   one_one.l = &child;
   one_one.r = &child;
   invariant(isOneOne(&one_one));
   node_t two_two = {.y = 1};
   invariant(isTwoTwo(&two_two));
   promote(&child);
   assert_equal(1, child.y);
   demote(&child);
   assert_equal(0, child.y);
   assert_aborts(test_rank_of_too_large_child);
   assert_aborts(test_rank_ordinal_too_negative_child);
   assert_aborts(test_rank_above_int_max_child);
   assert_aborts(test_rank_difference_null_parent_child);
   assert_aborts(test_rank_difference_child_above_parent_child);
   assert_aborts(test_rank_difference_null_child_too_far_child);
   assert_aborts(test_rank_distance_left_too_negative_child);
   assert_aborts(test_rank_distance_right_too_negative_child);
   assert_aborts(test_rank_at_most_one_above_rank_too_negative_child);
   assert_aborts(test_rank_at_most_one_above_base_too_negative_child);
   assert_aborts(test_rank_twice_negative_child);
   assert_aborts(test_rank_twice_overflow_child);
   assert_aborts(test_rank_twice_plus_one_negative_child);
   assert_aborts(test_rank_twice_plus_one_overflow_child);
   assert_aborts(test_has_zero_child_null_child);
   assert_aborts(test_promote_null_child);
   assert_aborts(test_promote_overflow_child);
   assert_aborts(test_demote_null_child);
   assert_aborts(test_demote_zero_child);

   assert_int_equal(0, util_redblack_height_bound(1));
   assert_int_equal(2, util_redblack_height_bound(2));
   assert_int_equal(3, util_redblack_height_bound(3));
   assert_int_equal(6, util_redblack_height_bound(11));
   assert_int_equal(7, util_redblack_height_bound(12));
   assert_int_equal(127, util_redblack_height_bound(UINT64_MAX));
   assert_redblack_height_boundaries();

   assert_int_equal(0, util_avl_height_bound(1));
   assert_int_equal(1, util_avl_height_bound(2));
   assert_int_equal(1, util_avl_height_bound(3));
   assert_int_equal(20, util_avl_height_bound(28656));
   assert_int_equal(20, util_avl_height_bound(46366));
   assert_int_equal(21, util_avl_height_bound(46367));
   assert_int_equal(89, util_avl_height_bound(12200160415121876736ULL));
   assert_int_equal(90, util_avl_height_bound(12200160415121876737ULL));
   assert_int_equal(90, util_avl_height_bound(UINT64_MAX));
   assert_avl_height_boundaries();
   printf("  helper boundaries OK\n");

   const u64 chain_size = NODE_STACK_INLINE_CAP * 2 + 9;

   tree_t weak = perfect_ranked_tree(8);
   assert_equal(weak.size, tree_verify_size(weak.root, weak.size));
   assert_int_equal(8, height(weak.root));
   verifyMaxRankHeap(weak.root, test_node_y_rank);
   verifyMaxRankHeapSized(weak.root, weak.size, test_node_y_rank);
   verifyRedBlackRanks(weak.root);
   verifyRedBlackRanksSized(weak.root, weak.size);
   assert_int_equal(8, verifyRedBlackHeight(weak.root, weak.size));
   assert_int_equal(8, verifyAVLRanks(weak.root, weak.size));
   verifyAVLWeakRanks(weak.root);
   verifyAVLWeakRanksSized(weak.root, weak.size);
   verifyAVLWeakHeight(weak.root, weak.size);
   free_nodes(weak.root);

   tree_t minimal_avl = minimal_height_balanced_tree(20);
   assert_equal(28656, minimal_avl.size);
   assert_int_equal(20, verifyAVLRanks(minimal_avl.root, minimal_avl.size));
   free_nodes(minimal_avl.root);

   tree_t avl_relaxed = right_chain_ranked_by_height(chain_size, 1);
   assert_equal(avl_relaxed.size,
                tree_verify_size(avl_relaxed.root, avl_relaxed.size));
   verifyMaxRankHeap(avl_relaxed.root, test_node_y_rank);
   verifyMaxRankHeapSized(avl_relaxed.root, avl_relaxed.size, test_node_y_rank);
   verifyAVLRelaxedRanks(avl_relaxed.root);
   verifyAVLRelaxedRanksSized(avl_relaxed.root, avl_relaxed.size);
   free_nodes(avl_relaxed.root);

   tree_t redblack_relaxed = right_chain_ranked_by_height(chain_size, 2);
   assert_equal(redblack_relaxed.size,
                tree_verify_size(redblack_relaxed.root, redblack_relaxed.size));
   verifyMaxRankHeap(redblack_relaxed.root, test_node_y_rank);
   verifyMaxRankHeapSized(redblack_relaxed.root, redblack_relaxed.size,
                          test_node_y_rank);
   verifyRedBlackRelaxedRanks(redblack_relaxed.root);
   verifyRedBlackRelaxedRanksSized(redblack_relaxed.root,
                                   redblack_relaxed.size);
   verifyRedBlackRelaxedHeight(redblack_relaxed.root);
   verifyRedBlackRelaxedHeightSized(redblack_relaxed.root,
                                    redblack_relaxed.size);
   verifyRedBlackRelaxed(redblack_relaxed.root);
   verifyRedBlackRelaxedSized(redblack_relaxed.root, redblack_relaxed.size);
   free_nodes(redblack_relaxed.root);

   printf("  empty, weak, and deep relaxed ranks OK\n");
}

static bool expected_gte_msb(u64 x, u64 y) {
   return y == 0 || (x > 0 && util_log2(x) >= util_log2(y));
}

static void assert_util_gte_msb(u64 x, u64 y) {
   bool expected = expected_gte_msb(x, y);
   invariant(util_gte_msb(x, y) == expected);
}

static void assert_util_gte_msb_boundaries(void) {
   for (u64 x = 0; x <= 512; x++) {
      for (u64 y = 0; y <= 512; y++) {
         assert_util_gte_msb(x, y);
      }
   }

   u64 values[1 + 64 * 3 + 1];
   u64 count = 0;
   values[count++] = 0;
   for (u64 bit = 0; bit < 64; bit++) {
      u64 power = 1ULL << bit;
      if (power > 0) {
         values[count++] = power - 1;
      }
      values[count++] = power;
      if (power < UINT64_MAX) {
         values[count++] = power + 1;
      }
   }
   values[count++] = UINT64_MAX;
   invariant(count <= sizeof(values) / sizeof(values[0]));

   for (u64 i = 0; i < count; i++) {
      for (u64 j = 0; j < count; j++) {
         assert_util_gte_msb(values[i], values[j]);
      }
   }
}

static bool expected_gte_msb_u128(__uint128_t x, __uint128_t y) {
   return y == 0 || (x > 0 && weight_u128_log2(x) >= weight_u128_log2(y));
}

static void assert_weight_gte_msb_u128(__uint128_t x, __uint128_t y) {
   bool expected = expected_gte_msb_u128(x, y);
   invariant(weight_gte_msb_u128(x, y) == expected);
}

static void assert_weight_gte_msb_u128_boundaries(void) {
   __uint128_t values[1 + 128 * 3 + 1];
   __uint128_t max = ~(__uint128_t)0;
   u64 count = 0;
   values[count++] = 0;
   for (u64 bit = 0; bit < 128; bit++) {
      __uint128_t power = (__uint128_t)1 << bit;
      values[count++] = power - 1;
      values[count++] = power;
      if (power < max) {
         values[count++] = power + 1;
      }
   }
   values[count++] = max;
   invariant(count <= sizeof(values) / sizeof(values[0]));

   for (u64 i = 0; i < count; i++) {
      for (u64 j = 0; j < count; j++) {
         assert_weight_gte_msb_u128(values[i], values[j]);
      }
   }
}

typedef int (*weight_max_height_fn)(u64);

static void assert_weight_max_height_zero_aborts(weight_max_height_fn fn) {
   invariant(fn != NULL);
   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      (void)fn(0);
      _exit(0);
   }
   assert_child_aborted(pid);
}

static void assert_log_max_height_boundaries(weight_max_height_fn fn) {
   invariant(fn != NULL);
   for (u64 bit = 0; bit < 64; bit++) {
      u64 power = 1ULL << bit;
      int expected = (int)(2 * bit);
      if (power > 1) {
         assert_int_equal(expected - 2, fn(power - 1));
      }
      assert_int_equal(expected, fn(power));
      if (power > 1 && power < UINT64_MAX) {
         assert_int_equal(expected, fn(power + 1));
      }
   }
   assert_int_equal(126, fn(UINT64_MAX));
}

static void test_weight_balance_missing_balanced_child(void) {
   weight_balance_require((weight_balance_t){
       .singleRotation = three_two_singleRotation,
       .maxHeight = three_two_maxHeight,
   });
}

static void test_weight_balance_missing_single_rotation_child(void) {
   weight_balance_require((weight_balance_t){
       .isBalanced = three_two_isBalanced,
       .maxHeight = three_two_maxHeight,
   });
}

static void test_weight_balance_missing_max_height_child(void) {
   weight_balance_require((weight_balance_t){
       .isBalanced = three_two_isBalanced,
       .singleRotation = three_two_singleRotation,
   });
}

static void test_weight_verifier_duplicate_child_child(void) {
   node_t child = {0};
   node_t root = {
       .l = &child,
       .r = &child,
       .s = 1,
   };
   wbst_verifyBalance(wb_three_two, &root, 3, 1);
}

static void test_weight_verifier_duplicate_after_seen_grow_child(void) {
   node_t nodes[TEST_NODE_SEEN_GROW_DUPLICATE_COUNT];
   u64 reported_size = 0;
   node_t *root = test_tree_verify_sized_seen_grow_duplicate_tree(
       nodes, TEST_NODE_SEEN_GROW_DUPLICATE_COUNT, &reported_size);
   wbst_verifyBalance(wb_three_two, root, reported_size, 1);
}

static void test_weight_verifier_wrong_left_size_child(void) {
   node_t child = {0};
   node_t root = {
       .l = &child,
       .s = 2,
   };
   wbst_verifyBalance(wb_three_two, &root, 3, 1);
}

static void test_weight_verifier_left_size_out_of_bounds_child(void) {
   node_t root = {.s = 1};
   wbst_verifyBalance(wb_three_two, &root, 1, 1);
}

static void test_weight_relaxed_verifier_duplicate_child_child(void) {
   node_t child = {0};
   node_t root = {
       .l = &child,
       .r = &child,
       .s = 1,
   };
   tree_t tree = {.root = &root, .size = 3};
   wbst_rlx_verify(wb_three_two, &tree);
}

static void test_weight_relaxed_verifier_duplicate_after_seen_grow_child(void) {
   node_t nodes[TEST_NODE_SEEN_GROW_DUPLICATE_COUNT];
   u64 reported_size = 0;
   node_t *root = test_tree_verify_sized_seen_grow_duplicate_tree(
       nodes, TEST_NODE_SEEN_GROW_DUPLICATE_COUNT, &reported_size);
   tree_t tree = {.root = root, .size = reported_size};
   wbst_rlx_verify(wb_three_two, &tree);
}

static void test_weight_relaxed_verifier_root_size_mismatch_child(void) {
   node_t root = {0};
   tree_t tree = {.root = &root, .size = 0};
   wbst_rlx_verify(wb_three_two, &tree);
}

static void test_weight_relaxed_rebalance_root_size_mismatch_child(void) {
   tree_t tree = {.size = 1};
   wbst_rlx_rebalance_if_too_tall(wb_three_two, &tree);
}

static void test_wbst_bu_build_children_exact_max_child(void) {
   node_t left = {0};
   node_t pivot = {0};
   node_t right = {0};
   (void)wbst_bu_build(wb_three_two, &left, &pivot, &right, UINT64_MAX - 1, 1);
}

static void test_wbst_td_build_children_exact_max_child(void) {
   node_t left = {0};
   node_t pivot = {0};
   node_t right = {0};
   (void)wbst_td_build(wb_three_two, &left, &pivot, &right, UINT64_MAX - 1, 1);
}

static void test_wbst_rlx_join_root_size_mismatch_child(void) {
   (void)wbst_rlx_join(wb_three_two, NULL, NULL, 1, 0);
}

static void test_wbst_rlx_join_total_size_overflow_child(void) {
   node_t left = {0};
   node_t right = {0};
   (void)wbst_rlx_join(wb_three_two, &left, &right, UINT64_MAX, 1);
}

static void
test_wbst_bu_join_deleteMin_nested_left_size_without_child_child(void) {
   node_t left = {0};
   node_t right_left = {.s = 1};
   node_t right = {
       .l = &right_left,
       .s = 2,
   };
   (void)wbst_bu_join(wb_three_two, &left, &right, 1, 3);
}

static void
test_wbst_bu_join_deleteMax_nested_right_size_without_child_child(void) {
   node_t right = {0};
   node_t left_right = {0};
   node_t left = {
       .r = &left_right,
       .s = 0,
   };
   (void)wbst_bu_join(wb_three_two, &left, &right, 3, 1);
}

static void
test_wbst_td_join_deleteMin_nested_left_size_without_child_child(void) {
   node_t left = {0};
   node_t right_left = {.s = 1};
   node_t right = {
       .l = &right_left,
       .s = 2,
   };
   (void)wbst_td_join(wb_three_two, &left, &right, 1, 3);
}

static void
test_wbst_td_join_deleteMax_nested_right_size_without_child_child(void) {
   node_t right = {0};
   node_t left_right = {0};
   node_t left = {
       .r = &left_right,
       .s = 0,
   };
   (void)wbst_td_join(wb_three_two, &left, &right, 3, 1);
}

static void test_wbst_rlx_insert_nested_right_size_without_child_child(void) {
   node_t right = {0};
   node_t root = {
       .r = &right,
       .s = 0,
   };
   node_t *root_ptr = &root;
   wbst_rlx_insert(wb_three_two, &root_ptr, 3, 1, 0, 3);
}

#ifdef DEBUG
static void test_wbst_bu_build_wrong_left_size_child(void) {
   node_t left = {0};
   node_t pivot = {0};
   (void)wbst_bu_build(wb_three_two, &left, &pivot, NULL, 2, 0);
}

static void test_wbst_bu_build_wrong_right_size_child(void) {
   node_t pivot = {0};
   node_t right = {0};
   (void)wbst_bu_build(wb_three_two, NULL, &pivot, &right, 0, 2);
}

static void test_wbst_td_build_wrong_left_size_child(void) {
   node_t left = {0};
   node_t pivot = {0};
   (void)wbst_td_build(wb_three_two, &left, &pivot, NULL, 2, 0);
}

static void test_wbst_td_build_wrong_right_size_child(void) {
   node_t pivot = {0};
   node_t right = {0};
   (void)wbst_td_build(wb_three_two, NULL, &pivot, &right, 0, 2);
}

static void test_wbst_bu_join_wrong_left_size_child(void) {
   node_t left = {0};
   (void)wbst_bu_join(wb_three_two, &left, NULL, 2, 0);
}

static void test_wbst_td_join_wrong_left_size_child(void) {
   node_t left = {0};
   (void)wbst_td_join(wb_three_two, &left, NULL, 2, 0);
}

static void test_wbst_rlx_join_wrong_left_size_child(void) {
   node_t left = {0};
   (void)wbst_rlx_join(wb_three_two, &left, NULL, 2, 0);
}

static void test_wbst_rlx_insert_wrong_size_child(void) {
   node_t child = {0};
   node_t root = {
       .r = &child,
       .s = 0,
   };
   node_t *root_ptr = &root;
   wbst_rlx_insert(wb_three_two, &root_ptr, 3, 1, 0, 3);
}

static void test_wbst_rlx_rebalance_wrong_size_child(void) {
   node_t child = {0};
   node_t root = {
       .r = &child,
       .s = 0,
   };
   tree_t tree = {
       .root = &root,
       .size = 3,
   };
   wbst_rlx_rebalance_if_too_tall(wb_three_two, &tree);
}

static void test_wbst_bu_join_overlap_child(void) {
   tree_t tree = right_chain_of_size(3);
   (void)wbst_bu_join(wb_three_two, tree.root, tree.root, tree.size, tree.size);
}

static void test_wbst_td_join_overlap_child(void) {
   tree_t tree = right_chain_of_size(3);
   (void)wbst_td_join(wb_three_two, tree.root, tree.root, tree.size, tree.size);
}

static void test_wbst_bu_split_wrong_size_child(void) {
   node_t child = {0};
   node_t root = {
       .r = &child,
       .s = 0,
   };
   (void)wbst_bu_split(wb_three_two, &root, 1, 3);
}

static void test_wbst_td_split_wrong_size_child(void) {
   node_t child = {0};
   node_t root = {
       .r = &child,
       .s = 0,
   };
   (void)wbst_td_split(wb_three_two, &root, 1, 3);
}

static void test_wbst_rlx_join_overlap_child(void) {
   tree_t tree = right_chain_of_size(3);
   (void)wbst_rlx_join(wb_three_two, tree.root, tree.root, tree.size,
                       tree.size);
}
#endif

static void test_wbst_bu_vtable_join_mismatched_weight_child(void) {
   wbst_bottomup_t left = {.wb = wb_three_two};
   wbst_bottomup_t right = {.wb = wb_log_size};
   list_t left_list = {.impl = &left, .vt = &wbst_bottomup_vtable};
   list_t right_list = {.impl = &right, .vt = &wbst_bottomup_vtable};
   (void)wbst_bottomup_vtable.join_fn(&left_list, &right_list);
}

static void test_wbst_td_vtable_join_mismatched_weight_child(void) {
   wbst_topdown_t left = {.wb = wb_three_two};
   wbst_topdown_t right = {.wb = wb_log_weight};
   list_t left_list = {.impl = &left, .vt = &wbst_topdown_vtable};
   list_t right_list = {.impl = &right, .vt = &wbst_topdown_vtable};
   (void)wbst_topdown_vtable.join_fn(&left_list, &right_list);
}

static void test_wbst_rlx_vtable_join_mismatched_weight_child(void) {
   wbst_relaxed_t left = {.wb = wb_three_two};
   wbst_relaxed_t right = {.wb = wb_log_weight};
   list_t left_list = {.impl = &left, .vt = &wbst_relaxed_vtable};
   list_t right_list = {.impl = &right, .vt = &wbst_relaxed_vtable};
   (void)wbst_relaxed_vtable.join_fn(&left_list, &right_list);
}

static void assert_wbst_relaxed_insert_rebalances_monotone(void) {
   tree_t tree = {0};
   const u64 count = NODE_STACK_INLINE_CAP + 17;
   for (u64 i = 0; i < count; i++) {
      wbst_rlx_insert(wb_three_two, &tree.root, tree.size, tree.size, i,
                      tree.size);
      tree_list_sized_inserted(&tree);
      wbst_rlx_verify(wb_three_two, &tree);
   }

   assert_equal(count, tree.size);
   assert_equal(tree.size, tree_verify_size(tree.root, tree.size));
   invariant(height(tree.root) <= wb_three_two.maxHeight(tree.size));
   assert_tree_inorder_sequence(tree);
   free_nodes(tree.root);
}

static void TestWeightVerifiers(void) {
   printf("Weight verifiers:\n");

   assert_equal(0, util_log2(0));
   assert_equal(0, util_log2(1));
   assert_equal(1, util_log2(2));
   assert_equal(1, util_log2(3));
   assert_equal(62, util_log2((1ULL << 63) - 1));
   assert_equal(63, util_log2(1ULL << 63));
   assert_equal(63, util_log2(UINT64_MAX));

   static const u64 msb_values[] = {
       0, 1, 2, 3, 7, 8, 9, (1ULL << 63) - 1, 1ULL << 63, UINT64_MAX,
   };
   for (u64 i = 0; i < sizeof(msb_values) / sizeof(msb_values[0]); i++) {
      for (u64 j = 0; j < sizeof(msb_values) / sizeof(msb_values[0]); j++) {
         assert_util_gte_msb(msb_values[i], msb_values[j]);
      }
   }
   assert_util_gte_msb_boundaries();
   printf("  utility math boundaries OK\n");

   assert_equal(UINT64_MAX, weight_u64_plus_one(UINT64_MAX - 1));
   invariant(weight_u128_plus_one(UINT64_MAX) == ((__uint128_t)UINT64_MAX + 1));
   assert_int_equal(0, weight_u128_log2(1));
   assert_int_equal(63, weight_u128_log2((__uint128_t)1 << 63));
   assert_int_equal(64, weight_u128_log2((__uint128_t)1 << 64));
   assert_int_equal(127, weight_u128_log2(~(__uint128_t)0));
   invariant(weight_gte_msb_u128((__uint128_t)1 << 64, UINT64_MAX));
   invariant(!weight_gte_msb_u128(UINT64_MAX, (__uint128_t)1 << 64));
   assert_weight_gte_msb_u128_boundaries();
   invariant(three_two_isBalanced(UINT64_MAX, UINT64_MAX));
   invariant(three_two_singleRotation(UINT64_MAX, UINT64_MAX));
   invariant(log_size_isBalanced(UINT64_MAX, UINT64_MAX));
   invariant(log_size_singleRotation(UINT64_MAX, UINT64_MAX));
   invariant(log_weight_isBalanced(UINT64_MAX, UINT64_MAX));
   invariant(log_weight_singleRotation(UINT64_MAX, UINT64_MAX));
   invariant(weight_balance_equal(wb_three_two, wb_three_two));
   invariant(!weight_balance_equal(wb_three_two, wb_log_size));
   invariant(!weight_balance_equal(wb_log_size, wb_log_weight));
   assert_weight_max_height_zero_aborts(three_two_maxHeight);
   assert_weight_max_height_zero_aborts(log_size_maxHeight);
   assert_weight_max_height_zero_aborts(log_weight_maxHeight);
   assert_aborts(test_weight_balance_missing_balanced_child);
   assert_aborts(test_weight_balance_missing_single_rotation_child);
   assert_aborts(test_weight_balance_missing_max_height_child);
   assert_int_equal(2, three_two_maxHeight(1));
   assert_int_equal(3, three_two_maxHeight(2));
   assert_int_equal(14, three_two_maxHeight(64));
   assert_int_equal(153, three_two_maxHeight(17400648639910404100ULL));
   assert_int_equal(154, three_two_maxHeight(17400648639910404101ULL));
   assert_int_equal(154, three_two_maxHeight(UINT64_MAX));
   assert_log_max_height_boundaries(log_size_maxHeight);
   assert_log_max_height_boundaries(log_weight_maxHeight);
   assert_int_equal(126, log_size_maxHeight(UINT64_MAX));
   assert_int_equal(126, log_weight_maxHeight(UINT64_MAX));
   assert_aborts(test_wbst_bu_build_children_exact_max_child);
   assert_aborts(test_wbst_td_build_children_exact_max_child);
   assert_aborts(test_wbst_rlx_join_root_size_mismatch_child);
   assert_aborts(test_wbst_rlx_join_total_size_overflow_child);
   assert_aborts(
       test_wbst_bu_join_deleteMin_nested_left_size_without_child_child);
   assert_aborts(
       test_wbst_bu_join_deleteMax_nested_right_size_without_child_child);
   assert_aborts(
       test_wbst_td_join_deleteMin_nested_left_size_without_child_child);
   assert_aborts(
       test_wbst_td_join_deleteMax_nested_right_size_without_child_child);
   assert_aborts(test_wbst_rlx_insert_nested_right_size_without_child_child);
   printf("  balance predicate boundaries OK\n");

#ifdef DEBUG
   assert_aborts(test_wbst_bu_build_wrong_left_size_child);
   assert_aborts(test_wbst_bu_build_wrong_right_size_child);
   assert_aborts(test_wbst_td_build_wrong_left_size_child);
   assert_aborts(test_wbst_td_build_wrong_right_size_child);
   printf("  WBST build precondition guards OK\n");

   assert_aborts(test_wbst_bu_join_wrong_left_size_child);
   assert_aborts(test_wbst_td_join_wrong_left_size_child);
   assert_aborts(test_wbst_rlx_join_wrong_left_size_child);
   assert_aborts(test_wbst_rlx_insert_wrong_size_child);
   assert_aborts(test_wbst_rlx_rebalance_wrong_size_child);
   assert_aborts(test_wbst_bu_join_overlap_child);
   assert_aborts(test_wbst_td_join_overlap_child);
   assert_aborts(test_wbst_rlx_join_overlap_child);
   printf("  WBST join precondition guards OK\n");

   assert_aborts(test_wbst_bu_split_wrong_size_child);
   assert_aborts(test_wbst_td_split_wrong_size_child);
   printf("  WBST split precondition guards OK\n");
#endif

   assert_aborts(test_wbst_bu_vtable_join_mismatched_weight_child);
   assert_aborts(test_wbst_td_vtable_join_mismatched_weight_child);
   assert_aborts(test_wbst_rlx_vtable_join_mismatched_weight_child);
   printf("  weight policy join guards OK\n");

   wbst_verifyBalance(wb_three_two, NULL, 0, 1);
   wbst_bu_verifyBalance(wb_three_two, NULL, 0);
   wbst_td_verifyBalance(wb_three_two, NULL, 0);

   assert_aborts(test_weight_verifier_duplicate_child_child);
   assert_aborts(test_weight_verifier_duplicate_after_seen_grow_child);
   assert_aborts(test_weight_verifier_wrong_left_size_child);
   assert_aborts(test_weight_verifier_left_size_out_of_bounds_child);
   assert_aborts(test_weight_relaxed_verifier_duplicate_child_child);
   assert_aborts(test_weight_relaxed_verifier_duplicate_after_seen_grow_child);
   assert_aborts(test_weight_relaxed_verifier_root_size_mismatch_child);
   assert_aborts(test_weight_relaxed_rebalance_root_size_mismatch_child);
   printf("  malformed shape guard OK\n");

   tree_t perfect = perfect_ranked_tree(8);
   assert_equal(perfect.size, tree_verify_size(perfect.root, perfect.size));
   assert_int_equal(8, height(perfect.root));

   wbst_verifyBalance(wb_three_two, perfect.root, perfect.size, 1);
   wbst_verifyBalance(wb_log_size, perfect.root, perfect.size, 1);
   wbst_verifyBalance(wb_log_weight, perfect.root, perfect.size, 1);
   wbst_bu_verifyBalance(wb_three_two, perfect.root, perfect.size);
   wbst_td_verifyBalance(wb_three_two, perfect.root, perfect.size);
   wbst_rlx_verify(wb_three_two, &perfect);
   wbst_rlx_verify(wb_log_weight, &perfect);

   free_nodes(perfect.root);

   tree_t relaxed = right_chain_of_size(NODE_STACK_INLINE_CAP + 9);
   wbst_rlx_rebalance_if_too_tall(wb_three_two, &relaxed);
   assert_equal(relaxed.size, tree_verify_size(relaxed.root, relaxed.size));
   invariant(height(relaxed.root) <= wb_three_two.maxHeight(relaxed.size));
   wbst_rlx_verify(wb_three_two, &relaxed);
   free_nodes(relaxed.root);

   relaxed = right_chain_of_size(NODE_STACK_INLINE_CAP + 9);
   wbst_rlx_rebalance_if_too_tall(wb_log_weight, &relaxed);
   assert_equal(relaxed.size, tree_verify_size(relaxed.root, relaxed.size));
   invariant(height(relaxed.root) <= wb_log_weight.maxHeight(relaxed.size));
   wbst_rlx_verify(wb_log_weight, &relaxed);
   free_nodes(relaxed.root);

   assert_wbst_relaxed_insert_rebalances_monotone();

   printf("  empty, perfect, and relaxed-height trees OK\n");
}

static void test_arena_restore_destroyed_child(void) {
   arena_t stale;
   arena_init(&stale, 1);
   arena_t *previous = arena_install(&stale);
   arena_restore(previous);
   arena_destroy(&stale);
   arena_restore(&stale);
}

static void test_arena_restore_unregistered_current_child(void) {
   arena_t unregistered = {0};
   global_arena = &unregistered;
   arena_restore(NULL);
}

static void test_arena_double_init_live_child(void) {
   arena_t live;
   arena_init(&live, 1);
   arena_init(&live, 1);
}

static void test_arena_init_too_large_child(void) {
   invariant(arena_max_capacity() < UINT64_MAX);
   arena_t arena;
   arena_init(&arena, arena_max_capacity() + 1);
}

static void test_arena_double_restore_child(void) {
   arena_t restored;
   arena_init(&restored, 1);
   arena_t *previous = arena_install(&restored);
   arena_restore(previous);
   arena_restore(previous);
}

static void test_arena_restore_out_of_order_child(void) {
   arena_t outer, inner;
   arena_init(&outer, 1);
   arena_init(&inner, 1);
   arena_t *outer_previous = arena_install(&outer);
   arena_t *inner_previous = arena_install(&inner);
   invariant(inner_previous == &outer);
   arena_restore(outer_previous);
}

static void test_arena_double_install_child(void) {
   arena_t *installed = (arena_t *)bst_malloc(sizeof(*installed));
   arena_init(installed, 1);
   (void)arena_install(installed);
   (void)arena_install(installed);
   bst_unreachable();
}

static void test_arena_reinstall_installed_ancestor_child(void) {
   arena_t outer, inner;
   arena_init(&outer, 1);
   arena_init(&inner, 1);
   (void)arena_install(&outer);
   (void)arena_install(&inner);
   (void)arena_install(&outer);
   bst_unreachable();
}

static void test_arena_reset_destroyed_child(void) {
   arena_t stale;
   arena_init(&stale, 1);
   arena_destroy(&stale);
   arena_reset(&stale);
}

static void test_arena_destroy_installed_child(void) {
   arena_t installed;
   arena_init(&installed, 1);
   (void)arena_install(&installed);
   arena_destroy(&installed);
}

static void test_arena_destroy_installed_ancestor_child(void) {
   arena_t outer, inner;
   arena_init(&outer, 1);
   arena_init(&inner, 1);
   (void)arena_install(&outer);
   (void)arena_install(&inner);
   arena_destroy(&outer);
}

static void test_arena_contains_partial_zero_state_child(void) {
   arena_t partial = {
       .first = {.capacity = 1},
   };
   node_t node = {0};
   (void)arena_contains(&partial, &node);
}

static void test_arena_contains_missing_current_child(void) {
   arena_t arena;
   arena_init(&arena, 1);
   arena.current = NULL;
   node_t node = {0};
   (void)arena_contains(&arena, &node);
}

static void test_arena_contains_current_not_terminal_child(void) {
   arena_t arena;
   arena_init(&arena, 1);
   arena_t *previous = arena_install(&arena);
   (void)tree_allocate((node_t){.x = 0});
   (void)tree_allocate((node_t){.x = 1});
   arena_restore(previous);
   arena.current = &arena.first;
   node_t node = {0};
   (void)arena_contains(&arena, &node);
}

#ifdef DEBUG
static void test_tree_allocate_unregistered_global_arena_child(void) {
   node_t buffer[1] = {0};
   arena_t arena = {
       .first =
           {
               .buffer = buffer,
               .capacity = 1,
           },
   };
   arena.current = &arena.first;
   global_arena = &arena;
   (void)tree_allocate((node_t){.x = 0});
}
#endif

static void test_node_stack_pop_empty_child(void) {
   node_t *inline_data[NODE_STACK_INLINE_CAP];
   node_stack_t stack = {
       .data = inline_data,
       .len = 0,
       .cap = NODE_STACK_INLINE_CAP,
   };
   (void)node_stack_pop(&stack);
}

static void test_node_stack_free_zero_cap_child(void) {
   node_t *inline_data[NODE_STACK_INLINE_CAP];
   node_stack_t stack = {
       .data = inline_data,
       .len = 0,
       .cap = 0,
   };
   node_stack_free(&stack, inline_data);
}

static void test_node_stack_free_wrong_inline_child(void) {
   node_t *inline_data[NODE_STACK_INLINE_CAP];
   node_t *other_inline[NODE_STACK_INLINE_CAP];
   node_stack_t stack = {
       .data = inline_data,
       .len = 0,
       .cap = NODE_STACK_INLINE_CAP,
   };
   node_stack_free(&stack, other_inline);
}

#ifdef PERSISTENT
static void test_tree_allocate_shared_value_child(void) {
   (void)tree_allocate((node_t){.refs = 1});
}
#endif

static void TestTreeArenaOwnership(void) {
   printf("Tree arena ownership:\n");

   node_t stack_nodes[NODE_STACK_INLINE_CAP + 3] = {0};
   node_t *inline_data[NODE_STACK_INLINE_CAP];
   node_stack_t stack = {
       .data = inline_data,
       .len = 0,
       .cap = NODE_STACK_INLINE_CAP,
   };
   const u64 stack_node_count = sizeof(stack_nodes) / sizeof(stack_nodes[0]);
   node_stack_push(&stack, inline_data, NULL);
   assert_equal(0, stack.len);
   invariant(stack.data == inline_data);
   for (u64 i = 0; i < stack_node_count; i++) {
      node_stack_push(&stack, inline_data, &stack_nodes[i]);
   }
   invariant(stack.data != inline_data);
   for (u64 i = 0; i < stack_node_count; i++) {
      u64 expected = suffix_size_after_rank(stack_node_count, i);
      invariant(node_stack_pop(&stack) == &stack_nodes[expected]);
   }
   node_stack_free(&stack, inline_data);
   invariant(stack.data == NULL);
   invariant(stack.len == 0);
   invariant(stack.cap == 0);
   assert_aborts(test_node_stack_pop_empty_child);
   assert_aborts(test_node_stack_free_zero_cap_child);
   assert_aborts(test_node_stack_free_wrong_inline_child);
   printf("  inline node stack helper contracts OK\n");

   assert_aborts(test_arena_restore_destroyed_child);
   assert_aborts(test_arena_restore_unregistered_current_child);
   assert_aborts(test_arena_double_init_live_child);
   if (arena_max_capacity() < UINT64_MAX) {
      assert_aborts(test_arena_init_too_large_child);
   }
   assert_aborts(test_arena_double_restore_child);
   assert_aborts(test_arena_restore_out_of_order_child);
   assert_aborts(test_arena_double_install_child);
   assert_aborts(test_arena_reinstall_installed_ancestor_child);
   assert_aborts(test_arena_reset_destroyed_child);
   assert_aborts(test_arena_destroy_installed_child);
   assert_aborts(test_arena_destroy_installed_ancestor_child);
   assert_aborts(test_arena_contains_partial_zero_state_child);
   assert_aborts(test_arena_contains_missing_current_child);
   assert_aborts(test_arena_contains_current_not_terminal_child);
#ifdef DEBUG
   assert_aborts(test_tree_allocate_unregistered_global_arena_child);
#endif
#ifdef PERSISTENT
   assert_aborts(test_tree_allocate_shared_value_child);
#endif

   node_t external_node = {0};
   invariant(!arena_contains_live(NULL));
   invariant(!arena_contains_live(&external_node));

   arena_t zero_capacity_arena;
   arena_init(&zero_capacity_arena, 0);
   invariant(arena_is_live(&zero_capacity_arena));
   invariant(zero_capacity_arena.first.capacity == 1);
   invariant(zero_capacity_arena.first.offset == 0);
   invariant(zero_capacity_arena.current == &zero_capacity_arena.first);
   arena_destroy(&zero_capacity_arena);

   arena_t outer_arena;
   arena_t inner_arena;
   arena_init(&outer_arena, 2);
   arena_init(&inner_arena, 2);
   invariant(!arena_is_installed(&outer_arena));
   invariant(!arena_is_installed(&inner_arena));
   arena_t *outer_previous = arena_install(&outer_arena);
   invariant(arena_is_installed(&outer_arena));
   invariant(!arena_is_installed(&inner_arena));
   arena_t *inner_previous = arena_install(&inner_arena);
   invariant(inner_previous == &outer_arena);
   invariant(global_arena == &inner_arena);
   invariant(arena_is_installed(&outer_arena));
   invariant(arena_is_installed(&inner_arena));
   arena_restore(inner_previous);
   invariant(global_arena == &outer_arena);
   invariant(arena_is_installed(&outer_arena));
   invariant(!arena_is_installed(&inner_arena));
   arena_restore(outer_previous);
   invariant(global_arena == outer_previous);
   invariant(!arena_is_installed(&outer_arena));
   invariant(!arena_is_installed(&inner_arena));
   arena_destroy(&inner_arena);
   arena_destroy(&outer_arena);

   arena_t growth_arena;
   arena_init(&growth_arena, 1);
   arena_t *growth_previous = arena_install(&growth_arena);
   node_t *growth_0 = tree_allocate((node_t){.x = 0});
   node_t *growth_1 = tree_allocate((node_t){.x = 1});
   node_t *growth_2 = tree_allocate((node_t){.x = 2});
   node_t *growth_3 = tree_allocate((node_t){.x = 3});
   invariant(growth_0 == growth_arena.first.buffer);
   invariant(growth_arena.first.capacity == 1);
   invariant(growth_arena.first.next != NULL);
   invariant(growth_arena.first.next->capacity == 2);
   invariant(growth_arena.first.next->next != NULL);
   invariant(growth_arena.first.next->next->capacity == 4);
   invariant(growth_arena.current == growth_arena.first.next->next);
   invariant(arena_contains(&growth_arena, growth_0));
   invariant(arena_contains(&growth_arena, growth_1));
   invariant(arena_contains(&growth_arena, growth_2));
   invariant(arena_contains(&growth_arena, growth_3));
   invariant(arena_contains_live(growth_0));
   invariant(arena_contains_live(growth_3));
   arena_restore(growth_previous);
   arena_destroy(&growth_arena);
   invariant(!arena_contains_live(growth_0));
   invariant(!arena_contains(&growth_arena, growth_0));

   arena_t arena;
   arena_init(&arena, 4);
   invariant(arena_is_live(&arena));
   invariant(arena.first.buffer != NULL);
   invariant(arena.first.capacity == 4);
   invariant(arena.first.offset == 0);
   invariant(!arena_contains(NULL, arena.first.buffer));
   invariant(!arena_contains(&arena, NULL));
   invariant(arena_contains(&arena, arena.first.buffer));
   invariant(arena_contains(&arena, arena.first.buffer + 3));
   invariant(!arena_contains(&arena, arena.first.buffer + 4));
   invariant(sizeof(node_t) > 1);
   node_t *misaligned_arena_slot = (node_t *)((char *)arena.first.buffer + 1);
   invariant(!arena_contains(&arena, misaligned_arena_slot));
   invariant(!arena_contains_live(misaligned_arena_slot));
   invariant(arena_contains_live(arena.first.buffer));
   arena_t *previous_arena = arena_install(&arena);

   const u64 chain_size = NODE_STACK_INLINE_CAP * 2 + 3;
   tree_t chain = right_chain_of_size(chain_size);
   invariant(arena_contains(&arena, chain.root));
   assert_equal(chain_size, node_size(chain.root));
   assert_equal(chain.size, tree_verify_size(chain.root, chain.size));

   free_nodes(chain.root);
   assert_equal(chain_size, node_size(chain.root));
   assert_equal(chain.size, tree_verify_size(chain.root, chain.size));

   tree_t uninstalled_chain = right_chain_of_size(chain_size);
   invariant(arena_contains(&arena, uninstalled_chain.root));
   arena_restore(previous_arena);
   invariant(arena_contains_live(uninstalled_chain.root));
   free_nodes(uninstalled_chain.root);
   assert_equal(chain_size, node_size(uninstalled_chain.root));
   assert_equal(
       uninstalled_chain.size,
       tree_verify_size(uninstalled_chain.root, uninstalled_chain.size));

   tree_t heap_child = right_chain_of_size(1);
   invariant(arena_install(&arena) == previous_arena);

   node_t *mixed = tree_allocate((node_t){
       .l = heap_child.root,
       .s = heap_child.size,
       .x = 42,
   });
   invariant(arena_contains(&arena, mixed));
   invariant(arena_contains_live(mixed));
   invariant(!arena_contains(&arena, heap_child.root));
   invariant(!arena_contains_live(heap_child.root));
   free_nodes(mixed);

   arena_reset(&arena);
   invariant(arena.first.buffer != NULL);
   invariant(arena.first.offset == 0);
   invariant(arena.first.next == NULL);
   invariant(arena.current == &arena.first);

   node_t *after_reset = tree_allocate((node_t){.x = 7});
   invariant(after_reset == arena.first.buffer);
   invariant(arena.first.offset == 1);
   invariant(arena_contains(&arena, after_reset));
   free_nodes(after_reset);

   arena_restore(previous_arena);
   arena_destroy(&arena);
   invariant(!arena_is_live(&arena));
   printf("  stack cleanup and arena-owned free_nodes pass OK\n");
}

static void test_tree_persist_null_pointer(void) {
   tree_persist(NULL);
}

static void test_tree_persist_null_node(void) {
   node_t *node = NULL;
   tree_persist(&node);
}

static void test_tree_persist_unshared_null_pointer(void) {
   tree_persist_unshared(NULL);
}

static void test_tree_persist_unshared_null_node(void) {
   node_t *node = NULL;
   tree_persist_unshared(&node);
}

#ifdef PERSISTENT
static void test_tree_share_refcount_overflow_child(void) {
   node_t node = {.refs = UINT64_MAX};
   tree_share(&node);
}

static void test_tree_persist_left_refcount_overflow_child(void) {
   node_t child = {.refs = UINT64_MAX};
   node_t root = {
       .l = &child,
       .s = 1,
       .refs = 1,
   };
   node_t *root_ptr = &root;
   tree_persist(&root_ptr);
}

static void test_tree_persist_right_refcount_overflow_child(void) {
   node_t child = {.refs = UINT64_MAX};
   node_t root = {
       .r = &child,
       .refs = 1,
   };
   node_t *root_ptr = &root;
   tree_persist(&root_ptr);
}

static void test_tree_persist_duplicate_child_child(void) {
   node_t child = {0};
   node_t root = {
       .l = &child,
       .r = &child,
       .s = 1,
       .refs = 1,
   };
   node_t *root_ptr = &root;
   tree_persist(&root_ptr);
}

static void test_tree_contains_shared_node_duplicate_child_child(void) {
   node_t child = {0};
   node_t root = {
       .l = &child,
       .r = &child,
       .s = 1,
   };
   (void)tree_contains_shared_node(&root);
}

static void test_tree_contains_shared_node_shared_duplicate_child_child(void) {
   node_t child = {.refs = 1};
   node_t root = {
       .l = &child,
       .r = &child,
       .s = 1,
   };
   (void)tree_contains_shared_node(&root);
}

static void test_tree_release_duplicate_child_child(void) {
   node_t child = {0};
   node_t root = {
       .l = &child,
       .r = &child,
       .s = 1,
   };
   tree_release(&root);
}

static void
test_tree_contains_shared_node_duplicate_after_seen_grow_child(void) {
   node_t nodes[TEST_NODE_SEEN_GROW_DUPLICATE_COUNT];
   (void)tree_contains_shared_node(test_node_seen_grow_duplicate_tree(
       nodes, TEST_NODE_SEEN_GROW_DUPLICATE_COUNT));
}

static void test_tree_release_duplicate_after_seen_grow_child(void) {
   node_t nodes[TEST_NODE_SEEN_GROW_DUPLICATE_COUNT];
   tree_release(test_node_seen_grow_duplicate_tree(
       nodes, TEST_NODE_SEEN_GROW_DUPLICATE_COUNT));
}
#endif

#if defined(PERSISTENT) || defined(DEBUG)
static void test_tree_roots_overlap_duplicate_child_child(void) {
   node_t child = {0};
   node_t root = {
       .l = &child,
       .r = &child,
       .s = 1,
   };
   node_t other = {0};
   tree_t left = {.root = &root, .size = 3};
   tree_t right = {.root = &other, .size = 1};
   (void)tree_roots_overlap(&left, &right);
}

static void test_tree_roots_overlap_root_size_mismatch_child(void) {
   node_t root = {0};
   tree_t left = {.root = &root, .size = 0};
   tree_t right = {0};
   (void)tree_roots_overlap(&left, &right);
}

static void assert_tree_roots_overlap_contract(void) {
   tree_t empty = {0};
   tree_t left = right_chain_of_size(5);
   tree_t right = right_chain_with_value(4, 100);
   tree_t same_left = {
       .root = left.root,
       .size = left.size,
   };
   tree_t left_tail = {
       .root = left.root->r,
       .size = node_size(left.root->r),
   };

   invariant(!tree_roots_overlap(&empty, &left));
   invariant(!tree_roots_overlap(&left, &empty));
   invariant(!tree_roots_overlap(&left, &right));
   invariant(!tree_roots_overlap(&right, &left));
   invariant(tree_roots_overlap(&left, &same_left));
   invariant(tree_roots_overlap(&same_left, &left));
   invariant(tree_roots_overlap(&left, &left_tail));
   invariant(tree_roots_overlap(&left_tail, &left));

   node_t conc_like_left_leaf = {.s = 1};
   node_t conc_like_right_leaf = {.s = 1};
   tree_t conc_like_left = {.root = &conc_like_left_leaf, .size = 1};
   tree_t conc_like_right = {.root = &conc_like_right_leaf, .size = 1};
   invariant(!tree_roots_overlap(&conc_like_left, &conc_like_right));

   free_nodes(left.root);
   free_nodes(right.root);
}
#endif

static void TestTreePersistenceOwnership(void) {
   printf("Tree persistence ownership:\n");

   invariant(!tree_contains_shared_node(NULL));

   assert_aborts(test_tree_persist_null_pointer);
   assert_aborts(test_tree_persist_null_node);
   assert_aborts(test_tree_persist_unshared_null_pointer);
   assert_aborts(test_tree_persist_unshared_null_node);
#if defined(PERSISTENT) || defined(DEBUG)
   assert_aborts(test_tree_roots_overlap_duplicate_child_child);
   assert_aborts(test_tree_roots_overlap_root_size_mismatch_child);
   assert_tree_roots_overlap_contract();
#endif
#ifdef PERSISTENT
   assert_aborts(test_tree_share_refcount_overflow_child);
   assert_aborts(test_tree_persist_left_refcount_overflow_child);
   assert_aborts(test_tree_persist_right_refcount_overflow_child);
   assert_aborts(test_tree_persist_duplicate_child_child);
   assert_aborts(test_tree_contains_shared_node_duplicate_child_child);
   assert_aborts(test_tree_contains_shared_node_shared_duplicate_child_child);
   assert_aborts(test_tree_release_duplicate_child_child);
   assert_aborts(
       test_tree_contains_shared_node_duplicate_after_seen_grow_child);
   assert_aborts(test_tree_release_duplicate_after_seen_grow_child);
#endif

   tree_t unshared = right_chain_of_size(3);
   node_t *unshared_root = unshared.root;
   node_t *unshared_right = unshared.root->r;
   tree_persist(&unshared.root);
   invariant(unshared.root == unshared_root);
   invariant(unshared.root->r == unshared_right);
   assert_equal(unshared.size, tree_verify_size(unshared.root, unshared.size));
   free_nodes(unshared.root);

   tree_t unshared_checked = right_chain_of_size(3);
   node_t *unshared_checked_root = unshared_checked.root;
   node_t *unshared_checked_right = unshared_checked.root->r;
   tree_persist_unshared(&unshared_checked.root);
   invariant(unshared_checked.root == unshared_checked_root);
   invariant(unshared_checked.root->r == unshared_checked_right);
   invariant(!tree_shared(unshared_checked.root));
   assert_equal(unshared_checked.size,
                tree_verify_size(unshared_checked.root, unshared_checked.size));
   free_nodes(unshared_checked.root);

#ifdef PERSISTENT
   tree_t shared_persist = right_chain_of_size(3);
   node_t *shared_old_root = shared_persist.root;
   node_t *shared_left = shared_old_root->l;
   node_t *shared_right = shared_old_root->r;
   tree_share(shared_persist.root);
   invariant(tree_shared(shared_old_root));
   tree_persist(&shared_persist.root);

   invariant(shared_persist.root != shared_old_root);
   invariant(!tree_shared(shared_persist.root));
   invariant(shared_persist.root->l == shared_left);
   invariant(shared_persist.root->r == shared_right);
   invariant(shared_old_root->refs == 0);
   invariant(shared_right == NULL || tree_shared(shared_right));
   assert_equal(shared_persist.size,
                tree_verify_size(shared_persist.root, shared_persist.size));

   tree_update_base(&shared_persist, 1, 4242);
   assert_equal(4242, tree_lookup(&shared_persist, 1));
   if (shared_right != NULL) {
      invariant(!tree_shared(shared_right));
   }
   free_nodes(shared_persist.root);
   tree_release(shared_old_root);

   tree_t shared_checked = right_chain_of_size(3);
   node_t *shared_checked_old_root = shared_checked.root;
   node_t *shared_checked_right = shared_checked.root->r;
   tree_share(shared_checked.root);
   tree_persist_unshared(&shared_checked.root);
   invariant(shared_checked.root != shared_checked_old_root);
   invariant(!tree_shared(shared_checked.root));
   invariant(shared_checked.root->r == shared_checked_right);
   invariant(shared_checked_old_root->refs == 0);
   invariant(shared_checked_right == NULL || tree_shared(shared_checked_right));
   assert_equal(shared_checked.size,
                tree_verify_size(shared_checked.root, shared_checked.size));
   free_nodes(shared_checked.root);
   tree_release(shared_checked_old_root);
#endif

   const u64 chain_size = NODE_STACK_INLINE_CAP * 2 + 11;
   tree_t original = right_chain_of_size(chain_size);
   invariant(!tree_contains_shared_node(original.root));
   tree_t clone = tree_clone(&original);

   assert_equal(original.size, clone.size);
   assert_equal(original.size, node_size(original.root));
   assert_equal(clone.size, node_size(clone.root));
#ifdef PERSISTENT
   invariant(original.root == clone.root);
   invariant(original.root->refs == 1);
   invariant(tree_contains_shared_node(original.root));
   invariant(tree_contains_shared_node(clone.root));
#else
   invariant(original.root != clone.root);
   invariant(!tree_contains_shared_node(original.root));
   invariant(!tree_contains_shared_node(clone.root));
#endif

   free_nodes(clone.root);
   assert_equal(original.size, node_size(original.root));
   assert_equal(original.size, tree_verify_size(original.root, original.size));
#ifdef PERSISTENT
   invariant(!tree_shared(original.root));
   invariant(!tree_contains_shared_node(original.root));
#endif

   tree_update_base(&original, original.size - 1, 9001);
   assert_equal(9001, tree_lookup(&original, original.size - 1));
   assert_equal(original.size, tree_verify_size(original.root, original.size));

   tree_t shared = tree_clone(&original);
#ifdef PERSISTENT
   invariant(original.root == shared.root);
   invariant(original.root->refs == 1);
   invariant(tree_contains_shared_node(original.root));
   invariant(tree_contains_shared_node(shared.root));
#else
   invariant(original.root != shared.root);
#endif

   u64 original_root_value = tree_lookup(&original, 0);
   tree_update_base(&original, 0, 1234);
   assert_equal(1234, tree_lookup(&original, 0));
   assert_equal(original_root_value, tree_lookup(&shared, 0));
   assert_equal(original.size, tree_verify_size(original.root, original.size));
   assert_equal(shared.size, tree_verify_size(shared.root, shared.size));
#ifdef PERSISTENT
   invariant(!tree_shared(original.root));
   invariant(!tree_shared(shared.root));
   invariant(tree_contains_shared_node(original.root));
   invariant(tree_contains_shared_node(shared.root));
#endif

   free_nodes(shared.root);
   assert_equal(original.size, node_size(original.root));
   assert_equal(original.size, tree_verify_size(original.root, original.size));
   assert_equal(1234, tree_lookup(&original, 0));
#ifdef PERSISTENT
   invariant(!tree_contains_shared_node(original.root));
#endif

#ifdef PERSISTENT
   tree_t split_source = right_chain_of_size(chain_size);
   u64 cut = NODE_STACK_INLINE_CAP + 3;
   tree_split_t split = tree_split_base(&split_source, cut);
   tree_t left = split.left;
   tree_t right = split.right;

   assert_equal(chain_size, split_source.size);
   assert_equal(cut, left.size);
   assert_equal(chain_size - cut, right.size);
   assert_equal(split_source.size,
                tree_verify_size(split_source.root, split_source.size));
   assert_equal(left.size, tree_verify_size(left.root, left.size));
   assert_equal(right.size, tree_verify_size(right.root, right.size));
   assert_equal(0, tree_lookup(&split_source, 0));
   assert_equal(cut - 1, tree_lookup(&left, cut - 1));
   assert_equal(cut, tree_lookup(&right, 0));

   tree_update_base(&left, 0, 777);
   tree_update_base(&right, 0, 888);
   assert_equal(0, tree_lookup(&split_source, 0));
   assert_equal(cut, tree_lookup(&split_source, cut));
   assert_equal(777, tree_lookup(&left, 0));
   assert_equal(888, tree_lookup(&right, 0));

   free_nodes(left.root);
   free_nodes(right.root);
   assert_equal(split_source.size,
                tree_verify_size(split_source.root, split_source.size));
   assert_equal(0, tree_lookup(&split_source, 0));
   assert_equal(cut, tree_lookup(&split_source, cut));
   free_nodes(split_source.root);

   tree_t join_left = right_chain_of_size(NODE_STACK_INLINE_CAP + 1);
   tree_t join_right = right_chain_of_size(NODE_STACK_INLINE_CAP + 2);
   tree_t joined = tree_join_base(&join_left, &join_right);

   assert_equal(join_left.size + join_right.size, joined.size);
   assert_equal(joined.size, tree_verify_size(joined.root, joined.size));
   assert_equal(join_left.size,
                tree_verify_size(join_left.root, join_left.size));
   assert_equal(join_right.size,
                tree_verify_size(join_right.root, join_right.size));
   assert_equal(0, tree_lookup(&join_left, 0));
   assert_equal(0, tree_lookup(&join_right, 0));
   assert_equal(0, tree_lookup(&joined, 0));
   assert_equal(0, tree_lookup(&joined, join_left.size));

   tree_update_base(&joined, 0, 111);
   tree_update_base(&joined, join_left.size, 222);
   assert_equal(0, tree_lookup(&join_left, 0));
   assert_equal(0, tree_lookup(&join_right, 0));
   assert_equal(111, tree_lookup(&joined, 0));
   assert_equal(222, tree_lookup(&joined, join_left.size));

   free_nodes(joined.root);
   assert_equal(join_left.size,
                tree_verify_size(join_left.root, join_left.size));
   assert_equal(join_right.size,
                tree_verify_size(join_right.root, join_right.size));
   free_nodes(join_left.root);
   free_nodes(join_right.root);

   tree_t reused_left_source = right_chain_of_size(3);
   tree_t reused_left_join_left = tree_clone(&reused_left_source);
   tree_t reused_left_join_right = right_chain_of_size(5);
   node_t *reused_left_root = reused_left_join_left.root;
   tree_t reused_left_joined =
       tree_join_base(&reused_left_join_left, &reused_left_join_right);

   invariant(reused_left_joined.root->l == reused_left_root);
   assert_equal(
       reused_left_joined.size,
       tree_verify_size(reused_left_joined.root, reused_left_joined.size));
   free_nodes(reused_left_joined.root);
   assert_equal(
       reused_left_source.size,
       tree_verify_size(reused_left_source.root, reused_left_source.size));
   free_nodes(reused_left_join_left.root);
   free_nodes(reused_left_join_right.root);
   free_nodes(reused_left_source.root);

   tree_t reused_right_join_left = right_chain_of_size(5);
   tree_t reused_right_source = right_chain_of_size(3);
   tree_t reused_right_join_right = tree_clone(&reused_right_source);
   node_t *reused_right_root = reused_right_join_right.root;
   tree_t reused_right_joined =
       tree_join_base(&reused_right_join_left, &reused_right_join_right);

   invariant(reused_right_joined.root->r == reused_right_root);
   assert_equal(
       reused_right_joined.size,
       tree_verify_size(reused_right_joined.root, reused_right_joined.size));
   free_nodes(reused_right_joined.root);
   assert_equal(
       reused_right_source.size,
       tree_verify_size(reused_right_source.root, reused_right_source.size));
   free_nodes(reused_right_join_left.root);
   free_nodes(reused_right_join_right.root);
   free_nodes(reused_right_source.root);

   tree_t empty_join_left = {0};
   tree_t empty_join_source = perfect_ranked_tree(5);
   tree_t empty_join_right = tree_clone(&empty_join_source);
   invariant(empty_join_source.root == empty_join_right.root);
   invariant(tree_contains_shared_node(empty_join_right.root));
   tree_t empty_joined = tree_join_base(&empty_join_left, &empty_join_right);
   invariant(empty_joined.root == empty_join_source.root);
   assert_equal(empty_join_source.size, empty_joined.size);
   assert_equal(empty_joined.size,
                tree_verify_size(empty_joined.root, empty_joined.size));
   assert_equal(
       empty_join_source.size,
       tree_verify_size(empty_join_source.root, empty_join_source.size));
   tree_update_base(&empty_joined, 0, 7777);
   assert_equal(7777, tree_lookup(&empty_joined, 0));
   assert_equal(0, tree_lookup(&empty_join_source, 0));
   free_nodes(empty_joined.root);
   assert_equal(
       empty_join_source.size,
       tree_verify_size(empty_join_source.root, empty_join_source.size));
   free_nodes(empty_join_source.root);
   free_nodes(empty_join_right.root);

   const u64 external_join_left_size = NODE_SEEN_INLINE_CAP + 9;
   const u64 external_join_right_size = NODE_SEEN_INLINE_CAP + 11;
   const u64 external_join_right_index = NODE_SEEN_INLINE_CAP;
   tree_t external_join_left =
       right_chain_with_value(external_join_left_size, 7000);
   tree_t external_join_source = right_chain_of_size(external_join_right_size);
   tree_t external_join_right = tree_clone(&external_join_source);
   invariant(external_join_source.root == external_join_right.root);
   invariant(tree_contains_shared_node(external_join_right.root));
   tree_t external_joined =
       tree_join_base(&external_join_left, &external_join_right);

   assert_equal(external_join_left.size + external_join_right.size,
                external_joined.size);
   assert_equal(external_joined.size,
                tree_verify_size(external_joined.root, external_joined.size));
   assert_equal(
       external_join_source.size,
       tree_verify_size(external_join_source.root, external_join_source.size));
   assert_equal(
       external_join_right.size,
       tree_verify_size(external_join_right.root, external_join_right.size));
   invariant(tree_contains_shared_node(external_joined.root));
   assert_equal(7000, tree_lookup(&external_joined, 0));
   assert_equal(0, tree_lookup(&external_joined, external_join_left.size));
   assert_equal(7000,
                tree_lookup(&external_joined, external_join_left.size - 1));
   assert_equal(external_join_right_index,
                tree_lookup(&external_joined, external_join_left.size +
                                                  external_join_right_index));

   tree_update_base(&external_joined,
                    external_join_left.size + external_join_right_index, 7777);
   assert_equal(7777,
                tree_lookup(&external_joined, external_join_left.size +
                                                  external_join_right_index));
   assert_equal(external_join_right_index,
                tree_lookup(&external_join_source, external_join_right_index));
   assert_equal(external_join_right_index,
                tree_lookup(&external_join_right, external_join_right_index));

   free_nodes(external_joined.root);
   assert_equal(
       external_join_source.size,
       tree_verify_size(external_join_source.root, external_join_source.size));
   assert_equal(
       external_join_right.size,
       tree_verify_size(external_join_right.root, external_join_right.size));
   free_nodes(external_join_left.root);
   free_nodes(external_join_right.root);
   free_nodes(external_join_source.root);

   tree_t prepare_join_base = perfect_ranked_tree(7);
   tree_t prepare_join_shared = tree_clone(&prepare_join_base);
   tree_split_t prepare_join_split = tree_split_base(&prepare_join_shared, 9);
   tree_t prepare_join_left = prepare_join_split.left;
   tree_t prepare_join_tail = prepare_join_split.right;
   invariant(prepare_join_left.size < prepare_join_base.size);

   tree_join_roots_t prepared =
       tree_prepare_join_roots(&prepare_join_left, &prepare_join_base);
   invariant(prepared.left != prepare_join_left.root);
   invariant(prepared.right == prepare_join_base.root);
   invariant(!tree_contains_shared_node(prepared.left));
   invariant(tree_contains_shared_node(prepared.right));

   tree_t prepare_joined = {
       .root =
           tree_join_internal(prepared.left, prepared.right,
                              prepare_join_left.size, prepare_join_base.size),
       .size =
           tree_list_join_size(prepare_join_left.size, prepare_join_base.size),
   };
   assert_equal(prepare_joined.size,
                tree_verify_size(prepare_joined.root, prepare_joined.size));
   assert_equal(0, tree_lookup(&prepare_joined, 0));
   assert_equal(8, tree_lookup(&prepare_joined, prepare_join_left.size - 1));
   assert_equal(0, tree_lookup(&prepare_joined, prepare_join_left.size));
   assert_equal(0, tree_lookup(&prepare_join_left, 0));
   assert_equal(0, tree_lookup(&prepare_join_base, 0));

   tree_update_base(&prepare_joined, 0, 8080);
   tree_update_base(&prepare_joined, prepare_join_left.size, 9090);
   assert_equal(8080, tree_lookup(&prepare_joined, 0));
   assert_equal(9090, tree_lookup(&prepare_joined, prepare_join_left.size));
   assert_equal(0, tree_lookup(&prepare_join_left, 0));
   assert_equal(0, tree_lookup(&prepare_join_base, 0));

   free_nodes(prepare_joined.root);
   assert_equal(
       prepare_join_base.size,
       tree_verify_size(prepare_join_base.root, prepare_join_base.size));
   assert_equal(
       prepare_join_left.size,
       tree_verify_size(prepare_join_left.root, prepare_join_left.size));
   free_nodes(prepare_join_left.root);
   free_nodes(prepare_join_tail.root);
   free_nodes(prepare_join_shared.root);
   free_nodes(prepare_join_base.root);

   tree_t shared_join_base = perfect_ranked_tree(6);
   tree_t shared_join_left = tree_clone(&shared_join_base);
   tree_t shared_join_right = tree_clone(&shared_join_base);
   tree_update_base(&shared_join_left, 0, 3333);
   tree_update_base(&shared_join_right, shared_join_right.size - 1, 4444);
   invariant(tree_contains_shared_node(shared_join_left.root));
   invariant(tree_contains_shared_node(shared_join_right.root));

   tree_t shared_joined = tree_join_base(&shared_join_left, &shared_join_right);
   assert_equal(shared_join_left.size + shared_join_right.size,
                shared_joined.size);
   assert_equal(shared_join_base.size,
                tree_verify_size(shared_join_base.root, shared_join_base.size));
   assert_equal(shared_join_left.size,
                tree_verify_size(shared_join_left.root, shared_join_left.size));
   assert_equal(
       shared_join_right.size,
       tree_verify_size(shared_join_right.root, shared_join_right.size));
   assert_equal(shared_joined.size,
                tree_verify_size(shared_joined.root, shared_joined.size));
   assert_equal(3333, tree_lookup(&shared_joined, 0));
   assert_equal(4444, tree_lookup(&shared_joined, shared_joined.size - 1));
   assert_equal(0, tree_lookup(&shared_join_base, 0));

   tree_update_base(&shared_joined, 0, 5555);
   tree_update_base(&shared_joined, shared_join_left.size, 6666);
   assert_equal(3333, tree_lookup(&shared_join_left, 0));
   assert_equal(0, tree_lookup(&shared_join_right, 0));
   assert_equal(5555, tree_lookup(&shared_joined, 0));
   assert_equal(6666, tree_lookup(&shared_joined, shared_join_left.size));

   free_nodes(shared_joined.root);
   assert_equal(shared_join_left.size,
                tree_verify_size(shared_join_left.root, shared_join_left.size));
   assert_equal(
       shared_join_right.size,
       tree_verify_size(shared_join_right.root, shared_join_right.size));
   free_nodes(shared_join_left.root);
   free_nodes(shared_join_right.root);
   assert_equal(shared_join_base.size,
                tree_verify_size(shared_join_base.root, shared_join_base.size));
   free_nodes(shared_join_base.root);

   tree_t delete_source = perfect_ranked_tree(6);
   tree_t delete_snapshot = tree_clone(&delete_source);
   u64 deleted = tree_delete_base(&delete_source, delete_source.root->s);

   assert_equal(63, deleted);
   assert_equal(delete_snapshot.size - 1, delete_source.size);
   assert_equal(delete_source.size,
                tree_verify_size(delete_source.root, delete_source.size));
   assert_equal(delete_snapshot.size,
                tree_verify_size(delete_snapshot.root, delete_snapshot.size));
   assert_equal(63, tree_lookup(&delete_snapshot, 63));
   assert_equal(62, tree_lookup(&delete_source, 62));
   assert_equal(64, tree_lookup(&delete_source, 63));

   tree_update_base(&delete_source, 63, 444);
   assert_equal(444, tree_lookup(&delete_source, 63));
   assert_equal(63, tree_lookup(&delete_snapshot, 63));

   free_nodes(delete_source.root);
   assert_equal(delete_snapshot.size,
                tree_verify_size(delete_snapshot.root, delete_snapshot.size));
   assert_equal(63, tree_lookup(&delete_snapshot, 63));
   free_nodes(delete_snapshot.root);

   tree_t delete_right_only = right_chain_of_size(4);
   tree_t delete_right_only_snapshot = tree_clone(&delete_right_only);
   u64 delete_right_only_value = tree_delete_base(&delete_right_only, 0);
   u64 delete_right_only_values[] = {1, 2, 3};
   u64 delete_right_only_snapshot_values[] = {0, 1, 2, 3};

   assert_equal(0, delete_right_only_value);
   assert_tree_values(delete_right_only, delete_right_only_values,
                      sizeof(delete_right_only_values) /
                          sizeof(delete_right_only_values[0]));
   assert_tree_values(delete_right_only_snapshot,
                      delete_right_only_snapshot_values,
                      sizeof(delete_right_only_snapshot_values) /
                          sizeof(delete_right_only_snapshot_values[0]));

   tree_update_base(&delete_right_only, 0, 8081);
   assert_equal(8081, tree_lookup(&delete_right_only, 0));
   assert_equal(0, tree_lookup(&delete_right_only_snapshot, 0));

   free_nodes(delete_right_only.root);
   assert_tree_values(delete_right_only_snapshot,
                      delete_right_only_snapshot_values,
                      sizeof(delete_right_only_snapshot_values) /
                          sizeof(delete_right_only_snapshot_values[0]));
   free_nodes(delete_right_only_snapshot.root);

   tree_t delete_left_only = left_chain_of_size(4);
   tree_t delete_left_only_snapshot = tree_clone(&delete_left_only);
   u64 delete_left_only_value =
       tree_delete_base(&delete_left_only, delete_left_only.size - 1);
   u64 delete_left_only_values[] = {0, 1, 2};
   u64 delete_left_only_snapshot_values[] = {0, 1, 2, 3};

   assert_equal(3, delete_left_only_value);
   assert_tree_values(delete_left_only, delete_left_only_values,
                      sizeof(delete_left_only_values) /
                          sizeof(delete_left_only_values[0]));
   assert_tree_values(delete_left_only_snapshot,
                      delete_left_only_snapshot_values,
                      sizeof(delete_left_only_snapshot_values) /
                          sizeof(delete_left_only_snapshot_values[0]));

   tree_update_base(&delete_left_only, delete_left_only.size - 1, 8082);
   assert_equal(8082,
                tree_lookup(&delete_left_only, delete_left_only.size - 1));
   assert_equal(2, tree_lookup(&delete_left_only_snapshot, 2));

   free_nodes(delete_left_only.root);
   assert_tree_values(delete_left_only_snapshot,
                      delete_left_only_snapshot_values,
                      sizeof(delete_left_only_snapshot_values) /
                          sizeof(delete_left_only_snapshot_values[0]));
   free_nodes(delete_left_only_snapshot.root);

   node_t *delete_right_heavy_left = tree_allocate((node_t){.x = 0});
   node_t *delete_right_heavy_tail = tree_allocate((node_t){.x = 3});
   node_t *delete_right_heavy_right = tree_allocate((node_t){
       .r = delete_right_heavy_tail,
       .x = 2,
   });
   tree_t delete_right_heavy = {
       .root = tree_allocate((node_t){
           .l = delete_right_heavy_left,
           .r = delete_right_heavy_right,
           .s = 1,
           .x = 1,
       }),
       .size = 4,
   };
   tree_t delete_right_heavy_snapshot = tree_clone(&delete_right_heavy);
   u64 delete_right_heavy_value =
       tree_delete_base(&delete_right_heavy, delete_right_heavy.root->s);
   u64 delete_right_heavy_values[] = {0, 2, 3};
   u64 delete_right_heavy_snapshot_values[] = {0, 1, 2, 3};

   assert_equal(1, delete_right_heavy_value);
   assert_tree_values(delete_right_heavy, delete_right_heavy_values,
                      sizeof(delete_right_heavy_values) /
                          sizeof(delete_right_heavy_values[0]));
   assert_tree_values(delete_right_heavy_snapshot,
                      delete_right_heavy_snapshot_values,
                      sizeof(delete_right_heavy_snapshot_values) /
                          sizeof(delete_right_heavy_snapshot_values[0]));

   tree_update_base(&delete_right_heavy, 1, 8083);
   assert_equal(8083, tree_lookup(&delete_right_heavy, 1));
   assert_equal(1, tree_lookup(&delete_right_heavy_snapshot, 1));

   free_nodes(delete_right_heavy.root);
   assert_tree_values(delete_right_heavy_snapshot,
                      delete_right_heavy_snapshot_values,
                      sizeof(delete_right_heavy_snapshot_values) /
                          sizeof(delete_right_heavy_snapshot_values[0]));
   free_nodes(delete_right_heavy_snapshot.root);

   node_t *delete_left_heavy_head = tree_allocate((node_t){.x = 0});
   node_t *delete_left_heavy_left = tree_allocate((node_t){
       .l = delete_left_heavy_head,
       .s = 1,
       .x = 1,
   });
   node_t *delete_left_heavy_right = tree_allocate((node_t){.x = 3});
   tree_t delete_left_heavy = {
       .root = tree_allocate((node_t){
           .l = delete_left_heavy_left,
           .r = delete_left_heavy_right,
           .s = 2,
           .x = 2,
       }),
       .size = 4,
   };
   tree_t delete_left_heavy_snapshot = tree_clone(&delete_left_heavy);
   u64 delete_left_heavy_value =
       tree_delete_base(&delete_left_heavy, delete_left_heavy.root->s);
   u64 delete_left_heavy_values[] = {0, 1, 3};
   u64 delete_left_heavy_snapshot_values[] = {0, 1, 2, 3};

   assert_equal(2, delete_left_heavy_value);
   assert_tree_values(delete_left_heavy, delete_left_heavy_values,
                      sizeof(delete_left_heavy_values) /
                          sizeof(delete_left_heavy_values[0]));
   assert_tree_values(delete_left_heavy_snapshot,
                      delete_left_heavy_snapshot_values,
                      sizeof(delete_left_heavy_snapshot_values) /
                          sizeof(delete_left_heavy_snapshot_values[0]));

   tree_update_base(&delete_left_heavy, delete_left_heavy.size - 1, 8084);
   assert_equal(8084,
                tree_lookup(&delete_left_heavy, delete_left_heavy.size - 1));
   assert_equal(2, tree_lookup(&delete_left_heavy_snapshot, 2));

   free_nodes(delete_left_heavy.root);
   assert_tree_values(delete_left_heavy_snapshot,
                      delete_left_heavy_snapshot_values,
                      sizeof(delete_left_heavy_snapshot_values) /
                          sizeof(delete_left_heavy_snapshot_values[0]));
   free_nodes(delete_left_heavy_snapshot.root);

   tree_t partition_source = right_chain_of_size(chain_size);
   tree_t partition_snapshot = tree_clone(&partition_source);
   u64 pivot = NODE_STACK_INLINE_CAP + 5;
   partition_source.root = tree_partition_sized(partition_source.root,
                                                partition_source.size, pivot);

   assert_equal(partition_source.size,
                tree_verify_size(partition_source.root, partition_source.size));
   assert_equal(
       partition_snapshot.size,
       tree_verify_size(partition_snapshot.root, partition_snapshot.size));
   invariant(partition_source.root != partition_snapshot.root);
   assert_equal(pivot, partition_source.root->s);
   assert_equal(pivot, tree_lookup(&partition_source, pivot));
   assert_equal(pivot, tree_lookup(&partition_snapshot, pivot));

   tree_update_base(&partition_source, pivot, 333);
   assert_equal(333, tree_lookup(&partition_source, pivot));
   assert_equal(pivot, tree_lookup(&partition_snapshot, pivot));

   free_nodes(partition_source.root);
   assert_equal(
       partition_snapshot.size,
       tree_verify_size(partition_snapshot.root, partition_snapshot.size));
   assert_equal(pivot, tree_lookup(&partition_snapshot, pivot));
   free_nodes(partition_snapshot.root);

   tree_t replace_right_source = perfect_ranked_tree(5);
   tree_t replace_right_snapshot = tree_clone(&replace_right_source);
   u64 replace_right_split = replace_right_source.root->s + 1;
   node_t *replace_right_detached_root =
       tree_replacedByRightSubtree(&replace_right_source.root);
   tree_t replace_right_detached = {
       .root = replace_right_detached_root,
       .size = replace_right_split,
   };
   replace_right_source.size -= replace_right_split;

   assert_equal(replace_right_detached.size,
                tree_verify_size(replace_right_detached.root,
                                 replace_right_detached.size));
   assert_equal(
       replace_right_source.size,
       tree_verify_size(replace_right_source.root, replace_right_source.size));
   assert_equal(replace_right_snapshot.size,
                tree_verify_size(replace_right_snapshot.root,
                                 replace_right_snapshot.size));
   assert_equal(
       replace_right_split - 1,
       tree_lookup(&replace_right_detached, replace_right_detached.size - 1));
   assert_equal(replace_right_split, tree_lookup(&replace_right_source, 0));
   assert_equal(replace_right_split - 1,
                tree_lookup(&replace_right_snapshot, replace_right_split - 1));
   assert_equal(replace_right_split,
                tree_lookup(&replace_right_snapshot, replace_right_split));

   tree_update_base(&replace_right_detached, replace_right_detached.size - 1,
                    6161);
   tree_update_base(&replace_right_source, 0, 6262);
   assert_equal(6161, tree_lookup(&replace_right_detached,
                                  replace_right_detached.size - 1));
   assert_equal(6262, tree_lookup(&replace_right_source, 0));
   assert_equal(replace_right_split - 1,
                tree_lookup(&replace_right_snapshot, replace_right_split - 1));
   assert_equal(replace_right_split,
                tree_lookup(&replace_right_snapshot, replace_right_split));

   free_nodes(replace_right_detached.root);
   free_nodes(replace_right_source.root);
   assert_equal(replace_right_snapshot.size,
                tree_verify_size(replace_right_snapshot.root,
                                 replace_right_snapshot.size));
   free_nodes(replace_right_snapshot.root);

   tree_t replace_left_source = perfect_ranked_tree(5);
   tree_t replace_left_snapshot = tree_clone(&replace_left_source);
   u64 replace_left_prefix = replace_left_source.root->s;
   node_t *replace_left_detached_root =
       tree_replacedByLeftSubtree(&replace_left_source.root);
   tree_t replace_left_detached = {
       .root = replace_left_detached_root,
       .size = replace_left_source.size - replace_left_prefix,
   };
   replace_left_source.size = replace_left_prefix;

   assert_equal(
       replace_left_source.size,
       tree_verify_size(replace_left_source.root, replace_left_source.size));
   assert_equal(replace_left_detached.size,
                tree_verify_size(replace_left_detached.root,
                                 replace_left_detached.size));
   assert_equal(replace_left_snapshot.size,
                tree_verify_size(replace_left_snapshot.root,
                                 replace_left_snapshot.size));
   assert_equal(
       replace_left_prefix - 1,
       tree_lookup(&replace_left_source, replace_left_source.size - 1));
   assert_equal(replace_left_prefix, tree_lookup(&replace_left_detached, 0));
   assert_equal(replace_left_prefix - 1,
                tree_lookup(&replace_left_snapshot, replace_left_prefix - 1));
   assert_equal(replace_left_prefix,
                tree_lookup(&replace_left_snapshot, replace_left_prefix));

   tree_update_base(&replace_left_source, replace_left_source.size - 1, 7171);
   tree_update_base(&replace_left_detached, 0, 7272);
   assert_equal(
       7171, tree_lookup(&replace_left_source, replace_left_source.size - 1));
   assert_equal(7272, tree_lookup(&replace_left_detached, 0));
   assert_equal(replace_left_prefix - 1,
                tree_lookup(&replace_left_snapshot, replace_left_prefix - 1));
   assert_equal(replace_left_prefix,
                tree_lookup(&replace_left_snapshot, replace_left_prefix));

   free_nodes(replace_left_source.root);
   free_nodes(replace_left_detached.root);
   assert_equal(replace_left_snapshot.size,
                tree_verify_size(replace_left_snapshot.root,
                                 replace_left_snapshot.size));
   free_nodes(replace_left_snapshot.root);
#endif

   free_nodes(original.root);
#ifdef PERSISTENT
   printf("  clone, split, join, and copy-on-write ownership OK\n");
#else
   printf("  clone release and ownership OK\n");
#endif
}

static void assert_sample_schedule(u64 total, u64 samples) {
   invariant(total > 0);
   invariant(samples > 0);
   invariant(samples <= 1024);
   sample_schedule_t schedule = {
       .total = total,
       .samples = samples,
   };
   sample_schedule_require(schedule);
   u64 previous = 0;
   for (u64 i = 0; i < samples; i++) {
      sample_schedule_point_t point =
          sample_schedule_next_point(schedule, i, previous);
      assert_equal(size_after_inserting_one(i), point.ordinal);
      assert_equal(point.position,
                   sample_schedule_position(schedule, point.ordinal));
      assert_equal(point.position,
                   sample_position(total, point.ordinal, samples));
      invariant(point.position <= total);
      previous = point.position;
   }
   assert_equal(total, previous);
}

static u64 test_infer_sample_schedule_from_position(u64 total, u64 sample,
                                                    u64 position) {
   invariant(total > 0);
   invariant(sample > 0);
   invariant(position > 0);
   invariant(position <= total);

   __uint128_t scaled = (__uint128_t)sample * (__uint128_t)total;
   __uint128_t candidate = scaled / (__uint128_t)position;
   if (candidate > UINT64_MAX) {
      return 0;
   }
   u64 samples = (u64)candidate;
   if (samples < sample || samples > total) {
      return 0;
   }
   if (sample_position(total, sample, samples) != position) {
      return 0;
   }
   if (samples > sample &&
       sample_position(total, sample, samples - 1) == position) {
      return 0;
   }
   return samples;
}

static u64 test_infer_sample_schedule_from_first_position(u64 total,
                                                          u64 first_position) {
   return test_infer_sample_schedule_from_position(total, 1, first_position);
}

static pid_t
assert_operation_run_config_exits_2(operation_t op,
                                    operation_run_config_t config) {
   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      operation_run_matrix(&op, &config, NULL, NULL, NULL, NULL);
      _exit(0);
   }
   assert_child_exited_2(pid);
   return pid;
}

static pid_t
assert_operation_run_config_exits_2_with_stderr(operation_t op,
                                                operation_run_config_t config,
                                                const char *expected_stderr) {
   invariant(expected_stderr != NULL);

   char stderr_path[] = "/tmp/bst-operation-run-stderr-XXXXXX";
   int stderr_fd = mkstemp(stderr_path);
   invariant(stderr_fd >= 0);
   invariant(close(stderr_fd) == 0);

   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen(stderr_path, "w", stderr);
      invariant(err != NULL);
      operation_run_matrix(&op, &config, NULL, NULL, NULL, NULL);
      _exit(0);
   }
   assert_child_exited_2(pid);

   char *stderr_text = test_read_text_file(stderr_path);
   test_require_substring(stderr_text, expected_stderr);
   free(stderr_text);
   invariant(remove(stderr_path) == 0);
   return pid;
}

static void test_operation_insert_delete_overfull_child(void) {
   (void)operation_insert_delete_should_insert(3, 4, 6);
}

static void test_operation_insert_delete_zero_step_child(void) {
   (void)operation_insert_delete_should_insert(3, 0, 0);
}

static void test_operation_insert_delete_size_ahead_child(void) {
   (void)operation_insert_delete_should_insert(10, 3, 2);
}

static void test_operation_step_count_overflow_child(void) {
   operation_t operations[operation_default_count];
   assert_int_equal(operation_default_count, operation_defaults(operations));
   list_t instance = list_new(&reference_vtable);
   distribution_t *dist = uniform_new(1);
   u64 step_count = UINT64_MAX;
   (void)operation_step(&operations[0], instance, dist, &step_count);
}

static void test_operation_step_after_declared_steps_child(void) {
   operation_t operations[operation_default_count];
   assert_int_equal(operation_default_count, operation_defaults(operations));
   operations[0].scale = 1;
   operations[0].steps = 1;
   list_t instance = list_new(&reference_vtable);
   distribution_t *dist = uniform_new(1);
   u64 step_count = operations[0].steps;
   (void)operation_step(&operations[0], instance, dist, &step_count);
}

static void test_operation_duplicate_name_child(void) {
   operation_t operations[operation_default_count];
   assert_int_equal(operation_default_count, operation_defaults(operations));
   operations[1] = operations[0];
   (void)operation_find(operations, operation_default_count,
                        operations[0].name);
}

static void test_operation_find_negative_count_child(void) {
   (void)operation_find(NULL, -1, NULL);
}

static void test_operation_find_or_exit_negative_count_child(void) {
   (void)operation_find_or_exit(NULL, -1, NULL);
}

static void test_operation_fprint_names_negative_count_child(void) {
   operation_fprint_names(stdout, "operations", NULL, -1);
}

static void test_operation_find_or_exit_null_child(void) {
   operation_t operations[operation_default_count];
   assert_int_equal(operation_default_count, operation_defaults(operations));
   (void)operation_find_or_exit(operations, operation_default_count, NULL);
}

static void test_operation_find_or_exit_empty_child(void) {
   operation_t operations[operation_default_count];
   assert_int_equal(operation_default_count, operation_defaults(operations));
   (void)operation_find_or_exit(operations, operation_default_count, "");
}

static void test_operation_find_or_exit_bad_name_child(void) {
   operation_t operations[operation_default_count];
   assert_int_equal(operation_default_count, operation_defaults(operations));
   (void)operation_find_or_exit(operations, operation_default_count,
                                "Bad/Name");
}

static void test_operation_find_or_exit_missing_child(void) {
   operation_t operations[operation_default_count];
   assert_int_equal(operation_default_count, operation_defaults(operations));
   (void)operation_find_or_exit(operations, operation_default_count,
                                "NotAnOperation");
}

static void test_sample_position_zero_total_child(void) {
   (void)sample_position(0, 1, 1);
}

static void test_sample_position_too_many_samples_child(void) {
   (void)sample_position(3, 1, 4);
}

static void test_sample_scale_floor_late_numerator_child(void) {
   (void)sample_scale_floor(4, 3, 3);
}

static void test_sample_scale_floor_large_denominator_child(void) {
   (void)sample_scale_floor(1, 3, 4);
}

static void test_sample_position_zero_sample_count_child(void) {
   (void)sample_position(3, 1, 0);
}

static void test_sample_position_zero_sample_ordinal_child(void) {
   (void)sample_position(3, 0, 3);
}

static void test_sample_position_late_sample_child(void) {
   (void)sample_position(3, 4, 3);
}

static void test_sample_schedule_position_bad_schedule_child(void) {
   sample_schedule_t schedule = {.total = 3, .samples = 4};
   (void)sample_schedule_position(schedule, 1);
}

static void test_sample_schedule_require_bad_schedule_child(void) {
   sample_schedule_t schedule = {.total = 3, .samples = 4};
   sample_schedule_require(schedule);
}

static void test_sample_schedule_require_zero_total_child(void) {
   sample_schedule_t schedule = {.total = 0, .samples = 1};
   sample_schedule_require(schedule);
}

static void test_sample_schedule_require_zero_samples_child(void) {
   sample_schedule_t schedule = {.total = 3, .samples = 0};
   sample_schedule_require(schedule);
}

static void test_operation_run_sample_schedule_too_large_child(void) {
   (void)operation_run_sample_schedule(UINT64_MAX, 1);
}

static void test_operation_run_next_iteration_overflow_child(void) {
   (void)operation_run_next_iteration(UINT64_MAX);
}

static void test_sample_schedule_position_late_sample_child(void) {
   sample_schedule_t schedule = {.total = 3, .samples = 3};
   (void)sample_schedule_position(schedule, 4);
}

static void test_sample_schedule_position_zero_sample_child(void) {
   sample_schedule_t schedule = sample_schedule_make(3, 3);
   (void)sample_schedule_position(schedule, 0);
}

static void test_sample_schedule_point_late_index_child(void) {
   sample_schedule_t schedule = sample_schedule_make(3, 3);
   (void)sample_schedule_point(schedule, 3);
}

static void test_sample_schedule_next_point_nonmonotonic_child(void) {
   sample_schedule_t schedule = sample_schedule_make(10, 10);
   (void)sample_schedule_next_point(schedule, 0, 1);
}

static void test_format_local_time_empty_format_child(void) {
   char buf[16];
   format_local_time_or_exit(buf, sizeof(buf), "");
}

static void test_format_local_time_null_buffer_child(void) {
   format_local_time_or_exit(NULL, 16, "%Y");
}

static void test_format_local_time_zero_size_child(void) {
   char buf[1];
   format_local_time_or_exit(buf, 0, "%Y");
}

static void test_format_local_time_null_format_child(void) {
   char buf[16];
   format_local_time_or_exit(buf, sizeof(buf), NULL);
}

static void test_format_local_time_tiny_buffer_child(void) {
   char buf[1];
   format_local_time_or_exit(buf, sizeof(buf), "%Y");
}

static void test_duration_reached_zero_duration_child(void) {
   (void)duration_reached(0, 0);
}

static void test_duration_sum_overflow_child(void) {
   (void)duration_sum(UINT64_MAX, 1);
}

static void test_duration_timespec_negative_seconds_child(void) {
   (void)duration_ns_from_timespec((struct timespec){
       .tv_sec = -1,
       .tv_nsec = 0,
   });
}

static void test_duration_timespec_negative_nanoseconds_child(void) {
   (void)duration_ns_from_timespec((struct timespec){
       .tv_sec = 0,
       .tv_nsec = -1,
   });
}

static void test_duration_timespec_large_nanoseconds_child(void) {
   (void)duration_ns_from_timespec((struct timespec){
       .tv_sec = 0,
       .tv_nsec = (long)TIME_NS_PER_SECOND,
   });
}

static void test_duration_timespec_overflow_child(void) {
   u64 max_seconds = UINT64_MAX / TIME_NS_PER_SECOND;
   u64 overflow_nanoseconds =
       size_after_inserting_one(UINT64_MAX % TIME_NS_PER_SECOND);
   (void)duration_ns_from_timespec((struct timespec){
       .tv_sec = (time_t)max_seconds,
       .tv_nsec = (long)overflow_nanoseconds,
   });
}

static void test_duration_average_zero_count_child(void) {
   (void)duration_average_nonzero(1, 0);
}

static void test_duration_average_zero_average_child(void) {
   (void)duration_average_nonzero(1, 2);
}

static void test_elapsed_time_ns_since_future_child(void) {
   (void)elapsed_time_ns_since(UINT64_MAX);
}

static void test_elapsed_time_ns_reached_zero_duration_child(void) {
   (void)elapsed_time_ns_reached(monotonic_time_ns(), 0);
}

static operation_run_t test_operation_workload_seed_run(operation_t *op) {
   invariant(op != NULL);
   operation_t operations[operation_default_count];
   int operation_count = operation_defaults(operations);
   invariant(operation_count == operation_default_count);
   *op = operations[0];
   op->scale = 1;
   op->steps = 1;
   const dist_spec_t *dist = dist_find("Uniform");
   invariant(dist != NULL);
   return (operation_run_t){
       .op = op,
       .tree_vt = &reference_vtable,
       .dist = dist,
       .scale = op->scale,
       .steps = op->steps,
       .iteration = 1,
       .iterations = 1,
   };
}

static u64 test_operation_tree_rng_first_value(operation_run_t run) {
   operation_require(run.op);
   list_vtable_require(run.tree_vt);
   dist_spec_require(run.dist);
   invariant(run.scale > 0);
   invariant(run.steps > 0);
   invariant(run.iteration > 0);
   invariant(run.iteration <= run.iterations);

   u64 seed = operation_run_workload_seed(&run, "operation-runs");
   seed = random_derive_seed(seed, run.op->name);
   seed = random_derive_seed(seed, run.tree_vt->name);
   seed = random_derive_seed(seed, run.dist->name);
   seed = random_derive_seed(seed, "tree");

   random_seed(seed);
   return random_uint64();
}

static void test_operation_workload_seed_empty_domain_child(void) {
   operation_t op;
   operation_run_t run = test_operation_workload_seed_run(&op);
   (void)operation_run_workload_seed(&run, "");
}

static void test_operation_workload_seed_null_distribution_child(void) {
   operation_t op;
   operation_run_t run = test_operation_workload_seed_run(&op);
   run.dist = NULL;
   (void)operation_run_workload_seed(&run, "test");
}

static void test_operation_workload_seed_zero_iteration_child(void) {
   operation_t op;
   operation_run_t run = test_operation_workload_seed_run(&op);
   run.iteration = 0;
   (void)operation_run_workload_seed(&run, "test");
}

static void test_operation_workload_seed_late_iteration_child(void) {
   operation_t op;
   operation_run_t run = test_operation_workload_seed_run(&op);
   run.iteration = 2;
   (void)operation_run_workload_seed(&run, "test");
}

static void test_path_temporary_attempt(char *buf, size_t size,
                                        const char *path, pid_t pid,
                                        unsigned int attempt) {
   char *tmp_path = path_temporary_attempt_alloc(path, pid, attempt);
   path_format(buf, size, "%s", tmp_path);
   free(tmp_path);
}

static void assert_temporary_path_absent_for_pid(const char *path, pid_t pid) {
   invariant(path != NULL);
   invariant(pid > 0);
   char tmp_path[512];
   test_path_temporary_attempt(tmp_path, sizeof(tmp_path), path, pid, 0);
   assert_path_absent(tmp_path);
}

static void assert_operation_run_temp_absent_for_pid(const char *path,
                                                     pid_t pid) {
   assert_temporary_path_absent_for_pid(path, pid);
}

static void assert_operation_run_temp_absent(const char *path) {
   assert_operation_run_temp_absent_for_pid(path, getpid());
}

static void TestOperations(void) {
   printf("Operations:\n");

   assert_equal(1, sample_position(10, 1, 10));
   assert_equal(5, sample_position(10, 5, 10));
   assert_equal(10, sample_position(10, 10, 10));
   assert_equal(3, sample_position(10, 1, 3));
   assert_equal(6, sample_position(10, 2, 3));
   assert_equal(10, sample_position(10, 3, 3));
   assert_equal(UINT64_MAX / 3, sample_position(UINT64_MAX, 1, 3));
   assert_equal((UINT64_MAX / 3) * 2, sample_position(UINT64_MAX, 2, 3));
   assert_equal(UINT64_MAX, sample_position(UINT64_MAX, 3, 3));
   assert_equal(1, sample_position(UINT64_MAX, 1, UINT64_MAX));
   assert_equal(UINT64_MAX - 1,
                sample_position(UINT64_MAX, UINT64_MAX - 1, UINT64_MAX));
   assert_equal(UINT64_MAX,
                sample_position(UINT64_MAX, UINT64_MAX, UINT64_MAX));
   assert_equal(333, sample_scale_floor(1, 1000, 3));
   assert_equal(666, sample_scale_floor(2, 1000, 3));
   assert_equal(UINT64_MAX - 1,
                sample_scale_floor(UINT64_MAX - 1, UINT64_MAX, UINT64_MAX));

   sample_schedule_t schedule = sample_schedule_make(0, 0);
   assert_equal(1, schedule.total);
   assert_equal(1, schedule.samples);
   schedule = sample_schedule_make(0, 10);
   assert_equal(1, schedule.total);
   assert_equal(1, schedule.samples);
   schedule = sample_schedule_make(3, 10);
   assert_equal(3, schedule.total);
   assert_equal(3, schedule.samples);
   schedule = sample_schedule_make(UINT64_MAX, 0);
   assert_equal(UINT64_MAX, schedule.total);
   assert_equal(1, schedule.samples);
   assert_equal(UINT64_MAX, sample_schedule_position(schedule, 1));
   schedule = sample_schedule_make(UINT64_MAX, UINT64_MAX);
   sample_schedule_require(schedule);
   assert_equal(UINT64_MAX, schedule.total);
   assert_equal(UINT64_MAX, schedule.samples);
   assert_equal(1, sample_schedule_position(schedule, 1));
   assert_equal(UINT64_MAX, sample_schedule_position(schedule, UINT64_MAX));
   sample_schedule_point_t final_point =
       sample_schedule_point(schedule, UINT64_MAX - 1);
   assert_equal(UINT64_MAX, final_point.ordinal);
   assert_equal(UINT64_MAX, final_point.position);

   schedule = sample_schedule_make(1000, 7);
   const u64 expected_sample_positions[] = {
       142, 285, 428, 571, 714, 857, 1000,
   };
   const u64 expected_sample_position_count =
       sizeof(expected_sample_positions) / sizeof(expected_sample_positions[0]);
   for (u64 i = 0; i < expected_sample_position_count; i++) {
      sample_schedule_point_t point = sample_schedule_point(schedule, i);
      assert_equal(size_after_inserting_one(i), point.ordinal);
      assert_equal(expected_sample_positions[i], point.position);
   }
   assert_equal(142, sample_schedule_next_point(schedule, 0, 0).position);
   assert_equal(285, sample_schedule_next_point(schedule, 1, 142).position);

   assert_sample_schedule(1, 1);
   assert_sample_schedule(10, 10);
   assert_sample_schedule(1000, 7);
   assert_sample_schedule(UINT64_MAX, 1024);
   assert_equal(3, test_infer_sample_schedule_from_first_position(10, 3));
   assert_equal(2, test_infer_sample_schedule_from_first_position(10, 5));
   assert_equal(7, test_infer_sample_schedule_from_first_position(1000, 142));
   assert_equal(0, test_infer_sample_schedule_from_first_position(10, 1));
   assert_equal(0, test_infer_sample_schedule_from_first_position(10, 2));
   assert_equal(0, test_infer_sample_schedule_from_first_position(10, 4));
   assert_equal(0,
                test_infer_sample_schedule_from_first_position(UINT64_MAX, 1));
   assert_equal(4, test_infer_sample_schedule_from_position(10, 4, 10));
   assert_equal(
       10000, test_infer_sample_schedule_from_position(10000000, 249, 249000));
   assert_equal(3, test_infer_sample_schedule_from_first_position(
                       UINT64_MAX, UINT64_MAX / 3));

   sample_schedule_t operation_schedule = operation_run_sample_schedule(0, 0);
   assert_equal(1, operation_schedule.total);
   assert_equal(1, operation_schedule.samples);
   operation_schedule = operation_run_sample_schedule(5, 100);
   assert_equal(5, operation_schedule.total);
   assert_equal(5, operation_schedule.samples);
   assert_void_child_exits_2(
       test_operation_run_sample_schedule_too_large_child);
   assert_equal(1, operation_run_next_iteration(0));
   assert_equal(UINT64_MAX, operation_run_next_iteration(UINT64_MAX - 1));
   assert_void_child_exits_2_with_stderr(
       test_operation_run_next_iteration_overflow_child,
       "operation iteration count is too large");

   assert_aborts(test_sample_position_zero_total_child);
   assert_aborts(test_sample_position_too_many_samples_child);
   assert_aborts(test_sample_scale_floor_late_numerator_child);
   assert_aborts(test_sample_scale_floor_large_denominator_child);
   assert_aborts(test_sample_position_zero_sample_count_child);
   assert_aborts(test_sample_position_zero_sample_ordinal_child);
   assert_aborts(test_sample_position_late_sample_child);
   assert_aborts(test_sample_schedule_require_bad_schedule_child);
   assert_aborts(test_sample_schedule_require_zero_total_child);
   assert_aborts(test_sample_schedule_require_zero_samples_child);
   assert_aborts(test_sample_schedule_position_bad_schedule_child);
   assert_aborts(test_sample_schedule_position_late_sample_child);
   assert_aborts(test_sample_schedule_position_zero_sample_child);
   assert_aborts(test_sample_schedule_point_late_index_child);
   assert_aborts(test_sample_schedule_next_point_nonmonotonic_child);
   printf("  sample schedule boundaries OK\n");

   assert_equal(1, nonzero_duration_ns(0));
   assert_equal(1, nonzero_duration_ns(1));
   assert_equal(UINT64_MAX, nonzero_duration_ns(UINT64_MAX));
   invariant(!duration_sum_would_overflow(0, 0));
   invariant(!duration_sum_would_overflow(1, 2));
   invariant(!duration_sum_would_overflow(UINT64_MAX, 0));
   invariant(duration_sum_would_overflow(UINT64_MAX, 1));
   invariant(duration_sum_would_overflow(UINT64_MAX - 1, 2));
   assert_equal(0, duration_sum(0, 0));
   assert_equal(3, duration_sum(1, 2));
   assert_equal(UINT64_MAX, duration_sum(UINT64_MAX, 0));
   assert_equal(0, duration_ns_from_timespec((struct timespec){0}));
   assert_equal(1, duration_ns_from_timespec((struct timespec){
                       .tv_sec = 0,
                       .tv_nsec = 1,
                   }));
   assert_equal(size_sum(TIME_NS_PER_SECOND, 2),
                duration_ns_from_timespec((struct timespec){
                    .tv_sec = 1,
                    .tv_nsec = 2,
                }));
   u64 max_timespec_seconds = UINT64_MAX / TIME_NS_PER_SECOND;
   u64 max_timespec_nanoseconds = UINT64_MAX % TIME_NS_PER_SECOND;
   assert_equal(UINT64_MAX, duration_ns_from_timespec((struct timespec){
                                .tv_sec = (time_t)max_timespec_seconds,
                                .tv_nsec = (long)max_timespec_nanoseconds,
                            }));
   assert_equal(1, duration_average_nonzero(1, 1));
   assert_equal(3, duration_average_nonzero(10, 3));
   assert_equal(UINT64_MAX, duration_average_nonzero(UINT64_MAX, 1));
   invariant(!duration_reached(0, 1));
   invariant(duration_reached(1, 1));
   invariant(duration_reached(2, 1));
   invariant(duration_reached(UINT64_MAX, UINT64_MAX));
   assert_aborts(test_duration_sum_overflow_child);
   assert_aborts(test_duration_timespec_negative_seconds_child);
   assert_aborts(test_duration_timespec_negative_nanoseconds_child);
   assert_aborts(test_duration_timespec_large_nanoseconds_child);
   assert_aborts(test_duration_timespec_overflow_child);
   assert_aborts(test_duration_average_zero_count_child);
   assert_aborts(test_duration_average_zero_average_child);
   assert_aborts(test_duration_reached_zero_duration_child);
   u64 timing_start = monotonic_time_ns();
   u64 elapsed = elapsed_time_ns_since(timing_start);
   invariant(elapsed <= elapsed_time_ns_since(timing_start));
   invariant(elapsed_time_ns_since_nonzero(timing_start) > 0);
   invariant(!elapsed_time_ns_reached(timing_start, UINT64_MAX));
   assert_aborts(test_elapsed_time_ns_since_future_child);
   assert_aborts(test_elapsed_time_ns_reached_zero_duration_child);
   char timebuf[64];
   format_local_time_or_exit(timebuf, sizeof(timebuf), "%H:%M:%S");
   invariant(timebuf[0] != '\0');
   assert_aborts(test_format_local_time_null_buffer_child);
   assert_aborts(test_format_local_time_zero_size_child);
   assert_aborts(test_format_local_time_null_format_child);
   assert_aborts(test_format_local_time_empty_format_child);
   assert_void_child_exits_2(test_format_local_time_tiny_buffer_child);
   printf("  benchmark timing helpers OK\n");

   assert_equal(1, operation_effective_scale_value(0));
   assert_equal(7, operation_effective_scale_value(7));
   invariant(operation_insert_delete_should_insert(1, 0, 1));
   invariant(!operation_insert_delete_should_insert(1, 1, 2));
   invariant(!operation_insert_delete_should_insert(0, 1, 2));
   invariant(operation_insert_delete_should_insert(2, 1, 4));
   invariant(!operation_insert_delete_should_insert(2, 2, 3));
   invariant(!operation_insert_delete_should_insert(3, 3, 4));
   invariant(operation_insert_delete_should_insert(3, 2, 5));
   assert_equal(UINT64_MAX, operation_effective_scale_value(UINT64_MAX));
   invariant(operation_insert_delete_should_insert(UINT64_MAX, UINT64_MAX - 1,
                                                   UINT64_MAX - 1));
   invariant(!operation_insert_delete_should_insert(UINT64_MAX, UINT64_MAX,
                                                    UINT64_MAX));
   invariant(!operation_insert_delete_should_insert(
       UINT64_MAX - 1, UINT64_MAX - 1, UINT64_MAX));
   assert_aborts(test_operation_insert_delete_overfull_child);
   assert_aborts(test_operation_insert_delete_zero_step_child);
   assert_aborts(test_operation_insert_delete_size_ahead_child);
   assert_aborts(test_operation_step_count_overflow_child);
   assert_aborts(test_operation_step_after_declared_steps_child);
   printf("  insert/delete schedule helper OK\n");

   operation_t operations[operation_default_count];
   assert_int_equal(operation_default_count, operation_defaults(operations));
   operation_require(&operations[0]);
   operation_require(&operations[1]);
   operation_t selected_operations[operation_default_count];
   assert_int_equal(operation_default_count,
                    operation_defaults_for_name(selected_operations, NULL));
   invariant(strcmp(selected_operations[0].name, operations[0].name) == 0);
   invariant(strcmp(selected_operations[1].name, operations[1].name) == 0);
   assert_int_equal(
       1, operation_defaults_for_name(selected_operations, operations[1].name));
   invariant(strcmp(selected_operations[0].name, operations[1].name) == 0);
   assert_equal(operations[1].scale, selected_operations[0].scale);
   assert_equal(operations[1].steps, selected_operations[0].steps);
   operation_t mixed_operation = operation_default_mixed();
   operation_require(&mixed_operation);
   invariant(!operation_inserts_only(&mixed_operation));
   u64 insert_only_operations = 0;
   u64 mixed_operations = 0;
   for (int i = 0; i < operation_default_count; i++) {
      invariant(operation_name_is_component(operations[i].name));
      if (operation_inserts_only(&operations[i])) {
         insert_only_operations++;
      } else {
         mixed_operations++;
      }
   }
   assert_equal(1, insert_only_operations);
   assert_equal(1, mixed_operations);
   invariant(operation_find(operations, operation_default_count,
                            operations[0].name) == &operations[0]);
   invariant(operation_find(operations, operation_default_count,
                            operations[1].name) == &operations[1]);
   invariant(operation_find_or_exit(operations, operation_default_count,
                                    operations[0].name) == &operations[0]);
   invariant(operation_find(operations, operation_default_count,
                            "NotAnOperation") == NULL);
   invariant(operation_find(operations, operation_default_count, "Bad/Name") ==
             NULL);
   invariant(operation_find(operations, operation_default_count, "Bad\\Name") ==
             NULL);
   invariant(!operation_name_is_component(NULL));
   invariant(!operation_name_is_component(""));
   invariant(!operation_name_is_component("."));
   invariant(!operation_name_is_component(".."));
   invariant(!operation_name_is_component("Bad/Name"));
   invariant(!operation_name_is_component("Bad\\Name"));
   invariant(!operation_name_is_component("Bad Name"));
   invariant(!operation_name_is_component("Bad\200Name"));
   invariant(operation_find(operations, operation_default_count, NULL) == NULL);
   invariant(operation_find(operations, operation_default_count, "") == NULL);
   invariant(operation_find(NULL, 0, "NotAnOperation") == NULL);
   invariant(operation_find(NULL, 0, NULL) == NULL);
   assert_aborts(test_operation_find_negative_count_child);
   assert_aborts(test_operation_find_or_exit_negative_count_child);
   assert_aborts(test_operation_fprint_names_negative_count_child);
   assert_void_child_exits_2_with_stderr(
       test_operation_find_or_exit_missing_child,
       "unknown operation for this build: NotAnOperation");
   assert_void_child_exits_2_with_stderr(test_operation_find_or_exit_null_child,
                                         "operation name is null");
   assert_void_child_exits_2_with_stderr(
       test_operation_find_or_exit_empty_child, "operation name is empty");
   assert_void_child_exits_2_with_stderr(
       test_operation_find_or_exit_bad_name_child,
       "operation name is not a path component");
   char operation_names_line[128];
   FILE *operation_names = tmpfile();
   invariant(operation_names != NULL);
   operation_fprint_names(operation_names, "operations", NULL, 0);
   rewind(operation_names);
   test_expect_file_line(operation_names, "operation names",
                         operation_names_line, sizeof(operation_names_line),
                         "operations:\n");
   test_expect_file_eof(operation_names, "operation names",
                        operation_names_line, sizeof(operation_names_line));
   invariant(fclose(operation_names) == 0);

   operation_names = tmpfile();
   invariant(operation_names != NULL);
   operation_fprint_names(operation_names, "operations", operations,
                          operation_default_count);
   rewind(operation_names);
#ifdef PERSISTENT
   test_expect_file_line(
       operation_names, "operation names", operation_names_line,
       sizeof(operation_names_line),
       "operations: InsertPersistent InsertDeletePersistent\n");
#else
   test_expect_file_line(operation_names, "operation names",
                         operation_names_line, sizeof(operation_names_line),
                         "operations: Insert InsertDelete\n");
#endif
   test_expect_file_eof(operation_names, "operation names",
                        operation_names_line, sizeof(operation_names_line));
   invariant(fclose(operation_names) == 0);
   invariant(operation_inserts_only(&operations[0]));
   invariant(!operation_inserts_only(&operations[1]));
   invariant(strcmp(mixed_operation.name, operations[1].name) == 0);
   assert_equal(operations[1].scale, mixed_operation.scale);
   assert_equal(operations[1].steps, mixed_operation.steps);
   assert_aborts(test_operation_duplicate_name_child);
   printf("  operation lookup OK\n");

   operation_t op = operations[1];
   op.scale = 0;
   op.steps = 5;

   list_t instance = list_new(&reference_vtable);
   distribution_t *dist = uniform_new(1);
   u64 step_count = 0;

   for (u64 i = 0; i < op.steps; i++) {
      instance = operation_update(&op, instance, dist, &step_count);
      list_verify(&instance);
   }

   assert_equal(op.steps, step_count);
   assert_equal(1, list_size(&instance));

   list_free(&instance);
   dist_destroy(dist);
   printf("  scale-zero insert/delete OK\n");

   op = operations[1];
   op.scale = 2;
   op.steps = 4;

   instance = list_new(&tracking_vtable);
   dist = maximum_new(1);
   step_count = 0;

   for (u64 i = 0; i < op.steps; i++) {
      instance = operation_update(&op, instance, dist, &step_count);
      list_verify(&instance);
   }

   tracking_list_t *tracked = tracking_list_impl(&instance);
   assert_equal(4, step_count);
   assert_equal(2, tracked->size);
   assert_equal(3, tracked->insert_count);
   assert_equal(1, tracked->delete_count);
   assert_equal(0, tracked->inserts[0]);
   assert_equal(1, tracked->inserts[1]);
   assert_equal(0, tracked->deletes[0]);
   assert_equal(1, tracked->inserts[2]);

   list_free(&instance);
   dist_destroy(dist);
   printf("  maximum insert/delete positions OK\n");

   op = operations[1];
   op.scale = 2;
   op.steps = 4;

   instance = list_new(&tracking_vtable);
   dist = minimum_new(1);
   step_count = 0;

   for (u64 i = 0; i < op.steps; i++) {
      instance = operation_update(&op, instance, dist, &step_count);
      list_verify(&instance);
   }

   tracked = tracking_list_impl(&instance);
   assert_equal(4, step_count);
   assert_equal(2, tracked->size);
   assert_equal(3, tracked->insert_count);
   assert_equal(1, tracked->delete_count);
   assert_equal(0, tracked->inserts[0]);
   assert_equal(0, tracked->inserts[1]);
   assert_equal(1, tracked->deletes[0]);
   assert_equal(0, tracked->inserts[2]);

   list_free(&instance);
   dist_destroy(dist);
   printf("  minimum insert/delete positions OK\n");

   op = operations[1];
   op.scale = 2;
   op.steps = 4;

   instance = list_new(&tracking_vtable);
   dist = maximum_new(1);
   step_count = 0;

   operation_step_t step = operation_step(&op, instance, dist, &step_count);
   instance = step.instance;
   assert_operation_action_equal(OPERATION_ACTION_INSERT, step.action);
   assert_equal(0, step.position);
   assert_equal(0, step.size_before);
   assert_equal(1, step.size_after);
   assert_equal(1, step.step_count);
   assert_equal(1, step_count);

   step = operation_step(&op, instance, dist, &step_count);
   instance = step.instance;
   assert_operation_action_equal(OPERATION_ACTION_INSERT, step.action);
   assert_equal(1, step.position);
   assert_equal(1, step.size_before);
   assert_equal(2, step.size_after);
   assert_equal(2, step.step_count);
   assert_equal(2, step_count);

   step = operation_step(&op, instance, dist, &step_count);
   instance = step.instance;
   assert_operation_action_equal(OPERATION_ACTION_DELETE, step.action);
   assert_equal(0, step.position);
   assert_equal(2, step.size_before);
   assert_equal(1, step.size_after);
   assert_equal(3, step.step_count);
   assert_equal(3, step_count);

   list_free(&instance);
   dist_destroy(dist);

   instance = list_new(&tracking_vtable);
   dist = minimum_new(1);
   step_count = 0;

   step = operation_step(&op, instance, dist, &step_count);
   instance = step.instance;
   assert_operation_action_equal(OPERATION_ACTION_INSERT, step.action);
   assert_equal(0, step.position);
   assert_equal(0, step.size_before);
   assert_equal(1, step.size_after);
   assert_equal(1, step.step_count);
   assert_equal(1, step_count);

   step = operation_step(&op, instance, dist, &step_count);
   instance = step.instance;
   assert_operation_action_equal(OPERATION_ACTION_INSERT, step.action);
   assert_equal(0, step.position);
   assert_equal(1, step.size_before);
   assert_equal(2, step.size_after);
   assert_equal(2, step.step_count);
   assert_equal(2, step_count);

   step = operation_step(&op, instance, dist, &step_count);
   instance = step.instance;
   assert_operation_action_equal(OPERATION_ACTION_DELETE, step.action);
   assert_equal(1, step.position);
   assert_equal(2, step.size_before);
   assert_equal(1, step.size_after);
   assert_equal(3, step.step_count);
   assert_equal(3, step_count);

   list_free(&instance);
   dist_destroy(dist);
   printf("  operation step result contract OK\n");

   op = operations[1];
   op.scale = 2;
   op.steps = 6;

   static const struct {
      operation_action_t action;
      u64 position;
      u64 size_before;
      u64 size_after;
   } expected_steps[] = {
       {OPERATION_ACTION_INSERT, 0, 0, 1}, {OPERATION_ACTION_INSERT, 0, 1, 2},
       {OPERATION_ACTION_DELETE, 0, 2, 1}, {OPERATION_ACTION_INSERT, 0, 1, 2},
       {OPERATION_ACTION_DELETE, 0, 2, 1}, {OPERATION_ACTION_INSERT, 0, 1, 2},
   };

   instance = list_new(&conc_vtable);
   list_t ref_instance = list_new(&reference_vtable);
   distribution_t *instance_dist = queue_new(1);
   distribution_t *ref_dist = queue_new(1);
   step_count = 0;
   u64 ref_step_count = 0;

   for (u64 i = 0; i < sizeof(expected_steps) / sizeof(expected_steps[0]);
        i++) {
      step = operation_step(&op, instance, instance_dist, &step_count);
      instance = step.instance;
      operation_step_t ref_step =
          operation_step(&op, ref_instance, ref_dist, &ref_step_count);
      ref_instance = ref_step.instance;

      assert_operation_action_equal(expected_steps[i].action, step.action);
      assert_equal(expected_steps[i].position, step.position);
      assert_equal(expected_steps[i].size_before, step.size_before);
      assert_equal(expected_steps[i].size_after, step.size_after);
      assert_equal(i + 1, step.step_count);
      assert_equal(step.step_count, step_count);

      assert_operation_action_equal(step.action, ref_step.action);
      assert_equal(step.position, ref_step.position);
      assert_equal(step.size_before, ref_step.size_before);
      assert_equal(step.size_after, ref_step.size_after);
      assert_equal(step.step_count, ref_step.step_count);
      assert_equal(step_count, ref_step_count);
      list_verify(&instance);
      list_verify(&ref_instance);
      assert_list(&ref_instance, &instance);
   }

   assert_equal(op.steps, step_count);
   assert_equal(op.steps, ref_step_count);
   assert_equal(2, list_size(&instance));
   assert_list(&ref_instance, &instance);

   list_free(&instance);
   list_free(&ref_instance);
   dist_destroy(instance_dist);
   dist_destroy(ref_dist);
   printf("  operation step reference sequence OK\n");

   op = operations[1];
   op.scale = 1;
   op.steps = 4;

   instance = list_new(&tracking_vtable);
   dist = maximum_new(1);
   step_count = 0;

   for (u64 i = 0; i < op.steps; i++) {
      instance = operation_update(&op, instance, dist, &step_count);
      list_verify(&instance);
   }

   tracked = tracking_list_impl(&instance);
   assert_equal(4, step_count);
   assert_equal(0, tracked->size);
   assert_equal(2, tracked->insert_count);
   assert_equal(2, tracked->delete_count);
   assert_equal(0, tracked->inserts[0]);
   assert_equal(0, tracked->deletes[0]);
   assert_equal(0, tracked->inserts[1]);
   assert_equal(0, tracked->deletes[1]);

   list_free(&instance);
   dist_destroy(dist);
   printf("  unit-scale insert/delete alternation OK\n");

   op = operations[1];
   op.scale = 3;
   op.steps = 6;

   instance = list_new(&tracking_vtable);
   dist = maximum_new(1);
   step_count = 0;

   for (u64 i = 0; i < op.steps; i++) {
      instance = operation_update(&op, instance, dist, &step_count);
      list_verify(&instance);
   }

   tracked = tracking_list_impl(&instance);
   assert_equal(6, step_count);
   assert_equal(2, tracked->size);
   assert_equal(4, tracked->insert_count);
   assert_equal(2, tracked->delete_count);
   assert_equal(0, tracked->inserts[0]);
   assert_equal(1, tracked->inserts[1]);
   assert_equal(2, tracked->inserts[2]);
   assert_equal(0, tracked->deletes[0]);
   assert_equal(2, tracked->inserts[3]);
   assert_equal(0, tracked->deletes[1]);

   list_free(&instance);
   dist_destroy(dist);
   printf("  odd-scale insert/delete alternation OK\n");

   operation_t runner_op = operations[0];
   runner_op.scale = 0;
   runner_op.steps = 0;

   const list_vtable_t *trees[] = {&reference_vtable};
   const dist_spec_t *dists[] = {dist_find("Uniform")};
   invariant(dists[0] != NULL);

   operation_t seed_op = operations[0];
   seed_op.scale = 1;
   seed_op.steps = 1;
   operation_run_t seed_run = {
       .op = &seed_op,
       .tree_vt = &reference_vtable,
       .dist = dists[0],
       .scale = seed_op.scale,
       .steps = seed_op.steps,
       .iteration = 2,
       .iterations = 3,
   };
   u64 expected_seed = random_derive_seed(0, "operation-runs");
   expected_seed = random_derive_seed(expected_seed, seed_op.name);
   expected_seed = random_derive_seed(expected_seed, "scale");
   expected_seed = random_derive_u64(expected_seed, seed_run.scale);
   expected_seed = random_derive_seed(expected_seed, "steps");
   expected_seed = random_derive_u64(expected_seed, seed_run.steps);
   expected_seed = random_derive_seed(expected_seed, dists[0]->name);
   expected_seed = random_derive_seed(expected_seed, "iteration");
   expected_seed = random_derive_u64(expected_seed, seed_run.iteration);
   u64 reference_seed =
       operation_run_workload_seed(&seed_run, "operation-runs");
   assert_equal(expected_seed, reference_seed);
   seed_run.tree_vt = &tracking_vtable;
   assert_equal(reference_seed,
                operation_run_workload_seed(&seed_run, "operation-runs"));
   invariant(reference_seed !=
             operation_run_workload_seed(&seed_run, "operation-benchmarks"));
   const dist_spec_t *alternate_dist = dist_find("Maximum");
   invariant(alternate_dist != NULL);
   operation_run_t varied_seed_run = seed_run;
   varied_seed_run.dist = alternate_dist;
   invariant(reference_seed !=
             operation_run_workload_seed(&varied_seed_run, "operation-runs"));
   operation_t alternate_op = operations[1];
   alternate_op.scale = seed_op.scale;
   alternate_op.steps = seed_op.steps;
   varied_seed_run = seed_run;
   varied_seed_run.op = &alternate_op;
   invariant(reference_seed !=
             operation_run_workload_seed(&varied_seed_run, "operation-runs"));
   operation_t scaled_op = seed_op;
   scaled_op.scale = 2;
   varied_seed_run = seed_run;
   varied_seed_run.op = &scaled_op;
   varied_seed_run.scale = scaled_op.scale;
   invariant(reference_seed !=
             operation_run_workload_seed(&varied_seed_run, "operation-runs"));
   operation_t stepped_op = seed_op;
   stepped_op.steps = 2;
   varied_seed_run = seed_run;
   varied_seed_run.op = &stepped_op;
   varied_seed_run.steps = stepped_op.steps;
   invariant(reference_seed !=
             operation_run_workload_seed(&varied_seed_run, "operation-runs"));
   varied_seed_run = seed_run;
   varied_seed_run.iteration = 3;
   invariant(reference_seed !=
             operation_run_workload_seed(&varied_seed_run, "operation-runs"));
   random_seed(reference_seed);
   u64 expected_next = random_uint64();
   random_seed(reference_seed);
   (void)operation_run_workload_seed(&seed_run, "operation-runs");
   assert_equal(expected_next, random_uint64());
   assert_aborts(test_operation_workload_seed_empty_domain_child);
   assert_aborts(test_operation_workload_seed_null_distribution_child);
   assert_aborts(test_operation_workload_seed_zero_iteration_child);
   assert_aborts(test_operation_workload_seed_late_iteration_child);
   printf("  operation workload seeds OK\n");

   operation_t rng_op = operations[0];
   rng_op.scale = 1;
   rng_op.steps = 1;
   const list_vtable_t *rng_probe_trees[] = {
       &rng_probe_a_vtable,
       &rng_probe_b_vtable,
   };
   const dist_spec_t *rng_recording_dists[] = {
       &test_recording_dist_spec,
   };
   rng_probe_record_count = 0;
   test_operation_run_recorded_seed_count = 0;

   char rng_output_dir[] = "/tmp/bst-operation-tree-rng-XXXXXX";
   char *rng_root = mkdtemp(rng_output_dir);
   invariant(rng_root != NULL);
   operation_run_config_t rng_config = {
       .samples = 1,
       .arena_cap = 0,
       .iterations = 1,
       .output_dir = rng_root,
       .kind = "test",
       .trees = rng_probe_trees,
       .tree_count = 2,
       .distributions = rng_recording_dists,
       .distribution_count = 1,
   };
   operation_run_matrix(&rng_op, &rng_config, NULL, NULL, NULL, NULL);

   assert_equal(2, rng_probe_record_count);
   assert_equal(2, test_operation_run_recorded_seed_count);
   assert_equal(test_operation_run_recorded_seeds[0],
                test_operation_run_recorded_seeds[1]);

   operation_run_t rng_run = {
       .op = &rng_op,
       .dist = rng_recording_dists[0],
       .scale = rng_op.scale,
       .steps = rng_op.steps,
       .iteration = 1,
       .iterations = 1,
   };
   for (u64 i = 0; i < rng_probe_record_count; i++) {
      rng_run.tree_vt = rng_probe_trees[i];
      invariant(strcmp(rng_probe_records[i].tree_name,
                       rng_probe_trees[i]->name) == 0);
      assert_equal(test_operation_tree_rng_first_value(rng_run),
                   rng_probe_records[i].value);
   }
   invariant(rng_probe_records[0].value != rng_probe_records[1].value);

   char rng_operations_dir[512];
   char rng_kind_dir[512];
   char rng_op_dir[512];
   char rng_file[512];
   path_format(rng_operations_dir, sizeof(rng_operations_dir), "%s/operations",
               rng_root);
   path_format(rng_kind_dir, sizeof(rng_kind_dir), "%s/test",
               rng_operations_dir);
   path_format(rng_op_dir, sizeof(rng_op_dir), "%s/%s", rng_kind_dir,
               rng_op.name);
   for (u64 i = 0; i < rng_probe_record_count; i++) {
      path_format(rng_file, sizeof(rng_file), "%s/%s", rng_op_dir,
                  rng_probe_trees[i]->name);
      invariant(remove(rng_file) == 0);
   }
   invariant(rmdir(rng_op_dir) == 0);
   invariant(rmdir(rng_kind_dir) == 0);
   invariant(rmdir(rng_operations_dir) == 0);
   invariant(rmdir(rng_root) == 0);
   printf("  runner tree RNG seeds OK\n");

   char output_dir[] = "/tmp/bst-operation-run-XXXXXX";
   char *created_dir = mkdtemp(output_dir);
   invariant(created_dir != NULL);
   char output_file[512];
   char tmp_output_file[512];

   operation_t child_op = operations[0];
   child_op.scale = 1;
   child_op.steps = 1;
   operation_run_config_t bad_config = {
       .samples = 1,
       .arena_cap = 1,
       .iterations = 1,
       .output_dir = "",
       .kind = "test",
       .trees = trees,
       .tree_count = 1,
       .distributions = dists,
       .distribution_count = 1,
   };
   assert_operation_run_config_exits_2(child_op, bad_config);

   bad_config.output_dir = created_dir;
   bad_config.kind = NULL;
   assert_operation_run_config_exits_2(child_op, bad_config);

   bad_config.kind = "";
   assert_operation_run_config_exits_2(child_op, bad_config);

   bad_config.kind = "../escaped";
   assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config, "operation output kind is not a path component");
   char escaped_kind_dir[512];
   path_format(escaped_kind_dir, sizeof(escaped_kind_dir), "%s/escaped",
               created_dir);
   assert_path_absent(escaped_kind_dir);

   bad_config.kind = "Bad\200Kind";
   assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config, "operation output kind is not a path component");

   bad_config.kind = "test";
   bad_config.header = "";
   assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config, "operation output header is empty");

   bad_config.header = "Bad\nHeader";
   assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config, "operation output header contains newline");

   bad_config.header = NULL;
   bad_config.trees = NULL;
   assert_operation_run_config_exits_2(child_op, bad_config);

   bad_config.trees = trees;
   bad_config.tree_count = 0;
   assert_operation_run_config_exits_2(child_op, bad_config);

   bad_config.tree_count = -1;
   assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config, "operation tree count is negative");

   bad_config.tree_count = 1;
   bad_config.distributions = NULL;
   assert_operation_run_config_exits_2(child_op, bad_config);

   bad_config.distributions = dists;
   bad_config.distribution_count = 0;
   assert_operation_run_config_exits_2(child_op, bad_config);

   bad_config.distribution_count = -1;
   assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config, "operation distribution count is negative");

   bad_config.distribution_count = 1;
   child_op.steps = UINT64_MAX;
   assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config, "operation step count is too large");

   child_op.steps = 1;
   bad_config.iterations = UINT64_MAX;
   assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config, "operation iteration count is too large");

   bad_config.iterations = 1;
   if (arena_max_capacity() < UINT64_MAX) {
      bad_config.arena_cap = arena_max_capacity() + 1;
      assert_operation_run_config_exits_2_with_stderr(
          child_op, bad_config, "operation arena capacity is too large");
      bad_config.arena_cap = 1;
   }

   bad_config.path = test_operation_run_empty_path;
   assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config, "operation output path is empty");

   bad_config.path = test_operation_run_empty_dir;
   assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config, "operation output directory is empty");

   bad_config.path = test_operation_run_unterminated_path;
   assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config, "operation path buffer is not terminated");

   bad_config.path = test_operation_run_unterminated_dir;
   assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config, "operation path buffer is not terminated");

   bad_config.path = test_operation_run_wrong_parent_dir;
   pid_t wrong_parent_pid = assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config, "operation output directory is not path parent");
   char wrong_parent_dir[512];
   char wrong_parent_file[512];
   path_format(wrong_parent_dir, sizeof(wrong_parent_dir), "%s/missing-parent",
               created_dir);
   path_format(wrong_parent_file, sizeof(wrong_parent_file), "%s/run.txt",
               wrong_parent_dir);
   assert_path_absent(wrong_parent_dir);
   assert_path_absent(wrong_parent_file);
   assert_operation_run_temp_absent_for_pid(wrong_parent_file,
                                            wrong_parent_pid);
   printf("  runner output path validation OK\n");

   bad_config.path = NULL;
   list_vtable_t escaped_tree_vtable = reference_vtable;
   escaped_tree_vtable.name = "../Reference";
   const list_vtable_t *escaped_tree_names[] = {&escaped_tree_vtable};
   bad_config.trees = escaped_tree_names;
   pid_t escaped_tree_pid = assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config,
       "operation output tree name is not a path component");
   char escaped_tree_file[512];
   path_format(escaped_tree_file, sizeof(escaped_tree_file),
               "%s/operations/test/Reference", created_dir);
   assert_path_absent(escaped_tree_file);
   assert_operation_run_temp_absent_for_pid(escaped_tree_file,
                                            escaped_tree_pid);

   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   list_vtable_t high_byte_tree_vtable = reference_vtable;
   high_byte_tree_vtable.name = "Bad\200Tree";
   const list_vtable_t *high_byte_tree_names[] = {&high_byte_tree_vtable};
   bad_config.trees = high_byte_tree_names;
   pid_t high_byte_tree_pid = assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config,
       "operation output tree name is not a path component");
   assert_path_absent(output_file);
   assert_operation_run_temp_absent_for_pid(output_file, high_byte_tree_pid);

   bad_config.path = test_operation_run_path;
   const list_vtable_t *bad_trees[] = {NULL};
   bad_config.trees = bad_trees;
   pid_t bad_tree_pid = assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config, "operation tree is null");
   assert_path_absent(output_file);
   assert_operation_run_temp_absent_for_pid(output_file, bad_tree_pid);

   const list_vtable_t *duplicate_trees[] = {
       &reference_vtable,
       &reference_vtable,
   };
   bad_config.trees = duplicate_trees;
   bad_config.tree_count = 2;
   pid_t duplicate_tree_pid = assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config, "duplicate operation tree: Reference");
   assert_path_absent(output_file);
   assert_operation_run_temp_absent_for_pid(output_file, duplicate_tree_pid);

   const dist_spec_t *bad_dists[] = {NULL};
   bad_config.trees = trees;
   bad_config.tree_count = 1;
   bad_config.distributions = bad_dists;
   pid_t bad_dist_pid = assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config, "operation distribution is null");
   assert_path_absent(output_file);
   assert_operation_run_temp_absent_for_pid(output_file, bad_dist_pid);

   dist_spec_t bad_dist_name = *dists[0];
   bad_dist_name.name = "Bad Name";
   const dist_spec_t *bad_dist_names[] = {&bad_dist_name};
   bad_config.distributions = bad_dist_names;
   pid_t bad_dist_name_pid = assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config,
       "operation distribution name is not a printable token");
   assert_path_absent(output_file);
   assert_operation_run_temp_absent_for_pid(output_file, bad_dist_name_pid);

   dist_spec_t high_byte_dist_name = *dists[0];
   high_byte_dist_name.name = "Bad\200Distribution";
   const dist_spec_t *high_byte_dist_names[] = {&high_byte_dist_name};
   bad_config.distributions = high_byte_dist_names;
   pid_t high_byte_dist_name_pid =
       assert_operation_run_config_exits_2_with_stderr(
           child_op, bad_config,
           "operation distribution name is not a printable token");
   assert_path_absent(output_file);
   assert_operation_run_temp_absent_for_pid(output_file,
                                            high_byte_dist_name_pid);

   const dist_spec_t *duplicate_dists[] = {dists[0], dists[0]};
   bad_config.distributions = duplicate_dists;
   bad_config.distribution_count = 2;
   pid_t duplicate_dist_pid = assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config, "duplicate operation distribution: Uniform");
   assert_path_absent(output_file);
   assert_operation_run_temp_absent_for_pid(output_file, duplicate_dist_pid);

   const list_vtable_t *duplicate_path_trees[] = {
       &reference_vtable,
       &tracking_vtable,
   };
   bad_config.trees = duplicate_path_trees;
   bad_config.tree_count = 2;
   bad_config.distributions = dists;
   bad_config.distribution_count = 1;
   bad_config.path = test_operation_run_path;
   pid_t duplicate_path_pid = assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config, "duplicate operation output path");
   assert_path_absent(output_file);
   assert_operation_run_temp_absent_for_pid(output_file, duplicate_path_pid);

   bad_config.path = test_operation_run_second_tree_empty_path;
   pid_t second_path_pid = assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config, "operation output path is empty");
   assert_path_absent(output_file);
   assert_operation_run_temp_absent_for_pid(output_file, second_path_pid);

   char preflight_dir[512];
   char preflight_file[512];
   path_format(preflight_dir, sizeof(preflight_dir), "%s/preflight-valid",
               created_dir);
   path_format(preflight_file, sizeof(preflight_file), "%s/run.txt",
               preflight_dir);
   bad_config.path = test_operation_run_second_tree_empty_dir;
   pid_t second_dir_pid = assert_operation_run_config_exits_2_with_stderr(
       child_op, bad_config, "operation output directory is empty");
   assert_path_absent(preflight_dir);
   assert_path_absent(preflight_file);
   assert_operation_run_temp_absent_for_pid(preflight_file, second_dir_pid);

   bad_config.trees = trees;
   bad_config.tree_count = 1;
   bad_config.distributions = dists;
   printf("  runner preflight validation OK\n");

   operation_run_config_t config = {
       .samples = 0,
       .arena_cap = 0,
       .iterations = 0,
       .output_dir = created_dir,
       .kind = "test",
       .header = "Header",
       .trees = trees,
       .tree_count = 1,
       .distributions = dists,
       .distribution_count = 1,
       .path = test_operation_run_path,
   };
   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   path_format(tmp_output_file, sizeof(tmp_output_file), "%s.tmp", output_file);
   FILE *file = fopen_or_exit(tmp_output_file, "w");
   file_puts_or_exit("stale", file, tmp_output_file);
   fclose_or_exit(file, tmp_output_file);
   operation_run_matrix(&runner_op, &config, NULL, NULL,
                        test_operation_run_after_chunk, NULL);

   file = fopen_or_exit(tmp_output_file, "r");
   char stale_data[6] = {0};
   file_read_or_exit(stale_data, 5, file, tmp_output_file);
   fclose_or_exit(file, tmp_output_file);
   invariant(strcmp(stale_data, "stale") == 0);

   file = fopen_or_exit(output_file, "r");
   char line[128];
   test_expect_file_line(file, output_file, line, sizeof(line), "Header\n");
   test_expect_file_line(file, output_file, line, sizeof(line), "1 1 1 1\n");
   test_expect_file_eof(file, output_file, line, sizeof(line));
   fclose_or_exit(file, output_file);
   assert_operation_run_temp_absent(output_file);
   invariant(remove(output_file) == 0);
   invariant(remove(tmp_output_file) == 0);
   invariant(rmdir(created_dir) == 0);
   printf("  runner stale generic temp ignored OK\n");

   char default_output_dir[] = "/tmp/bst-operation-run-XXXXXX";
   created_dir = mkdtemp(default_output_dir);
   invariant(created_dir != NULL);
   config.output_dir = created_dir;
   operation_run_matrix(&runner_op, &config, NULL, NULL,
                        test_operation_run_after_chunk, NULL);

   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   path_format(tmp_output_file, sizeof(tmp_output_file), "%s.tmp", output_file);
   file = fopen_or_exit(output_file, "r");
   test_expect_file_line(file, output_file, line, sizeof(line), "Header\n");
   test_expect_file_line(file, output_file, line, sizeof(line), "1 1 1 1\n");
   test_expect_file_eof(file, output_file, line, sizeof(line));
   fclose_or_exit(file, output_file);
   assert_path_absent(tmp_output_file);
   assert_operation_run_temp_absent(output_file);
   invariant(remove(output_file) == 0);
   invariant(rmdir(created_dir) == 0);
   printf("  zero-value runner defaults OK\n");

   char default_path_output_dir[] = "/tmp/bst-operation-default-path-XXXXXX";
   created_dir = mkdtemp(default_path_output_dir);
   invariant(created_dir != NULL);
   config.output_dir = created_dir;
   config.path = NULL;
   operation_run_matrix(&runner_op, &config, NULL, NULL,
                        test_operation_run_after_chunk, NULL);

   char default_kind_dir[512];
   char default_op_dir[512];
   path_format(default_kind_dir, sizeof(default_kind_dir), "%s/operations/test",
               created_dir);
   path_format(default_op_dir, sizeof(default_op_dir), "%s/%s",
               default_kind_dir, runner_op.name);
   path_format(output_file, sizeof(output_file), "%s/%s", default_op_dir,
               reference_vtable.name);
   path_format(tmp_output_file, sizeof(tmp_output_file), "%s.tmp", output_file);
   file = fopen_or_exit(output_file, "r");
   test_expect_file_line(file, output_file, line, sizeof(line), "Header\n");
   test_expect_file_line(file, output_file, line, sizeof(line), "1 1 1 1\n");
   test_expect_file_eof(file, output_file, line, sizeof(line));
   fclose_or_exit(file, output_file);
   assert_path_absent(tmp_output_file);
   assert_operation_run_temp_absent(output_file);
   invariant(remove(output_file) == 0);
   invariant(rmdir(default_op_dir) == 0);
   invariant(rmdir(default_kind_dir) == 0);
   path_format(default_kind_dir, sizeof(default_kind_dir), "%s/operations",
               created_dir);
   invariant(rmdir(default_kind_dir) == 0);
   invariant(rmdir(created_dir) == 0);
   config.path = test_operation_run_path;
   printf("  default runner output path OK\n");

   char multi_tree_output_dir[] = "/tmp/bst-operation-multi-tree-XXXXXX";
   created_dir = mkdtemp(multi_tree_output_dir);
   invariant(created_dir != NULL);

   const list_vtable_t *multi_trees[] = {&reference_vtable, &tracking_vtable};
   runner_op = operations[0];
   runner_op.scale = 2;
   runner_op.steps = 2;
   config = (operation_run_config_t){
       .samples = 2,
       .arena_cap = 0,
       .iterations = 1,
       .output_dir = created_dir,
       .kind = "test",
       .header = "Header",
       .trees = multi_trees,
       .tree_count = 2,
       .distributions = dists,
       .distribution_count = 1,
   };
   operation_run_matrix(&runner_op, &config, NULL, NULL,
                        test_operation_run_after_chunk, NULL);

   path_format(default_kind_dir, sizeof(default_kind_dir), "%s/operations/test",
               created_dir);
   path_format(default_op_dir, sizeof(default_op_dir), "%s/%s",
               default_kind_dir, runner_op.name);
   for (int i = 0; i < 2; i++) {
      path_format(output_file, sizeof(output_file), "%s/%s", default_op_dir,
                  multi_trees[i]->name);
      path_format(tmp_output_file, sizeof(tmp_output_file), "%s.tmp",
                  output_file);
      file = fopen_or_exit(output_file, "r");
      test_expect_file_line(file, output_file, line, sizeof(line), "Header\n");
      test_expect_file_line(file, output_file, line, sizeof(line), "2 1 1 1\n");
      test_expect_file_line(file, output_file, line, sizeof(line), "2 2 1 2\n");
      test_expect_file_eof(file, output_file, line, sizeof(line));
      fclose_or_exit(file, output_file);
      assert_path_absent(tmp_output_file);
      assert_operation_run_temp_absent(output_file);
      invariant(remove(output_file) == 0);
   }
   invariant(rmdir(default_op_dir) == 0);
   invariant(rmdir(default_kind_dir) == 0);
   path_format(default_kind_dir, sizeof(default_kind_dir), "%s/operations",
               created_dir);
   invariant(rmdir(default_kind_dir) == 0);
   invariant(rmdir(created_dir) == 0);
   config.path = test_operation_run_path;
   printf("  multi-tree runner output paths OK\n");

   char atomic_publish_output_dir[] =
       "/tmp/bst-operation-atomic-publish-XXXXXX";
   created_dir = mkdtemp(atomic_publish_output_dir);
   invariant(created_dir != NULL);
   runner_op = operations[0];
   runner_op.scale = 1;
   runner_op.steps = 1;
   config = (operation_run_config_t){
       .samples = 1,
       .arena_cap = 0,
       .iterations = 1,
       .output_dir = created_dir,
       .kind = "test",
       .header = "Header",
       .trees = multi_trees,
       .tree_count = 2,
       .distributions = dists,
       .distribution_count = 1,
   };

   fflush(NULL);
   pid_t atomic_publish_pid = fork();
   invariant(atomic_publish_pid >= 0);
   if (atomic_publish_pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      operation_run_matrix(&runner_op, &config, NULL, NULL,
                           test_operation_run_abort_tracking_after_chunk, NULL);
      _exit(0);
   }
   assert_child_aborted(atomic_publish_pid);

   path_format(default_kind_dir, sizeof(default_kind_dir), "%s/operations/test",
               created_dir);
   path_format(default_op_dir, sizeof(default_op_dir), "%s/%s",
               default_kind_dir, runner_op.name);
   for (int i = 0; i < 2; i++) {
      path_format(output_file, sizeof(output_file), "%s/%s", default_op_dir,
                  multi_trees[i]->name);
      test_path_temporary_attempt(tmp_output_file, sizeof(tmp_output_file),
                                  output_file, atomic_publish_pid, 0);
      assert_path_absent(output_file);
      invariant(remove(tmp_output_file) == 0);
   }
   invariant(rmdir(default_op_dir) == 0);
   invariant(rmdir(default_kind_dir) == 0);
   path_format(default_kind_dir, sizeof(default_kind_dir), "%s/operations",
               created_dir);
   invariant(rmdir(default_kind_dir) == 0);
   invariant(rmdir(created_dir) == 0);
   printf("  multi-tree runner publish boundary OK\n");

   char partial_distribution_output_dir[] =
       "/tmp/bst-operation-partial-distribution-XXXXXX";
   created_dir = mkdtemp(partial_distribution_output_dir);
   invariant(created_dir != NULL);
   runner_op = operations[0];
   runner_op.scale = 1;
   runner_op.steps = 1;
   const dist_spec_t *partial_distribution_dists[] = {
       dist_find("Uniform"),
       dist_find("Maximum"),
   };
   invariant(partial_distribution_dists[0] != NULL);
   invariant(partial_distribution_dists[1] != NULL);
   config = (operation_run_config_t){
       .samples = 1,
       .arena_cap = 0,
       .iterations = 1,
       .output_dir = created_dir,
       .kind = "test",
       .header = "Header",
       .trees = trees,
       .tree_count = 1,
       .distributions = partial_distribution_dists,
       .distribution_count = 2,
       .path = test_operation_run_path,
   };

   fflush(NULL);
   pid_t partial_distribution_pid = fork();
   invariant(partial_distribution_pid >= 0);
   if (partial_distribution_pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      operation_run_matrix(&runner_op, &config, NULL, NULL,
                           test_operation_run_abort_maximum_after_chunk, NULL);
      _exit(0);
   }
   assert_child_aborted(partial_distribution_pid);

   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   test_path_temporary_attempt(tmp_output_file, sizeof(tmp_output_file),
                               output_file, partial_distribution_pid, 0);
   assert_path_absent(output_file);
   invariant(remove(tmp_output_file) == 0);
   invariant(rmdir(created_dir) == 0);
   printf("  multi-distribution runner publish boundary OK\n");

   runner_op = operations[0];
   runner_op.scale = 9;
   runner_op.steps = 3;

   char sampled_output_dir[] = "/tmp/bst-operation-samples-XXXXXX";
   created_dir = mkdtemp(sampled_output_dir);
   invariant(created_dir != NULL);

   config = (operation_run_config_t){
       .samples = 10,
       .arena_cap = 0,
       .iterations = 0,
       .output_dir = created_dir,
       .kind = "test",
       .header = "Header",
       .trees = trees,
       .tree_count = 1,
       .distributions = dists,
       .distribution_count = 1,
       .path = test_operation_run_path,
   };
   operation_run_matrix(&runner_op, &config, NULL, NULL,
                        test_operation_run_after_chunk, NULL);

   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   path_format(tmp_output_file, sizeof(tmp_output_file), "%s.tmp", output_file);
   file = fopen_or_exit(output_file, "r");
   test_expect_file_line(file, output_file, line, sizeof(line), "Header\n");
   test_expect_file_line(file, output_file, line, sizeof(line), "9 1 1 1\n");
   test_expect_file_line(file, output_file, line, sizeof(line), "9 2 1 2\n");
   test_expect_file_line(file, output_file, line, sizeof(line), "9 3 1 3\n");
   test_expect_file_eof(file, output_file, line, sizeof(line));
   fclose_or_exit(file, output_file);
   assert_path_absent(tmp_output_file);
   assert_operation_run_temp_absent(output_file);
   invariant(remove(output_file) == 0);
   invariant(rmdir(created_dir) == 0);
   printf("  sample clamping rows OK\n");

   runner_op = operations[0];
   runner_op.scale = 5;
   runner_op.steps = 4;

   char hook_output_dir[] = "/tmp/bst-operation-hooks-XXXXXX";
   created_dir = mkdtemp(hook_output_dir);
   invariant(created_dir != NULL);

   test_operation_run_hooks_t hooks = {0};
   test_operation_run_seed_calls = 0;
   const dist_spec_t *matrix_dists[] = {
       dist_find("Uniform"),
       dist_find("Maximum"),
   };
   invariant(matrix_dists[0] != NULL);
   invariant(matrix_dists[1] != NULL);
   config = (operation_run_config_t){
       .samples = 2,
       .arena_cap = 0,
       .iterations = 2,
       .output_dir = created_dir,
       .kind = "test",
       .header = "Header",
       .trees = trees,
       .tree_count = 1,
       .distributions = matrix_dists,
       .distribution_count = 2,
       .path = test_operation_run_path,
       .seed = test_operation_run_seed,
   };
   operation_run_matrix(&runner_op, &config, test_operation_run_progress,
                        test_operation_run_before_chunk,
                        test_operation_run_after_chunk, &hooks);
   assert_equal(4, hooks.progress_calls);
   assert_equal(8, hooks.before_calls);
   assert_equal(8, hooks.after_calls);
   assert_equal(4, test_operation_run_seed_calls);

   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   path_format(tmp_output_file, sizeof(tmp_output_file), "%s.tmp", output_file);
   file = fopen_or_exit(output_file, "r");
   test_expect_file_line(file, output_file, line, sizeof(line), "Header\n");
   for (int repeat = 0; repeat < 4; repeat++) {
      test_expect_file_line(file, output_file, line, sizeof(line), "5 2 2 2\n");
      test_expect_file_line(file, output_file, line, sizeof(line), "5 4 2 4\n");
   }
   test_expect_file_eof(file, output_file, line, sizeof(line));
   fclose_or_exit(file, output_file);
   assert_path_absent(tmp_output_file);
   assert_operation_run_temp_absent(output_file);
   invariant(remove(output_file) == 0);
   invariant(rmdir(created_dir) == 0);
   printf("  runner hook lifecycle OK\n");

#ifdef DEBUG
   runner_op = operations[0];
   runner_op.scale = 1;
   runner_op.steps = 1;

   config = (operation_run_config_t){
       .samples = 1,
       .arena_cap = 0,
       .iterations = 1,
       .output_dir = NULL,
       .kind = "test",
       .trees = trees,
       .tree_count = 1,
       .distributions = dists,
       .distribution_count = 1,
       .path = test_operation_run_path,
   };

   char writing_progress_hook_output_dir[] =
       "/tmp/bst-operation-writing-progress-hook-XXXXXX";
   created_dir = mkdtemp(writing_progress_hook_output_dir);
   invariant(created_dir != NULL);
   config.output_dir = created_dir;

   fflush(NULL);
   pid_t writing_progress_pid;
   {
      writing_progress_pid = fork();
      invariant(writing_progress_pid >= 0);
      if (writing_progress_pid == 0) {
         FILE *err = freopen("/dev/null", "w", stderr);
         invariant(err != NULL);
         operation_run_matrix(&runner_op, &config,
                              test_operation_run_writing_hook, NULL, NULL,
                              NULL);
         _exit(0);
      }
      assert_child_aborted(writing_progress_pid);
   }

   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   test_path_temporary_attempt(tmp_output_file, sizeof(tmp_output_file),
                               output_file, writing_progress_pid, 0);
   assert_path_absent(output_file);
   path_unlink_if_exists_or_exit(tmp_output_file);
   invariant(rmdir(created_dir) == 0);
   printf("  runner progress hook file-write guard OK\n");

   char rewinding_progress_hook_output_dir[] =
       "/tmp/bst-operation-rewinding-progress-hook-XXXXXX";
   created_dir = mkdtemp(rewinding_progress_hook_output_dir);
   invariant(created_dir != NULL);
   config.output_dir = created_dir;

   fflush(NULL);
   pid_t rewinding_progress_pid;
   {
      rewinding_progress_pid = fork();
      invariant(rewinding_progress_pid >= 0);
      if (rewinding_progress_pid == 0) {
         FILE *err = freopen("/dev/null", "w", stderr);
         invariant(err != NULL);
         operation_run_matrix(&runner_op, &config,
                              test_operation_run_writing_rewinding_hook, NULL,
                              NULL, NULL);
         _exit(0);
      }
      assert_child_aborted(rewinding_progress_pid);
   }

   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   test_path_temporary_attempt(tmp_output_file, sizeof(tmp_output_file),
                               output_file, rewinding_progress_pid, 0);
   assert_path_absent(output_file);
   path_unlink_if_exists_or_exit(tmp_output_file);
   invariant(rmdir(created_dir) == 0);
   printf("  runner progress hook rewind-write guard OK\n");

   char writing_seed_output_dir[] = "/tmp/bst-operation-writing-seed-XXXXXX";
   created_dir = mkdtemp(writing_seed_output_dir);
   invariant(created_dir != NULL);
   config.output_dir = created_dir;
   config.seed = test_operation_run_writing_seed;

   fflush(NULL);
   pid_t writing_seed_pid;
   {
      writing_seed_pid = fork();
      invariant(writing_seed_pid >= 0);
      if (writing_seed_pid == 0) {
         FILE *err = freopen("/dev/null", "w", stderr);
         invariant(err != NULL);
         operation_run_matrix(&runner_op, &config, NULL, NULL, NULL, NULL);
         _exit(0);
      }
      assert_child_aborted(writing_seed_pid);
   }

   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   test_path_temporary_attempt(tmp_output_file, sizeof(tmp_output_file),
                               output_file, writing_seed_pid, 0);
   assert_path_absent(output_file);
   path_unlink_if_exists_or_exit(tmp_output_file);
   invariant(rmdir(created_dir) == 0);
   config.seed = NULL;
   printf("  runner seed hook file-write guard OK\n");

   char mutating_path_tree_name_output_dir[] =
       "/tmp/bst-operation-mutating-path-tree-name-XXXXXX";
   created_dir = mkdtemp(mutating_path_tree_name_output_dir);
   invariant(created_dir != NULL);
   config.output_dir = created_dir;
   char mutable_path_tree_name[] = "MutablePathTree";
   list_vtable_t mutable_path_tree_vtable = reference_vtable;
   mutable_path_tree_vtable.name = mutable_path_tree_name;
   const list_vtable_t *mutable_path_trees[] = {&mutable_path_tree_vtable};
   config.trees = mutable_path_trees;
   config.path = test_operation_run_mutating_tree_name_path;

   fflush(NULL);
   pid_t mutating_path_tree_name_pid;
   {
      mutating_path_tree_name_pid = fork();
      invariant(mutating_path_tree_name_pid >= 0);
      if (mutating_path_tree_name_pid == 0) {
         FILE *err = freopen("/dev/null", "w", stderr);
         invariant(err != NULL);
         operation_run_matrix(&runner_op, &config, NULL, NULL, NULL, NULL);
         _exit(0);
      }
      assert_child_aborted(mutating_path_tree_name_pid);
   }

   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   assert_path_absent(output_file);
   assert_operation_run_temp_absent_for_pid(output_file,
                                            mutating_path_tree_name_pid);
   invariant(rmdir(created_dir) == 0);
   config.trees = trees;
   config.path = test_operation_run_path;
   printf("  runner path callback mutation guard OK\n");

   char writing_before_hook_output_dir[] =
       "/tmp/bst-operation-writing-before-hook-XXXXXX";
   created_dir = mkdtemp(writing_before_hook_output_dir);
   invariant(created_dir != NULL);
   config.output_dir = created_dir;

   fflush(NULL);
   pid_t writing_before_pid;
   {
      writing_before_pid = fork();
      invariant(writing_before_pid >= 0);
      if (writing_before_pid == 0) {
         FILE *err = freopen("/dev/null", "w", stderr);
         invariant(err != NULL);
         operation_run_matrix(&runner_op, &config, NULL,
                              test_operation_run_writing_hook, NULL, NULL);
         _exit(0);
      }
      assert_child_aborted(writing_before_pid);
   }

   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   test_path_temporary_attempt(tmp_output_file, sizeof(tmp_output_file),
                               output_file, writing_before_pid, 0);
   assert_path_absent(output_file);
   path_unlink_if_exists_or_exit(tmp_output_file);
   invariant(rmdir(created_dir) == 0);
   printf("  runner before hook file-write guard OK\n");

   char mutating_ctx_hook_output_dir[] =
       "/tmp/bst-operation-mutating-ctx-hook-XXXXXX";
   created_dir = mkdtemp(mutating_ctx_hook_output_dir);
   invariant(created_dir != NULL);
   config.output_dir = created_dir;

   fflush(NULL);
   pid_t mutating_ctx_pid;
   {
      mutating_ctx_pid = fork();
      invariant(mutating_ctx_pid >= 0);
      if (mutating_ctx_pid == 0) {
         FILE *err = freopen("/dev/null", "w", stderr);
         invariant(err != NULL);
         operation_run_matrix(&runner_op, &config,
                              test_operation_run_mutating_ctx_hook, NULL, NULL,
                              &hooks);
         _exit(0);
      }
      assert_child_aborted(mutating_ctx_pid);
   }

   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   test_path_temporary_attempt(tmp_output_file, sizeof(tmp_output_file),
                               output_file, mutating_ctx_pid, 0);
   assert_path_absent(output_file);
   path_unlink_if_exists_or_exit(tmp_output_file);
   invariant(rmdir(created_dir) == 0);
   printf("  runner hook context mutation guard OK\n");

   char mutating_path_hook_output_dir[] =
       "/tmp/bst-operation-mutating-path-hook-XXXXXX";
   created_dir = mkdtemp(mutating_path_hook_output_dir);
   invariant(created_dir != NULL);
   config.output_dir = created_dir;

   fflush(NULL);
   pid_t mutating_path_pid;
   {
      mutating_path_pid = fork();
      invariant(mutating_path_pid >= 0);
      if (mutating_path_pid == 0) {
         FILE *err = freopen("/dev/null", "w", stderr);
         invariant(err != NULL);
         operation_run_matrix(&runner_op, &config,
                              test_operation_run_mutating_path_hook, NULL, NULL,
                              NULL);
         _exit(0);
      }
      assert_child_aborted(mutating_path_pid);
   }

   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   test_path_temporary_attempt(tmp_output_file, sizeof(tmp_output_file),
                               output_file, mutating_path_pid, 0);
   assert_path_absent(output_file);
   path_unlink_if_exists_or_exit(tmp_output_file);
   invariant(rmdir(created_dir) == 0);
   printf("  runner hook path mutation guard OK\n");

   char mutating_name_hook_output_dir[] =
       "/tmp/bst-operation-mutating-name-hook-XXXXXX";
   created_dir = mkdtemp(mutating_name_hook_output_dir);
   invariant(created_dir != NULL);
   config.output_dir = created_dir;
   char mutable_operation_name[] = "MutableOperation";
   operation_t mutable_name_op = runner_op;
   mutable_name_op.name = mutable_operation_name;

   fflush(NULL);
   pid_t mutating_name_pid;
   {
      mutating_name_pid = fork();
      invariant(mutating_name_pid >= 0);
      if (mutating_name_pid == 0) {
         FILE *err = freopen("/dev/null", "w", stderr);
         invariant(err != NULL);
         operation_run_matrix(&mutable_name_op, &config,
                              test_operation_run_mutating_operation_name_hook,
                              NULL, NULL, NULL);
         _exit(0);
      }
      assert_child_aborted(mutating_name_pid);
   }

   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   test_path_temporary_attempt(tmp_output_file, sizeof(tmp_output_file),
                               output_file, mutating_name_pid, 0);
   assert_path_absent(output_file);
   path_unlink_if_exists_or_exit(tmp_output_file);
   invariant(rmdir(created_dir) == 0);
   printf("  runner hook operation-name mutation guard OK\n");

   char mutating_tree_name_hook_output_dir[] =
       "/tmp/bst-operation-mutating-tree-name-hook-XXXXXX";
   created_dir = mkdtemp(mutating_tree_name_hook_output_dir);
   invariant(created_dir != NULL);
   config.output_dir = created_dir;
   char mutable_tree_name[] = "MutableTree";
   list_vtable_t mutable_tree_vtable = reference_vtable;
   mutable_tree_vtable.name = mutable_tree_name;
   const list_vtable_t *mutable_trees[] = {&mutable_tree_vtable};
   config.trees = mutable_trees;

   fflush(NULL);
   pid_t mutating_tree_name_pid;
   {
      mutating_tree_name_pid = fork();
      invariant(mutating_tree_name_pid >= 0);
      if (mutating_tree_name_pid == 0) {
         FILE *err = freopen("/dev/null", "w", stderr);
         invariant(err != NULL);
         operation_run_matrix(&runner_op, &config,
                              test_operation_run_mutating_tree_name_hook, NULL,
                              NULL, NULL);
         _exit(0);
      }
      assert_child_aborted(mutating_tree_name_pid);
   }

   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   test_path_temporary_attempt(tmp_output_file, sizeof(tmp_output_file),
                               output_file, mutating_tree_name_pid, 0);
   assert_path_absent(output_file);
   path_unlink_if_exists_or_exit(tmp_output_file);
   invariant(rmdir(created_dir) == 0);
   config.trees = trees;
   printf("  runner hook tree-name mutation guard OK\n");

   char mutating_dist_hook_output_dir[] =
       "/tmp/bst-operation-mutating-dist-hook-XXXXXX";
   created_dir = mkdtemp(mutating_dist_hook_output_dir);
   invariant(created_dir != NULL);
   config.output_dir = created_dir;
   char mutable_distribution_name[] = "MutableDistribution";
   dist_spec_t mutable_dist = {
       .name = mutable_distribution_name,
       .factory = uniform_new,
   };
   const dist_spec_t *mutable_dists[] = {&mutable_dist};
   config.distributions = mutable_dists;

   fflush(NULL);
   pid_t mutating_dist_pid;
   {
      mutating_dist_pid = fork();
      invariant(mutating_dist_pid >= 0);
      if (mutating_dist_pid == 0) {
         FILE *err = freopen("/dev/null", "w", stderr);
         invariant(err != NULL);
         operation_run_matrix(
             &runner_op, &config,
             test_operation_run_mutating_distribution_name_hook, NULL, NULL,
             NULL);
         _exit(0);
      }
      assert_child_aborted(mutating_dist_pid);
   }

   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   test_path_temporary_attempt(tmp_output_file, sizeof(tmp_output_file),
                               output_file, mutating_dist_pid, 0);
   assert_path_absent(output_file);
   path_unlink_if_exists_or_exit(tmp_output_file);
   invariant(rmdir(created_dir) == 0);
   config.distributions = dists;
   printf("  runner hook distribution-name mutation guard OK\n");

   char mutating_hook_output_dir[] = "/tmp/bst-operation-mutating-hook-XXXXXX";
   created_dir = mkdtemp(mutating_hook_output_dir);
   invariant(created_dir != NULL);
   config.output_dir = created_dir;

   fflush(NULL);
   pid_t mutating_pid;
   {
      mutating_pid = fork();
      invariant(mutating_pid >= 0);
      if (mutating_pid == 0) {
         FILE *err = freopen("/dev/null", "w", stderr);
         invariant(err != NULL);
         operation_run_matrix(&runner_op, &config, NULL, NULL,
                              test_operation_run_mutating_after_chunk, NULL);
         _exit(0);
      }
      assert_child_aborted(mutating_pid);
   }

   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   test_path_temporary_attempt(tmp_output_file, sizeof(tmp_output_file),
                               output_file, mutating_pid, 0);
   assert_path_absent(output_file);
   path_unlink_if_exists_or_exit(tmp_output_file);
   invariant(rmdir(created_dir) == 0);
   printf("  runner hook mutation guard OK\n");

   char mutating_tree_hook_output_dir[] =
       "/tmp/bst-operation-mutating-tree-hook-XXXXXX";
   created_dir = mkdtemp(mutating_tree_hook_output_dir);
   invariant(created_dir != NULL);

   const list_vtable_t *tree_backed[] = {tree_registry_find("Splay")};
   invariant(tree_backed[0] != NULL);
   config = (operation_run_config_t){
       .samples = 1,
       .arena_cap = 4,
       .iterations = 1,
       .output_dir = created_dir,
       .kind = "test",
       .trees = tree_backed,
       .tree_count = 1,
       .distributions = dists,
       .distribution_count = 1,
       .path = test_operation_run_path,
   };

   fflush(NULL);
   pid_t mutating_tree_pid;
   {
      mutating_tree_pid = fork();
      invariant(mutating_tree_pid >= 0);
      if (mutating_tree_pid == 0) {
         FILE *err = freopen("/dev/null", "w", stderr);
         invariant(err != NULL);
         operation_run_matrix(&runner_op, &config, NULL, NULL,
                              test_operation_run_mutating_tree_after_chunk,
                              NULL);
         _exit(0);
      }
      assert_child_aborted(mutating_tree_pid);
   }

   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   test_path_temporary_attempt(tmp_output_file, sizeof(tmp_output_file),
                               output_file, mutating_tree_pid, 0);
   assert_path_absent(output_file);
   path_unlink_if_exists_or_exit(tmp_output_file);
   invariant(rmdir(created_dir) == 0);
   printf("  runner hook tree mutation guard OK\n");

   char aliasing_tree_hook_output_dir[] =
       "/tmp/bst-operation-aliasing-tree-hook-XXXXXX";
   created_dir = mkdtemp(aliasing_tree_hook_output_dir);
   invariant(created_dir != NULL);

   runner_op = operations[0];
   runner_op.scale = 3;
   runner_op.steps = 3;
   config = (operation_run_config_t){
       .samples = 1,
       .arena_cap = 4,
       .iterations = 1,
       .output_dir = created_dir,
       .kind = "test",
       .trees = tree_backed,
       .tree_count = 1,
       .distributions = dists,
       .distribution_count = 1,
       .path = test_operation_run_path,
   };

   fflush(NULL);
   pid_t aliasing_tree_pid;
   {
      aliasing_tree_pid = fork();
      invariant(aliasing_tree_pid >= 0);
      if (aliasing_tree_pid == 0) {
         FILE *err = freopen("/dev/null", "w", stderr);
         invariant(err != NULL);
         operation_run_matrix(&runner_op, &config, NULL, NULL,
                              test_operation_run_aliasing_tree_after_chunk,
                              NULL);
         _exit(0);
      }
      assert_child_aborted(aliasing_tree_pid);
   }

   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   test_path_temporary_attempt(tmp_output_file, sizeof(tmp_output_file),
                               output_file, aliasing_tree_pid, 0);
   assert_path_absent(output_file);
   path_unlink_if_exists_or_exit(tmp_output_file);
   invariant(rmdir(created_dir) == 0);
   printf("  runner hook tree shape guard OK\n");

   char non_rank_tree_hook_output_dir[] =
       "/tmp/bst-operation-non-rank-tree-hook-XXXXXX";
   created_dir = mkdtemp(non_rank_tree_hook_output_dir);
   invariant(created_dir != NULL);

   runner_op = operations[0];
   runner_op.scale = 2;
   runner_op.steps = 2;
   const list_vtable_t *non_rank_tree_backed[] = {&conc_vtable};
   config = (operation_run_config_t){
       .samples = 2,
       .arena_cap = 8,
       .iterations = 1,
       .output_dir = created_dir,
       .kind = "test",
       .trees = non_rank_tree_backed,
       .tree_count = 1,
       .distributions = dists,
       .distribution_count = 1,
       .path = test_operation_run_path,
   };
   operation_run_matrix(&runner_op, &config, NULL, NULL,
                        test_operation_run_after_chunk, NULL);

   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   path_format(tmp_output_file, sizeof(tmp_output_file), "%s.tmp", output_file);
   assert_path_absent(tmp_output_file);
   assert_operation_run_temp_absent(output_file);
   invariant(remove(output_file) == 0);
   invariant(rmdir(created_dir) == 0);
   printf("  runner non-rank tree observer OK\n");

   runner_op = operations[0];
   runner_op.scale = 1;
   runner_op.steps = 1;

#ifdef PERSISTENT
   char mutating_refs_hook_output_dir[] =
       "/tmp/bst-operation-mutating-refs-hook-XXXXXX";
   created_dir = mkdtemp(mutating_refs_hook_output_dir);
   invariant(created_dir != NULL);

   config = (operation_run_config_t){
       .samples = 1,
       .arena_cap = 4,
       .iterations = 1,
       .output_dir = created_dir,
       .kind = "test",
       .trees = tree_backed,
       .tree_count = 1,
       .distributions = dists,
       .distribution_count = 1,
       .path = test_operation_run_path,
   };

   fflush(NULL);
   pid_t mutating_refs_pid;
   {
      mutating_refs_pid = fork();
      invariant(mutating_refs_pid >= 0);
      if (mutating_refs_pid == 0) {
         FILE *err = freopen("/dev/null", "w", stderr);
         invariant(err != NULL);
         operation_run_matrix(&runner_op, &config, NULL, NULL,
                              test_operation_run_mutating_refs_after_chunk,
                              NULL);
         _exit(0);
      }
      assert_child_aborted(mutating_refs_pid);
   }

   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   test_path_temporary_attempt(tmp_output_file, sizeof(tmp_output_file),
                               output_file, mutating_refs_pid, 0);
   assert_path_absent(output_file);
   path_unlink_if_exists_or_exit(tmp_output_file);
   invariant(rmdir(created_dir) == 0);
   printf("  runner hook refcount mutation guard OK\n");
#endif
#endif

   runner_op = operations[0];
   runner_op.scale = 3;
   runner_op.steps = 1;

   char default_seed_output_dir[] = "/tmp/bst-operation-default-seeds-XXXXXX";
   created_dir = mkdtemp(default_seed_output_dir);
   invariant(created_dir != NULL);

   const dist_spec_t *recording_dists[] = {&test_recording_dist_spec};
   test_operation_run_recorded_seed_count = 0;
   config = (operation_run_config_t){
       .samples = 1,
       .arena_cap = 0,
       .iterations = 3,
       .output_dir = created_dir,
       .kind = "test",
       .header = "Header",
       .trees = trees,
       .tree_count = 1,
       .distributions = recording_dists,
       .distribution_count = 1,
       .path = test_operation_run_path,
   };
   operation_run_matrix(&runner_op, &config, NULL, NULL,
                        test_operation_run_after_chunk, NULL);
   assert_equal(3, test_operation_run_recorded_seed_count);
   u64 previous_seed = 0;
   for (u64 iteration = 1; iteration <= test_operation_run_recorded_seed_count;
        iteration++) {
      u64 seed = random_derive_seed(0, "operation-runs");
      seed = random_derive_seed(seed, runner_op.name);
      seed = random_derive_seed(seed, "scale");
      seed = random_derive_u64(seed, runner_op.scale);
      seed = random_derive_seed(seed, "steps");
      seed = random_derive_u64(seed, runner_op.steps);
      seed = random_derive_seed(seed, test_recording_dist_spec.name);
      seed = random_derive_seed(seed, "iteration");
      seed = random_derive_u64(seed, iteration);
      assert_equal(seed, test_operation_run_recorded_seeds[iteration - 1]);
      if (iteration > 1) {
         invariant(seed != previous_seed);
      }
      previous_seed = seed;
   }
   path_format(output_file, sizeof(output_file), "%s/run.txt", created_dir);
   path_format(tmp_output_file, sizeof(tmp_output_file), "%s.tmp", output_file);
   assert_path_absent(tmp_output_file);
   assert_operation_run_temp_absent(output_file);
   invariant(remove(output_file) == 0);
   invariant(rmdir(created_dir) == 0);
   printf("  default runner seeds OK\n");
}

#ifdef MEASUREMENTS
static void test_measurement_null_counter(void) {
   measurement(NULL, 1);
}

static void test_measurement_overflow_counter(void) {
   measurement_rotations = UINT64_MAX;
   measurement(&measurement_rotations, 1);
}

static void test_measurement_orphan_partition_depth_snapshot(void) {
   measurement_snapshot_require((measurement_snapshot_t){.partition_depth = 1});
}

static void test_measurement_orphan_partition_descent(void) {
   measurement_reset_all();
   measurement_record_partition_descent();
}

static void assert_measurement_child_aborts(void (*fn)(void)) {
   invariant(fn != NULL);
   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      fn();
      _exit(0);
   }
   assert_child_aborted(pid);
}

static u64 test_measurement_run_seed(const operation_run_t *run) {
   return operation_run_workload_seed(run, "operation-measurements");
}

static void test_measurement_run_before_chunk(operation_run_t *run) {
   invariant(run != NULL);
   measurement_reset_all();
}

static void test_measurement_run_after_chunk(operation_run_t *run) {
   invariant(run != NULL);
   invariant(run->file != NULL);
   invariant(run->path != NULL);
   list_verify(&run->instance);

   tree_t *tree = list_tree(&run->instance);
   invariant(tree != NULL);
   tree_sized_handle_require(*tree);

   measurement_snapshot_t measurements = measurement_snapshot();
   node_path_metrics_t path =
       tree_verify_size_path_metrics(tree->root, tree->size);
   double apl = nodeAveragePathLengthFromMetrics(path);
   int mpl = nodeMaximumPathLengthFromMetrics(path);

   file_printf_or_exit(
       run->file, run->path,
       "%s %llu %llu %llu %llu %llu %llu %llu %.17g %d %llu %llu\n",
       run->dist->name, (unsigned long long)run->scale,
       (unsigned long long)run->steps, (unsigned long long)tree->size,
       (unsigned long long)run->step, (unsigned long long)run->position,
       (unsigned long long)measurements.partition_count,
       (unsigned long long)measurements.partition_depth, apl, mpl,
       (unsigned long long)measurements.rotations,
       (unsigned long long)measurements.allocations);
}

static void assert_measurement_operation_run_emits_rows(void) {
   operation_t operations[operation_default_count];
   assert_int_equal(operation_default_count, operation_defaults(operations));
   operation_t op = operations[0];
   op.scale = 4;
   op.steps = 4;

   const list_vtable_t *tree = tree_registry_find("Splay");
   invariant(tree != NULL);
   const list_vtable_t *trees[] = {tree};
   const dist_spec_t *dist = dist_find("Uniform");
   invariant(dist != NULL);
   const dist_spec_t *dists[] = {dist};

   char output_dir[] = "/tmp/bst-measurement-run-XXXXXX";
   char *root = mkdtemp(output_dir);
   invariant(root != NULL);

   operation_run_config_t config = {
       .samples = 2,
       .arena_cap = 32,
       .iterations = 1,
       .output_dir = root,
       .kind = "measurements",
       .header = BENCHMARK_SCHEMA_OPERATION_MEASUREMENT_HEADER,
       .trees = trees,
       .tree_count = 1,
       .distributions = dists,
       .distribution_count = 1,
       .seed = test_measurement_run_seed,
       .path = test_operation_run_path,
   };
   operation_run_matrix(&op, &config, NULL, test_measurement_run_before_chunk,
                        test_measurement_run_after_chunk, NULL);

   char path[512];
   path_format(path, sizeof(path), "%s/run.txt", root);
   char tmp_path[512];
   path_format(tmp_path, sizeof(tmp_path), "%s.tmp", path);
   assert_path_absent(tmp_path);
   assert_operation_run_temp_absent(path);

   FILE *file = fopen_or_exit(path, "r");
   char line[256];
   test_expect_file_line(file, path, line, sizeof(line),
                         BENCHMARK_SCHEMA_OPERATION_MEASUREMENT_HEADER "\n");
#ifdef PERSISTENT
   test_expect_file_line(file, path, line, sizeof(line),
                         "Uniform 4 4 2 2 2 0 0 0.5 1 0 3\n");
   test_expect_file_line(file, path, line, sizeof(line),
                         "Uniform 4 4 4 2 4 0 0 1.5 3 1 5\n");
#else
   test_expect_file_line(file, path, line, sizeof(line),
                         "Uniform 4 4 2 2 2 0 0 0.5 1 0 2\n");
   test_expect_file_line(file, path, line, sizeof(line),
                         "Uniform 4 4 4 2 4 0 0 1 2 1 2\n");
#endif
   test_expect_file_eof(file, path, line, sizeof(line));
   fclose_or_exit(file, path);

   invariant(remove(path) == 0);
   invariant(rmdir(root) == 0);
}

static void assert_measurement_balancer_restore_records_counters(void) {
   const dist_spec_t *dist = dist_find("Uniform");
   invariant(dist != NULL);

   arena_t arena;
   arena_init(&arena, 8);
   arena_t *previous = arena_install(&arena);

   splay_t instance;
   splay_init(&instance);

   balancer_run_grow_splay_to(&instance, 2);
   instance.tree = balancer_run_randomize_tree(instance.tree, dist,
                                               "balancer-measurements", 1);
   measurement_reset_all();
   instance.tree = balancer_run_restore_tree(&balancer_median, instance.tree);
   balancer_verify_tree(&balancer_median, instance.tree);

   measurement_snapshot_t snapshot = measurement_snapshot();
   assert_equal(0, snapshot.partition_count);
   assert_equal(0, snapshot.partition_depth);
   assert_equal(0, snapshot.rotations);

   balancer_run_grow_splay_to(&instance, 4);
   instance.tree = balancer_run_randomize_tree(instance.tree, dist,
                                               "balancer-measurements", 1);
   measurement_reset_all();
   instance.tree = balancer_run_restore_tree(&balancer_median, instance.tree);
   balancer_verify_tree(&balancer_median, instance.tree);

   snapshot = measurement_snapshot();
   assert_equal(1, snapshot.partition_count);
   assert_equal(2, snapshot.partition_depth);
   assert_equal(0, snapshot.rotations);

   splay_free(&instance);
   arena_restore(previous);
   arena_destroy(&arena);
}
#endif

static void TestMeasurements(void) {
   printf("Measurements:\n");

   measurement_reset_all();

#ifdef MEASUREMENTS
   measurement(&measurement_rotations, 3);
   measurement(&measurement_allocations, 5);
   measurement(&measurement_partition_count, 7);
   measurement(&measurement_partition_depth, 11);

   measurement_snapshot_t snapshot = measurement_snapshot();
   assert_equal(3, snapshot.rotations);
   assert_equal(5, snapshot.allocations);
   assert_equal(7, snapshot.partition_count);
   assert_equal(11, snapshot.partition_depth);

   measurement_reset_all();
   measurement_record_rotation();
   measurement_record_allocation();
   measurement_record_partition();
   measurement_record_partition_descent();

   snapshot = measurement_snapshot();
   assert_equal(1, snapshot.rotations);
   assert_equal(1, snapshot.allocations);
   assert_equal(1, snapshot.partition_count);
   assert_equal(1, snapshot.partition_depth);

   measurement_reset_all();
   snapshot = measurement_snapshot();
   assert_equal(0, snapshot.rotations);
   assert_equal(0, snapshot.allocations);
   assert_equal(0, snapshot.partition_count);
   assert_equal(0, snapshot.partition_depth);

   assert_measurement_child_aborts(test_measurement_null_counter);
   assert_measurement_child_aborts(test_measurement_overflow_counter);
   assert_measurement_child_aborts(
       test_measurement_orphan_partition_depth_snapshot);
   assert_measurement_child_aborts(test_measurement_orphan_partition_descent);
   printf("  enabled counters, helpers, reset, and guards OK\n");

   assert_measurement_operation_run_emits_rows();
   printf("  operation measurement runner output OK\n");

   assert_measurement_balancer_restore_records_counters();
   printf("  balancer measurement restore counters OK\n");
#else
   u64 local = 13;
   measurement(&local, 21);
   assert_equal(13, local);

   measurement_record_rotation();
   measurement_record_allocation();
   measurement_record_partition();
   measurement_record_partition_descent();

   measurement_snapshot_t snapshot = measurement_snapshot();
   assert_equal(0, snapshot.rotations);
   assert_equal(0, snapshot.allocations);
   assert_equal(0, snapshot.partition_count);
   assert_equal(0, snapshot.partition_depth);
   printf("  disabled counters compile away OK\n");
#endif
}

static void assert_filesystem_child_exited_2(void (*fn)(void)) {
   invariant(fn != NULL);
   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      fn();
      _exit(0);
   }
   assert_child_exited_2(pid);
}

static pid_t
assert_filesystem_child_exited_2_with_stderr_pid(void (*fn)(void),
                                                 const char *expected_stderr) {
   return assert_void_child_exits_2_with_stderr(fn, expected_stderr);
}

static void
assert_filesystem_child_exited_2_with_stderr(void (*fn)(void),
                                             const char *expected_stderr) {
   (void)assert_filesystem_child_exited_2_with_stderr_pid(fn, expected_stderr);
}

static void test_file_publish_empty_tmp_path_child(void) {
   FILE *file = tmpfile();
   invariant(file != NULL);
   file_publish_or_exit(file, "", "/tmp/bst-file-publish-empty-tmp");
}

static void test_file_publish_empty_path_child(void) {
   FILE *file = tmpfile();
   invariant(file != NULL);
   file_publish_or_exit(file, "/tmp/bst-file-publish-empty-path.tmp", "");
}

static void test_file_publish_no_tmp_file_name_child(void) {
   FILE *file = tmpfile();
   invariant(file != NULL);
   file_publish_or_exit(file, ".", "/tmp/bst-file-publish-no-tmp-file-name");
}

static void test_file_publish_no_path_file_name_child(void) {
   FILE *file = tmpfile();
   invariant(file != NULL);
   file_publish_or_exit(file, "/tmp/bst-file-publish-no-path-file-name.tmp",
                        ".");
}

static void test_file_publish_same_path_child(void) {
   FILE *file = tmpfile();
   invariant(file != NULL);
   file_publish_or_exit(file, "/tmp/bst-file-publish-same-path",
                        "/tmp/bst-file-publish-same-path");
}

static char test_path_rename_missing_old[512];
static char test_path_rename_missing_new[512];

static void test_path_rename_missing_old_child(void) {
   path_rename_or_exit(test_path_rename_missing_old,
                       test_path_rename_missing_new);
}

static char test_file_publish_mismatched_handle_tmp[512];
static char test_file_publish_mismatched_handle_path[512];
static char test_file_publish_mismatched_handle_other[512];
static char test_file_publish_wrong_parent_tmp[512];
static char test_file_publish_wrong_parent_path[512];

static void test_file_publish_mismatched_handle_child(void) {
   FILE *tmp = fopen_or_exit(test_file_publish_mismatched_handle_tmp, "w");
   file_puts_or_exit("tmp", tmp, test_file_publish_mismatched_handle_tmp);
   fclose_or_exit(tmp, test_file_publish_mismatched_handle_tmp);

   FILE *other = fopen_or_exit(test_file_publish_mismatched_handle_other, "w");
   file_puts_or_exit("other", other, test_file_publish_mismatched_handle_other);
   file_publish_or_exit(other, test_file_publish_mismatched_handle_tmp,
                        test_file_publish_mismatched_handle_path);
}

static void test_file_publish_wrong_parent_child(void) {
   FILE *tmp = fopen_or_exit(test_file_publish_wrong_parent_tmp, "w");
   file_puts_or_exit("tmp", tmp, test_file_publish_wrong_parent_tmp);
   file_publish_or_exit(tmp, test_file_publish_wrong_parent_tmp,
                        test_file_publish_wrong_parent_path);
}

static void test_staged_file_publish_empty_tmp_path_child(void) {
   staged_file_t file = {
       .path = path_format_alloc("/tmp/bst-staged-file-publish-empty-tmp"),
       .tmp_path = path_format_alloc(""),
       .finished = true,
   };
   staged_file_publish_or_exit(&file);
}

static void test_staged_file_publish_no_tmp_file_name_child(void) {
   staged_file_t file = {
       .path =
           path_format_alloc("/tmp/bst-staged-file-publish-no-tmp-file-name"),
       .tmp_path = path_format_alloc("."),
       .finished = true,
   };
   staged_file_publish_or_exit(&file);
}

static void test_staged_file_publish_same_path_child(void) {
   staged_file_t file = {
       .path = path_format_alloc("/tmp/bst-staged-file-publish-same-path"),
       .tmp_path = path_format_alloc("/tmp/bst-staged-file-publish-same-path"),
       .finished = true,
   };
   staged_file_publish_or_exit(&file);
}

static char test_staged_file_dispose_same_path[512];

static void test_staged_file_dispose_same_path_child(void) {
   staged_file_t file = {
       .path = path_format_alloc("%s", test_staged_file_dispose_same_path),
       .tmp_path = path_format_alloc("%s", test_staged_file_dispose_same_path),
   };
   staged_file_dispose(&file);
}

static char test_staged_file_publish_wrong_parent_tmp[512];
static char test_staged_file_publish_wrong_parent_path[512];

static void test_staged_file_publish_wrong_parent_child(void) {
   staged_file_t file = {
       .path =
           path_format_alloc("%s", test_staged_file_publish_wrong_parent_path),
       .tmp_path =
           path_format_alloc("%s", test_staged_file_publish_wrong_parent_tmp),
       .finished = true,
   };
   staged_file_publish_or_exit(&file);
}

static void test_staged_file_finished_missing_tmp_path_child(void) {
   staged_file_t file = {
       .path = path_format_alloc("/tmp/bst-staged-file-finished-missing-tmp"),
       .finished = true,
   };
   staged_file_publish_or_exit(&file);
}

static char test_staged_file_finish_mismatched_handle_path[512];
static char test_staged_file_finish_mismatched_handle_other[512];

static void test_staged_file_finish_mismatched_handle_child(void) {
   staged_file_t file = {
       .path = path_format_alloc(
           "%s", test_staged_file_finish_mismatched_handle_path),
   };
   FILE *staged = staged_file_open_or_exit(&file, "w");
   file_puts_or_exit("staged", staged, file.tmp_path);

   FILE *other =
       fopen_or_exit(test_staged_file_finish_mismatched_handle_other, "w");
   file_puts_or_exit("other", other,
                     test_staged_file_finish_mismatched_handle_other);
   staged_file_finish_or_exit(&file, other);
}

static void test_staged_file_open_no_file_name_child(void) {
   staged_file_t file = {
       .path = path_format_alloc("."),
   };
   FILE *staged = staged_file_open_or_exit(&file, "w");
   (void)staged;
}

static void test_filesystem_malloc_zero_child(void) {
   (void)filesystem_malloc(0);
}

static void test_filesystem_malloc_array_zero_count_child(void) {
   (void)filesystem_malloc_array(0, 1, "array too large");
}

static void test_filesystem_malloc_array_zero_size_child(void) {
   (void)filesystem_malloc_array(1, 0, "array too large");
}

static void test_filesystem_malloc_array_overflow_child(void) {
   (void)filesystem_malloc_array(SIZE_MAX, 2, "array too large");
}

static void test_filesystem_checked_array_size_zero_count_child(void) {
   (void)filesystem_checked_array_size(0, 1, "custom array count is zero",
                                       "custom array element size is zero",
                                       "custom array too large");
}

static void test_filesystem_checked_array_size_zero_size_child(void) {
   (void)filesystem_checked_array_size(1, 0, "custom array count is zero",
                                       "custom array element size is zero",
                                       "custom array too large");
}

static void test_filesystem_checked_array_size_overflow_child(void) {
   (void)filesystem_checked_array_size(
       SIZE_MAX, 2, "custom array count is zero",
       "custom array element size is zero", "custom array too large");
}

static void test_filesystem_realloc_null_child(void) {
   (void)filesystem_realloc(NULL, 1);
}

static void test_filesystem_realloc_zero_size_child(void) {
   void *ptr = filesystem_malloc(1);
   (void)filesystem_realloc(ptr, 0);
}

static void test_filesystem_realloc_array_zero_count_child(void) {
   void *ptr = filesystem_malloc(1);
   (void)filesystem_realloc_array(ptr, 0, 1, "array too large");
}

static void test_filesystem_realloc_array_zero_size_child(void) {
   void *ptr = filesystem_malloc(1);
   (void)filesystem_realloc_array(ptr, 1, 0, "array too large");
}

static void test_filesystem_realloc_array_overflow_child(void) {
   void *ptr = filesystem_malloc(1);
   (void)filesystem_realloc_array(ptr, SIZE_MAX, 2, "array too large");
}

static void test_fopen_empty_path_child(void) {
   (void)fopen_or_exit("", "r");
}

static void test_fopen_empty_mode_child(void) {
   (void)fopen_or_exit("/tmp/bst-fopen-empty-mode", "");
}

static void test_fopen_temporary_empty_mode_child(void) {
   char *tmp_path = NULL;
   FILE *file = fopen_temporary_or_exit(
       &tmp_path, "/tmp/bst-fopen-temporary-empty-mode", "");
   (void)file;
}

static void test_fopen_temporary_null_mode_child(void) {
   char *tmp_path = NULL;
   FILE *file = fopen_temporary_or_exit(
       &tmp_path, "/tmp/bst-fopen-temporary-null-mode", NULL);
   (void)file;
}

static void test_fopen_temporary_no_file_name_child(void) {
   char *tmp_path = NULL;
   FILE *file = fopen_temporary_or_exit(&tmp_path, ".", "w");
   (void)file;
   bst_unreachable();
}

static void test_fopen_temporary_nonempty_output_child(void) {
   char *tmp_path = (char *)1;
   FILE *file = fopen_temporary_or_exit(
       &tmp_path, "/tmp/bst-fopen-temporary-nonempty-output", "w");
   (void)file;
}

static void test_path_is_directory_empty_path_child(void) {
   (void)path_is_directory("");
}

static void test_path_parent_no_file_name_child(void) {
   char *parent = path_parent_dir_alloc("dir/");
   (void)parent;
}

static void test_path_require_parent_dir_mismatch_child(void) {
   path_require_parent_dir("/tmp/file.txt", "/var",
                           "file path parent mismatch");
}

static void test_mkdir_or_existing_dir_empty_path_child(void) {
   mkdir_or_existing_dir("");
}

static void test_mkdir_p_empty_path_child(void) {
   mkdir_p("");
}

static void test_fd_set_cloexec_empty_path_child(void) {
   fd_set_cloexec_or_exit(STDOUT_FILENO, "");
}

static void test_fd_set_cloexec_closed_fd(void) {
   int fd = dup(STDOUT_FILENO);
   invariant(fd >= 0);
   invariant(close(fd) == 0);

   errno = 0;
   invariant(!fd_set_cloexec(fd, "closed-fd"));
   invariant(errno == EBADF);
}

static void test_file_printf_empty_path_child(void) {
   file_printf_or_exit(stdout, "", "unused");
}

static void test_file_puts_empty_path_child(void) {
   file_puts_or_exit("unused", stdout, "");
}

static void test_file_putc_empty_path_child(void) {
   file_putc_or_exit('x', stdout, "");
}

static void test_file_write_empty_path_child(void) {
   file_write_or_exit(NULL, 0, stdout, "");
}

static void test_file_read_empty_path_child(void) {
   file_read_or_exit(NULL, 0, stdin, "");
}

static void test_file_sync_empty_path_child(void) {
   file_sync_or_exit(stdout, "");
}

static void test_fclose_empty_path_child(void) {
   fclose_or_exit(stdout, "");
}

static void test_file_putc_negative_byte_child(void) {
   file_putc_or_exit(-1, stdout, "stdout");
}

static void test_file_putc_large_byte_child(void) {
   file_putc_or_exit(UCHAR_MAX + 1, stdout, "stdout");
}

static void test_file_write_null_buffer_child(void) {
   file_write_or_exit(NULL, 1, stdout, "stdout");
}

static void test_file_read_null_buffer_child(void) {
   file_read_or_exit(NULL, 1, stdin, "stdin");
}

static void assert_file_close_on_exec(FILE *file, const char *path) {
   invariant(file != NULL);
   invariant(path != NULL);

   int fd = fileno(file);
   invariant(fd >= 0);
   int flags = fcntl(fd, F_GETFD);
   if (flags < 0) {
      perror(path);
      abort();
   }
   invariant((flags & FD_CLOEXEC) != 0);
}

static void assert_wait_child_retries_after_signal(void) {
   struct sigaction action;
   memset(&action, 0, sizeof(action));
   action.sa_handler = test_wait_child_signal_handler;
   sigemptyset(&action.sa_mask);

   struct sigaction previous;
   invariant(sigaction(SIGALRM, &action, &previous) == 0);
   test_wait_child_signal_seen = 0;
   u64 previous_eintr_count = test_wait_child_eintr_count;

   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      const struct timespec delay = {
          .tv_sec = 0,
          .tv_nsec = 500000000L,
      };
      invariant(nanosleep(&delay, NULL) == 0);
      _exit(2);
   }

   const struct itimerval timer = {
       .it_interval =
           {
               .tv_sec = 0,
               .tv_usec = 50000,
           },
       .it_value =
           {
               .tv_sec = 0,
               .tv_usec = 50000,
           },
   };
   invariant(setitimer(ITIMER_REAL, &timer, NULL) == 0);
   assert_child_exited_2(pid);
   const struct itimerval no_timer = {0};
   invariant(setitimer(ITIMER_REAL, &no_timer, NULL) == 0);

   invariant(test_wait_child_signal_seen != 0);
   invariant(test_wait_child_eintr_count > previous_eintr_count);
   invariant(sigaction(SIGALRM, &previous, NULL) == 0);
}

static void TestFilesystem(void) {
   printf("Filesystem helpers:\n");

   assert_wait_child_retries_after_signal();
   printf("  child wait retries after signal OK\n");

   char output_dir[] = "/tmp/bst-filesystem-XXXXXX";
   char *created_dir = mkdtemp(output_dir);
   invariant(created_dir != NULL);

   unsigned char *bytes = (unsigned char *)filesystem_malloc_array(
       2, sizeof(bytes[0]), "byte array too large");
   bytes[0] = 11;
   bytes[1] = 22;
   bytes = (unsigned char *)filesystem_realloc_array(bytes, 4, sizeof(bytes[0]),
                                                     "byte array too large");
   invariant(bytes[0] == 11);
   invariant(bytes[1] == 22);
   bytes[2] = 33;
   bytes[3] = 44;
   free(bytes);

   assert_equal(12, (u64)filesystem_checked_array_size(
                        3, 4, "custom array count is zero",
                        "custom array element size is zero",
                        "custom array too large"));
   assert_filesystem_child_exited_2_with_stderr(
       test_filesystem_malloc_zero_child, "allocation size is zero");
   assert_filesystem_child_exited_2_with_stderr(
       test_filesystem_malloc_array_zero_count_child,
       "allocation count is zero");
   assert_filesystem_child_exited_2_with_stderr(
       test_filesystem_malloc_array_zero_size_child,
       "allocation element size is zero");
   assert_filesystem_child_exited_2_with_stderr(
       test_filesystem_malloc_array_overflow_child, "array too large");
   assert_filesystem_child_exited_2_with_stderr(
       test_filesystem_checked_array_size_zero_count_child,
       "custom array count is zero");
   assert_filesystem_child_exited_2_with_stderr(
       test_filesystem_checked_array_size_zero_size_child,
       "custom array element size is zero");
   assert_filesystem_child_exited_2_with_stderr(
       test_filesystem_checked_array_size_overflow_child,
       "custom array too large");
   assert_filesystem_child_exited_2_with_stderr(
       test_filesystem_realloc_null_child, "reallocation pointer is null");
   assert_filesystem_child_exited_2_with_stderr(
       test_filesystem_realloc_zero_size_child, "reallocation size is zero");
   assert_filesystem_child_exited_2_with_stderr(
       test_filesystem_realloc_array_zero_count_child,
       "allocation count is zero");
   assert_filesystem_child_exited_2_with_stderr(
       test_filesystem_realloc_array_zero_size_child,
       "allocation element size is zero");
   assert_filesystem_child_exited_2_with_stderr(
       test_filesystem_realloc_array_overflow_child, "array too large");
   printf("  allocation helper contracts OK\n");

   char nested[512];
   path_format(nested, sizeof(nested), "%s/a/b/c", created_dir);
   mkdir_p(nested);
   mkdir_p(nested);
   invariant(path_is_directory(nested));
   assert_filesystem_child_exited_2_with_stderr(
       test_path_is_directory_empty_path_child, "directory path is empty");
   assert_filesystem_child_exited_2_with_stderr(
       test_mkdir_or_existing_dir_empty_path_child, "directory path is empty");
   assert_filesystem_child_exited_2_with_stderr(test_mkdir_p_empty_path_child,
                                                "directory path is empty");
   assert_filesystem_child_exited_2_with_stderr(
       test_fd_set_cloexec_empty_path_child, "file path is empty");
   test_fd_set_cloexec_closed_fd();
   assert_filesystem_child_exited_2_with_stderr(
       test_file_printf_empty_path_child, "file path is empty");
   assert_filesystem_child_exited_2_with_stderr(test_file_puts_empty_path_child,
                                                "file path is empty");
   assert_filesystem_child_exited_2_with_stderr(test_file_putc_empty_path_child,
                                                "file path is empty");
   assert_filesystem_child_exited_2_with_stderr(
       test_file_write_empty_path_child, "file path is empty");
   assert_filesystem_child_exited_2_with_stderr(test_file_read_empty_path_child,
                                                "file path is empty");
   assert_filesystem_child_exited_2_with_stderr(test_file_sync_empty_path_child,
                                                "file path is empty");
   assert_filesystem_child_exited_2_with_stderr(test_fclose_empty_path_child,
                                                "file path is empty");

   char *parent = path_parent_dir_alloc("file.txt");
   invariant(strcmp(parent, ".") == 0);
   free(parent);
   parent = path_parent_dir_alloc("/file.txt");
   invariant(strcmp(parent, "/") == 0);
   free(parent);
   parent = path_parent_dir_alloc("/tmp/file.txt");
   invariant(strcmp(parent, "/tmp") == 0);
   free(parent);
   parent = path_parent_dir_alloc("dir/sub/file.txt");
   invariant(strcmp(parent, "dir/sub") == 0);
   free(parent);
   path_require_parent_dir("/tmp/file.txt", "/tmp",
                           "file path parent mismatch");
   invariant(path_has_file_name("file.txt"));
   invariant(path_has_file_name("./file.txt"));
   invariant(path_has_file_name("/tmp/file.txt"));
   invariant(!path_has_file_name("."));
   invariant(!path_has_file_name(".."));
   invariant(!path_has_file_name("/"));
   invariant(!path_has_file_name("dir/"));
   invariant(!path_has_file_name("dir/."));
   invariant(!path_has_file_name("dir/.."));
   invariant(path_is_component("benchmarks"));
   invariant(path_is_component("AVLTopDown"));
   invariant(!path_is_component(NULL));
   invariant(!path_is_component(""));
   invariant(!path_is_component("."));
   invariant(!path_is_component(".."));
   invariant(!path_is_component("nested/name"));
   invariant(!path_is_component("nested\\name"));
   invariant(!path_is_component("two words"));
   invariant(!path_is_component("line\nbreak"));
   assert_filesystem_child_exited_2_with_stderr(
       test_path_parent_no_file_name_child, "file path has no file name");
   assert_filesystem_child_exited_2_with_stderr(
       test_path_require_parent_dir_mismatch_child,
       "file path parent mismatch");

   char file_path[512];
   path_format(file_path, sizeof(file_path), "%s/data.txt", nested);
   FILE *file = fopen_or_exit(file_path, "w");
   assert_file_close_on_exec(file, file_path);
   file_puts_or_exit("hello\n", file, file_path);
   file_putc_or_exit('!', file, file_path);
   fclose_or_exit(file, file_path);

   char file_data[8] = {0};
   file = fopen_or_exit(file_path, "r");
   file_read_or_exit(file_data, 7, file, file_path);
   file_read_or_exit(NULL, 0, file, file_path);
   fclose_or_exit(file, file_path);
   invariant(strcmp(file_data, "hello\n!") == 0);

   file = fopen_or_exit(file_path, "a");
   file_write_or_exit(NULL, 0, file, file_path);
   fclose_or_exit(file, file_path);

   assert_filesystem_child_exited_2(test_file_putc_negative_byte_child);
   assert_filesystem_child_exited_2(test_file_putc_large_byte_child);
   assert_filesystem_child_exited_2(test_file_write_null_buffer_child);
   assert_filesystem_child_exited_2(test_file_read_null_buffer_child);
   printf("  byte output contract OK\n");

   fflush(NULL);
   pid_t short_read_pid = fork();
   invariant(short_read_pid >= 0);
   if (short_read_pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      FILE *short_file = fopen_or_exit(file_path, "r");
      char too_much[8];
      file_read_or_exit(too_much, sizeof(too_much), short_file, file_path);
      _exit(0);
   }
   assert_child_exited_2(short_read_pid);

   char binary_path[512];
   path_format(binary_path, sizeof(binary_path), "%s/binary.dat", nested);
   static const unsigned char binary_data[] = {0, 1, 2, 253, 254, 255};
   file = fopen_or_exit(binary_path, "wb");
   file_write_or_exit(binary_data, sizeof(binary_data), file, binary_path);
   fclose_or_exit(file, binary_path);

   unsigned char binary_copy[sizeof(binary_data)] = {0};
   file = fopen_or_exit(binary_path, "rb");
   file_read_or_exit(binary_copy, sizeof(binary_copy), file, binary_path);
   fclose_or_exit(file, binary_path);
   invariant(memcmp(binary_data, binary_copy, sizeof(binary_data)) == 0);

   char rename_tmp[512];
   char rename_final[512];
   path_format(rename_tmp, sizeof(rename_tmp), "%s/publish.txt.tmp", nested);
   path_format(rename_final, sizeof(rename_final), "%s/publish.txt", nested);
   file = fopen_or_exit(rename_final, "w");
   file_puts_or_exit("old", file, rename_final);
   fclose_or_exit(file, rename_final);
   file = fopen_or_exit(rename_tmp, "w");
   file_puts_or_exit("new", file, rename_tmp);
   file_sync_or_exit(file, rename_tmp);
   fclose_or_exit(file, rename_tmp);
   path_rename_or_exit(rename_tmp, rename_final);
   assert_path_absent(rename_tmp);

   char renamed_data[4] = {0};
   file = fopen_or_exit(rename_final, "r");
   file_read_or_exit(renamed_data, 3, file, rename_final);
   invariant(fgetc(file) == EOF);
   fclose_or_exit(file, rename_final);
   invariant(strcmp(renamed_data, "new") == 0);

   path_format(test_path_rename_missing_old,
               sizeof(test_path_rename_missing_old),
               "%s/missing-rename-source.txt", nested);
   path_format(test_path_rename_missing_new,
               sizeof(test_path_rename_missing_new),
               "%s/missing-rename-target.txt", nested);
   char expected_rename_error[1200];
   path_format(expected_rename_error, sizeof(expected_rename_error),
               "rename %s -> %s:", test_path_rename_missing_old,
               test_path_rename_missing_new);
   assert_filesystem_child_exited_2_with_stderr(
       test_path_rename_missing_old_child, expected_rename_error);
   assert_path_absent(test_path_rename_missing_new);

   char publish_final[512];
   path_format(publish_final, sizeof(publish_final), "%s/publish-helper.txt",
               nested);
   char *publish_tmp = NULL;
   file = fopen_temporary_or_exit(&publish_tmp, publish_final, "w");
   assert_file_close_on_exec(file, publish_tmp);
   file_puts_or_exit("published", file, publish_tmp);
   file_publish_or_exit(file, publish_tmp, publish_final);
   assert_path_absent(publish_tmp);
   free(publish_tmp);

   char published_data[10] = {0};
   file = fopen_or_exit(publish_final, "r");
   file_read_or_exit(published_data, 9, file, publish_final);
   invariant(fgetc(file) == EOF);
   fclose_or_exit(file, publish_final);
   invariant(strcmp(published_data, "published") == 0);

   path_format(test_file_publish_mismatched_handle_tmp,
               sizeof(test_file_publish_mismatched_handle_tmp),
               "%s/publish-mismatched-handle.tmp", nested);
   path_format(test_file_publish_mismatched_handle_path,
               sizeof(test_file_publish_mismatched_handle_path),
               "%s/publish-mismatched-handle.txt", nested);
   path_format(test_file_publish_mismatched_handle_other,
               sizeof(test_file_publish_mismatched_handle_other),
               "%s/publish-mismatched-handle-other.txt", nested);
   assert_filesystem_child_exited_2_with_stderr(
       test_file_publish_mismatched_handle_child,
       "file handle does not match path");
   assert_path_absent(test_file_publish_mismatched_handle_path);
   invariant(remove(test_file_publish_mismatched_handle_tmp) == 0);
   invariant(remove(test_file_publish_mismatched_handle_other) == 0);

   char publish_wrong_parent_dir[512];
   path_format(publish_wrong_parent_dir, sizeof(publish_wrong_parent_dir),
               "%s/publish-wrong-parent", nested);
   mkdir_p(publish_wrong_parent_dir);
   path_format(test_file_publish_wrong_parent_tmp,
               sizeof(test_file_publish_wrong_parent_tmp),
               "%s/publish-wrong-parent.tmp", publish_wrong_parent_dir);
   path_format(test_file_publish_wrong_parent_path,
               sizeof(test_file_publish_wrong_parent_path),
               "%s/publish-wrong-parent.txt", nested);
   assert_filesystem_child_exited_2_with_stderr(
       test_file_publish_wrong_parent_child,
       "temporary file path is not in publish directory");
   assert_path_absent(test_file_publish_wrong_parent_path);
   invariant(remove(test_file_publish_wrong_parent_tmp) == 0);
   invariant(rmdir(publish_wrong_parent_dir) == 0);

   assert_filesystem_child_exited_2_with_stderr(
       test_file_publish_empty_tmp_path_child, "temporary file path is empty");
   assert_filesystem_child_exited_2_with_stderr(
       test_file_publish_empty_path_child, "publish path is empty");
   assert_filesystem_child_exited_2_with_stderr(
       test_file_publish_no_tmp_file_name_child,
       "temporary file path has no file name");
   assert_filesystem_child_exited_2_with_stderr(
       test_file_publish_no_path_file_name_child,
       "publish path has no file name");
   assert_filesystem_child_exited_2_with_stderr(
       test_file_publish_same_path_child, "temporary file path matches target");

   staged_file_t staged = {
       .path = path_format_alloc("%s/staged-helper.txt", nested),
   };
   file = staged_file_open_or_exit(&staged, "w");
   invariant(!staged.finished);
   file_puts_or_exit("staged", file, staged.tmp_path);
   staged_file_finish_or_exit(&staged, file);
   invariant(staged.finished);
   assert_path_absent(staged.path);
   invariant(access(staged.tmp_path, F_OK) == 0);
   char *staged_tmp_path = path_format_alloc("%s", staged.tmp_path);
   staged_file_publish_or_exit(&staged);
   invariant(staged.tmp_path == NULL);
   invariant(!staged.finished);
   assert_path_absent(staged_tmp_path);
   free(staged_tmp_path);

   char staged_data[7] = {0};
   file = fopen_or_exit(staged.path, "r");
   file_read_or_exit(staged_data, 6, file, staged.path);
   invariant(fgetc(file) == EOF);
   fclose_or_exit(file, staged.path);
   invariant(strcmp(staged_data, "staged") == 0);
   invariant(remove(staged.path) == 0);
   staged_file_dispose(&staged);
   invariant(staged.path == NULL);
   invariant(staged.tmp_path == NULL);
   invariant(!staged.finished);
   staged_file_dispose(&staged);

   path_format(test_staged_file_finish_mismatched_handle_path,
               sizeof(test_staged_file_finish_mismatched_handle_path),
               "%s/staged-mismatched-handle.txt", nested);
   path_format(test_staged_file_finish_mismatched_handle_other,
               sizeof(test_staged_file_finish_mismatched_handle_other),
               "%s/staged-mismatched-handle-other.txt", nested);
   pid_t staged_mismatch_pid = assert_filesystem_child_exited_2_with_stderr_pid(
       test_staged_file_finish_mismatched_handle_child,
       "file handle does not match path");
   char staged_mismatch_tmp[512];
   test_path_temporary_attempt(staged_mismatch_tmp, sizeof(staged_mismatch_tmp),
                               test_staged_file_finish_mismatched_handle_path,
                               staged_mismatch_pid, 0);
   assert_path_absent(test_staged_file_finish_mismatched_handle_path);
   invariant(remove(staged_mismatch_tmp) == 0);
   invariant(remove(test_staged_file_finish_mismatched_handle_other) == 0);

   assert_filesystem_child_exited_2_with_stderr(
       test_staged_file_publish_empty_tmp_path_child,
       "staged temporary file path is empty");
   assert_filesystem_child_exited_2_with_stderr(
       test_staged_file_publish_no_tmp_file_name_child,
       "staged temporary file path has no file name");
   assert_filesystem_child_exited_2_with_stderr(
       test_staged_file_publish_same_path_child,
       "staged temporary file path matches target");
   path_format(test_staged_file_dispose_same_path,
               sizeof(test_staged_file_dispose_same_path),
               "%s/staged-dispose-same-path.txt", nested);
   file = fopen_or_exit(test_staged_file_dispose_same_path, "w");
   file_puts_or_exit("keep", file, test_staged_file_dispose_same_path);
   fclose_or_exit(file, test_staged_file_dispose_same_path);
   assert_filesystem_child_exited_2_with_stderr(
       test_staged_file_dispose_same_path_child,
       "staged temporary file path matches target");
   char dispose_same_path_data[5] = {0};
   file = fopen_or_exit(test_staged_file_dispose_same_path, "r");
   file_read_or_exit(dispose_same_path_data, 4, file,
                     test_staged_file_dispose_same_path);
   invariant(fgetc(file) == EOF);
   fclose_or_exit(file, test_staged_file_dispose_same_path);
   invariant(strcmp(dispose_same_path_data, "keep") == 0);
   invariant(remove(test_staged_file_dispose_same_path) == 0);
   test_staged_file_dispose_same_path[0] = '\0';
   path_format(test_staged_file_publish_wrong_parent_tmp,
               sizeof(test_staged_file_publish_wrong_parent_tmp),
               "%s/staged-wrong-parent.tmp", publish_wrong_parent_dir);
   path_format(test_staged_file_publish_wrong_parent_path,
               sizeof(test_staged_file_publish_wrong_parent_path),
               "%s/staged-wrong-parent.txt", nested);
   mkdir_p(publish_wrong_parent_dir);
   assert_filesystem_child_exited_2_with_stderr(
       test_staged_file_publish_wrong_parent_child,
       "staged temporary file path is not in target directory");
   assert_path_absent(test_staged_file_publish_wrong_parent_path);
   invariant(rmdir(publish_wrong_parent_dir) == 0);
   assert_filesystem_child_exited_2_with_stderr(
       test_staged_file_finished_missing_tmp_path_child,
       "finished staged file has no temporary path");
   assert_filesystem_child_exited_2_with_stderr(
       test_staged_file_open_no_file_name_child,
       "staged file path has no file name");
   assert_filesystem_child_exited_2_with_stderr(test_fopen_empty_path_child,
                                                "file path is empty");
   assert_filesystem_child_exited_2_with_stderr(test_fopen_empty_mode_child,
                                                "file mode is empty");
   assert_filesystem_child_exited_2_with_stderr(
       test_fopen_temporary_empty_mode_child, "temporary file mode is empty");
   assert_filesystem_child_exited_2_with_stderr(
       test_fopen_temporary_null_mode_child, "temporary file mode is null");
   assert_filesystem_child_exited_2_with_stderr(
       test_fopen_temporary_no_file_name_child, "file path has no file name");
   assert_filesystem_child_exited_2_with_stderr(
       test_fopen_temporary_nonempty_output_child,
       "temporary path output is not empty");
   printf("  publish helper preflight diagnostics OK\n");

   char discard_final[512];
   path_format(discard_final, sizeof(discard_final), "%s/staged-discard.txt",
               nested);
   staged_file_t discarded = {
       .path = path_format_alloc("%s", discard_final),
   };
   file = staged_file_open_or_exit(&discarded, "w");
   file_puts_or_exit("discarded", file, discarded.tmp_path);
   staged_file_finish_or_exit(&discarded, file);
   char *discard_tmp_path = path_format_alloc("%s", discarded.tmp_path);
   staged_file_dispose(&discarded);
   invariant(discarded.path == NULL);
   invariant(discarded.tmp_path == NULL);
   invariant(!discarded.finished);
   assert_path_absent(discard_final);
   assert_path_absent(discard_tmp_path);
   free(discard_tmp_path);
   staged_file_dispose(NULL);

   staged_file_t unpublished = {
       .path = path_format_alloc("%s/staged-unpublished.txt", nested),
   };
   fflush(NULL);
   pid_t unpublished_pid = fork();
   invariant(unpublished_pid >= 0);
   if (unpublished_pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      staged_file_publish_or_exit(&unpublished);
      _exit(0);
   }
   assert_child_exited_2(unpublished_pid);
   staged_file_dispose(&unpublished);

   char staged_unfinished_final[512];
   path_format(staged_unfinished_final, sizeof(staged_unfinished_final),
               "%s/staged-unfinished-publish.txt", nested);
   fflush(NULL);
   pid_t staged_unfinished_pid = fork();
   invariant(staged_unfinished_pid >= 0);
   if (staged_unfinished_pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      staged_file_t unfinished = {
          .path = path_format_alloc("%s", staged_unfinished_final),
      };
      FILE *unfinished_file = staged_file_open_or_exit(&unfinished, "w");
      file_puts_or_exit("unfinished", unfinished_file, unfinished.tmp_path);
      staged_file_publish_or_exit(&unfinished);
      _exit(0);
   }
   assert_child_exited_2(staged_unfinished_pid);

   char staged_unfinished_tmp[512];
   test_path_temporary_attempt(
       staged_unfinished_tmp, sizeof(staged_unfinished_tmp),
       staged_unfinished_final, staged_unfinished_pid, 0);
   assert_path_absent(staged_unfinished_final);
   invariant(remove(staged_unfinished_tmp) == 0);

   char staged_double_final[512];
   path_format(staged_double_final, sizeof(staged_double_final),
               "%s/staged-double-open.txt", nested);
   fflush(NULL);
   pid_t staged_double_pid = fork();
   invariant(staged_double_pid >= 0);
   if (staged_double_pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      staged_file_t double_open = {
          .path = path_format_alloc("%s", staged_double_final),
      };
      FILE *first = staged_file_open_or_exit(&double_open, "w");
      (void)first;
      FILE *second = staged_file_open_or_exit(&double_open, "w");
      (void)second;
      _exit(0);
   }
   assert_child_exited_2(staged_double_pid);

   char staged_double_tmp[512];
   test_path_temporary_attempt(staged_double_tmp, sizeof(staged_double_tmp),
                               staged_double_final, staged_double_pid, 0);
   assert_path_absent(staged_double_final);
   invariant(remove(staged_double_tmp) == 0);

   char invalid_mode_final[512];
   path_format(invalid_mode_final, sizeof(invalid_mode_final),
               "%s/invalid-mode.txt", nested);
   fflush(NULL);
   pid_t invalid_mode_pid = fork();
   invariant(invalid_mode_pid >= 0);
   if (invalid_mode_pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      char *invalid_mode_tmp = (char *)1;
      FILE *invalid_mode_file =
          fopen_temporary_or_exit(&invalid_mode_tmp, invalid_mode_final, "r");
      (void)invalid_mode_file;
      _exit(invalid_mode_tmp == NULL ? 0 : 1);
   }
   assert_child_exited_2(invalid_mode_pid);

   char invalid_mode_tmp[512];
   test_path_temporary_attempt(invalid_mode_tmp, sizeof(invalid_mode_tmp),
                               invalid_mode_final, invalid_mode_pid, 0);
   assert_path_absent(invalid_mode_final);
   assert_path_absent(invalid_mode_tmp);
   invariant(!fopen_temporary_mode_is_supported(NULL));
   invariant(!fopen_temporary_mode_is_supported(""));
   invariant(!fopen_temporary_mode_is_supported("r"));
   invariant(fopen_temporary_mode_is_supported("w"));
   invariant(fopen_temporary_mode_is_supported("wb"));

   char collision_final[512];
   path_format(collision_final, sizeof(collision_final), "%s/collision.txt",
               nested);
   char collision_stale_tmp[512];
   test_path_temporary_attempt(collision_stale_tmp, sizeof(collision_stale_tmp),
                               collision_final, getpid(), 0);
   file = fopen_or_exit(collision_stale_tmp, "w");
   file_puts_or_exit("stale", file, collision_stale_tmp);
   fclose_or_exit(file, collision_stale_tmp);

   char *collision_tmp = NULL;
   file = fopen_temporary_or_exit(&collision_tmp, collision_final, "w");
   char collision_expected_tmp[512];
   test_path_temporary_attempt(collision_expected_tmp,
                               sizeof(collision_expected_tmp), collision_final,
                               getpid(), 1);
   invariant(strcmp(collision_tmp, collision_expected_tmp) == 0);
   file_puts_or_exit("fresh", file, collision_tmp);
   file_publish_or_exit(file, collision_tmp, collision_final);
   assert_path_absent(collision_tmp);
   free(collision_tmp);

   char collision_data[6] = {0};
   file = fopen_or_exit(collision_final, "r");
   file_read_or_exit(collision_data, 5, file, collision_final);
   invariant(fgetc(file) == EOF);
   fclose_or_exit(file, collision_final);
   invariant(strcmp(collision_data, "fresh") == 0);

   char stale_data[6] = {0};
   file = fopen_or_exit(collision_stale_tmp, "r");
   file_read_or_exit(stale_data, 5, file, collision_stale_tmp);
   invariant(fgetc(file) == EOF);
   fclose_or_exit(file, collision_stale_tmp);
   invariant(strcmp(stale_data, "stale") == 0);

   char exhausted_final[512];
   path_format(exhausted_final, sizeof(exhausted_final), "%s/exhausted.txt",
               nested);
   int exhausted_gate[2];
   invariant(pipe(exhausted_gate) == 0);
   fflush(NULL);
   pid_t exhausted_pid = fork();
   invariant(exhausted_pid >= 0);
   if (exhausted_pid == 0) {
      invariant(close(exhausted_gate[1]) == 0);
      char token;
      invariant(read(exhausted_gate[0], &token, 1) == 1);
      invariant(close(exhausted_gate[0]) == 0);
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      char *exhausted_tmp = NULL;
      FILE *exhausted_file =
          fopen_temporary_or_exit(&exhausted_tmp, exhausted_final, "w");
      (void)exhausted_file;
      _exit(0);
   }
   invariant(close(exhausted_gate[0]) == 0);
   for (unsigned int attempt = 0; attempt < FOPEN_TEMPORARY_MAX_ATTEMPTS;
        attempt++) {
      char tmp_path[512];
      test_path_temporary_attempt(tmp_path, sizeof(tmp_path), exhausted_final,
                                  exhausted_pid, attempt);
      file = fopen_or_exit(tmp_path, "w");
      file_printf_or_exit(file, tmp_path, "%u\n", attempt);
      fclose_or_exit(file, tmp_path);
   }
   invariant(write(exhausted_gate[1], "x", 1) == 1);
   invariant(close(exhausted_gate[1]) == 0);
   assert_child_exited_2(exhausted_pid);
   assert_path_absent(exhausted_final);
   for (unsigned int attempt = 0; attempt < FOPEN_TEMPORARY_MAX_ATTEMPTS;
        attempt++) {
      char tmp_path[512];
      test_path_temporary_attempt(tmp_path, sizeof(tmp_path), exhausted_final,
                                  exhausted_pid, attempt);
      invariant(remove(tmp_path) == 0);
   }

   char trunc_stderr[512];
   path_format(trunc_stderr, sizeof(trunc_stderr), "%s/trunc-stderr.txt",
               created_dir);
   fflush(NULL);
   {
      pid_t pid = fork();
      invariant(pid >= 0);
      if (pid == 0) {
         FILE *err = freopen(trunc_stderr, "w", stderr);
         invariant(err != NULL);
         char tiny[4];
         path_format(tiny, sizeof(tiny), "%s", "abcdef");
         _exit(0);
      }
      assert_child_exited_2(pid);
   }

   invariant(remove(file_path) == 0);
   invariant(remove(binary_path) == 0);
   invariant(remove(rename_final) == 0);
   invariant(remove(publish_final) == 0);
   invariant(remove(collision_final) == 0);
   invariant(remove(collision_stale_tmp) == 0);
   invariant(remove(trunc_stderr) == 0);
   invariant(rmdir(nested) == 0);
   char dir[512];
   path_format(dir, sizeof(dir), "%s/a/b", created_dir);
   invariant(rmdir(dir) == 0);
   path_format(dir, sizeof(dir), "%s/a", created_dir);
   invariant(rmdir(dir) == 0);
   invariant(rmdir(created_dir) == 0);
   printf("  nested directories, path formatting, and file I/O OK\n");

   char symlink_dir[] = "/tmp/bst-filesystem-link-XXXXXX";
   created_dir = mkdtemp(symlink_dir);
   invariant(created_dir != NULL);

   char target_path[512];
   char link_path[512];
   char stderr_path[512];
   path_format(target_path, sizeof(target_path), "%s/missing-target",
               created_dir);
   path_format(link_path, sizeof(link_path), "%s/link", created_dir);
   path_format(stderr_path, sizeof(stderr_path), "%s/stderr.txt", created_dir);
   invariant(symlink(target_path, link_path) == 0);

   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen(stderr_path, "w", stderr);
      invariant(err != NULL);
      mkdir_p(link_path);
      _exit(0);
   }

   assert_child_exited_2(pid);

   char message[512] = {0};
   file = fopen_or_exit(stderr_path, "r");
   invariant(test_fgets_or_abort(message, sizeof(message), file, stderr_path));
   fclose_or_exit(file, stderr_path);
   invariant(strstr(message, strerror(EEXIST)) != NULL);

   invariant(remove(stderr_path) == 0);
   invariant(remove(link_path) == 0);
   invariant(rmdir(created_dir) == 0);
   printf("  mkdir diagnostics preserve errno OK\n");
}

static void test_write_text_file(const char *path, const char *text) {
   invariant(path != NULL);
   invariant(text != NULL);

   FILE *file = fopen_or_exit(path, "w");
   file_puts_or_exit(text, file, path);
   fclose_or_exit(file, path);
}

static char *test_read_text_file(const char *path) {
   invariant(path != NULL);

   FILE *file = fopen_or_exit(path, "rb");
   invariant(fseek(file, 0, SEEK_END) == 0);
   long length = ftell(file);
   invariant(length >= 0);
   invariant(fseek(file, 0, SEEK_SET) == 0);

   size_t len = (size_t)length;
   invariant((long)len == length);
   invariant(len < SIZE_MAX);
   char *content = (char *)bst_malloc(len + 1);
   file_read_or_exit(content, len, file, path);
   fclose_or_exit(file, path);
   content[len] = '\0';
   return content;
}

static const char *test_require_substring(const char *text,
                                          const char *needle) {
   invariant(text != NULL);
   invariant(needle != NULL);
   const char *found = strstr(text, needle);
   if (found == NULL) {
      fprintf(stderr, "missing substring: %s\n", needle);
      abort();
   }
   return found;
}

static void test_forbid_substring(const char *name, const char *text,
                                  const char *needle) {
   invariant(name != NULL);
   invariant(text != NULL);
   invariant(needle != NULL);

   if (strstr(text, needle) != NULL) {
      fprintf(stderr, "%s contains forbidden substring: %s\n", name, needle);
      abort();
   }
}

static void
assert_benchmark_index_template_bytes_exits_2(const void *template_data,
                                              size_t template_size) {
   invariant(template_size == 0 || template_data != NULL);

   char output_dir[] = "/tmp/bst-benchmark-index-invalid-XXXXXX";
   char *root = mkdtemp(output_dir);
   invariant(root != NULL);

   char template_path[512];
   path_format(template_path, sizeof(template_path), "%s/template.html", root);
   FILE *file = fopen_or_exit(template_path, "wb");
   file_write_or_exit(template_data, template_size, file, template_path);
   fclose_or_exit(file, template_path);

   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      benchmark_index_run(template_path, root);
      _exit(0);
   }
   assert_child_exited_2(pid);

   char root_index[512];
   path_format(root_index, sizeof(root_index), "%s/index.html", root);
   assert_path_absent(root_index);
   assert_temporary_path_absent_for_pid(root_index, pid);
   invariant(remove(template_path) == 0);
   invariant(rmdir(root) == 0);
}

static void assert_benchmark_index_template_exits_2(const char *template_text) {
   invariant(template_text != NULL);
   assert_benchmark_index_template_bytes_exits_2(template_text,
                                                 strlen(template_text));
}

static void assert_benchmark_index_template_path_exits_2(void) {
   char output_dir[] = "/tmp/bst-benchmark-index-invalid-path-XXXXXX";
   char *root = mkdtemp(output_dir);
   invariant(root != NULL);

   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      benchmark_index_run(root, root);
      _exit(0);
   }
   assert_child_exited_2(pid);

   char root_index[512];
   path_format(root_index, sizeof(root_index), "%s/index.html", root);
   assert_path_absent(root_index);
   assert_temporary_path_absent_for_pid(root_index, pid);
   invariant(rmdir(root) == 0);
}

static void assert_benchmark_index_inline_bytes_exits_2(
    const void *inline_data, size_t inline_size, const char *expected_stderr) {
   invariant(inline_size == 0 || inline_data != NULL);
   invariant(expected_stderr != NULL);

   char output_dir[] = "/tmp/bst-benchmark-index-invalid-inline-XXXXXX";
   char *root = mkdtemp(output_dir);
   invariant(root != NULL);

   char template_path[512];
   char inline_path[512];
   char stderr_path[512];
   path_format(template_path, sizeof(template_path), "%s/template.html", root);
   path_format(inline_path, sizeof(inline_path), "%s/asset.svg", root);
   path_format(stderr_path, sizeof(stderr_path), "%s/stderr.txt", root);

   test_write_text_file(template_path, "{{ range $_, $file := .files }}\n"
                                       "{{ inline $file }}\n"
                                       "{{ end }}\n");

   FILE *file = fopen_or_exit(inline_path, "wb");
   file_write_or_exit(inline_data, inline_size, file, inline_path);
   fclose_or_exit(file, inline_path);

   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen(stderr_path, "w", stderr);
      invariant(err != NULL);
      benchmark_index_run(template_path, root);
      _exit(0);
   }
   assert_child_exited_2(pid);

   char *stderr_text = test_read_text_file(stderr_path);
   test_require_substring(stderr_text, expected_stderr);
   free(stderr_text);

   char root_index[512];
   path_format(root_index, sizeof(root_index), "%s/index.html", root);
   assert_path_absent(root_index);
   assert_temporary_path_absent_for_pid(root_index, pid);

   invariant(remove(stderr_path) == 0);
   invariant(remove(inline_path) == 0);
   invariant(remove(template_path) == 0);
   invariant(rmdir(root) == 0);
}

static void assert_benchmark_index_preflights_nested_inline_failure(void) {
   char output_dir[] = "/tmp/bst-benchmark-index-preflight-XXXXXX";
   char *root = mkdtemp(output_dir);
   invariant(root != NULL);

   char template_path[512];
   char root_index[512];
   char good_dir[512];
   char bad_dir[512];
   char good_svg[512];
   char bad_svg[512];
   char good_index[512];
   char bad_index[512];
   char stderr_path[512];
   path_format(template_path, sizeof(template_path), "%s/template.html", root);
   path_format(root_index, sizeof(root_index), "%s/index.html", root);
   path_format(good_dir, sizeof(good_dir), "%s/a-good", root);
   path_format(bad_dir, sizeof(bad_dir), "%s/z-bad", root);
   path_format(good_svg, sizeof(good_svg), "%s/good.svg", good_dir);
   path_format(bad_svg, sizeof(bad_svg), "%s/bad.svg", bad_dir);
   path_format(good_index, sizeof(good_index), "%s/index.html", good_dir);
   path_format(bad_index, sizeof(bad_index), "%s/index.html", bad_dir);
   path_format(stderr_path, sizeof(stderr_path), "%s/stderr.txt", root);

   test_write_text_file(template_path, "{{ range $_, $file := .files }}\n"
                                       "{{ inline $file }}\n"
                                       "{{ end }}\n");
   mkdir_p(good_dir);
   mkdir_p(bad_dir);
   test_write_text_file(good_svg, "<svg id=\"good\"></svg>\n");
   static const unsigned char invalid_svg[] = {
       '<', 's', 'v', 'g', '>', 0, '<', '/', 's', 'v', 'g', '>', '\n',
   };
   FILE *file = fopen_or_exit(bad_svg, "wb");
   file_write_or_exit(invalid_svg, sizeof(invalid_svg), file, bad_svg);
   fclose_or_exit(file, bad_svg);

   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen(stderr_path, "w", stderr);
      invariant(err != NULL);
      benchmark_index_run(template_path, root);
      _exit(0);
   }
   assert_child_exited_2(pid);

   char *stderr_text = test_read_text_file(stderr_path);
   test_require_substring(stderr_text, "inline file contains NUL byte");
   free(stderr_text);

   assert_path_absent(root_index);
   assert_path_absent(good_index);
   assert_path_absent(bad_index);
   assert_temporary_path_absent_for_pid(root_index, pid);
   assert_temporary_path_absent_for_pid(good_index, pid);
   assert_temporary_path_absent_for_pid(bad_index, pid);

   invariant(remove(stderr_path) == 0);
   invariant(remove(good_svg) == 0);
   invariant(remove(bad_svg) == 0);
   invariant(rmdir(good_dir) == 0);
   invariant(rmdir(bad_dir) == 0);
   invariant(remove(template_path) == 0);
   invariant(rmdir(root) == 0);
}

static void assert_benchmark_index_run_exits_2(const char *template_path,
                                               const char *root_path,
                                               const char *expected_stderr) {
   invariant(expected_stderr != NULL);

   char stderr_path[] = "/tmp/bst-benchmark-index-stderr-XXXXXX";
   int stderr_fd = mkstemp(stderr_path);
   invariant(stderr_fd >= 0);
   invariant(close(stderr_fd) == 0);

   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen(stderr_path, "w", stderr);
      invariant(err != NULL);
      benchmark_index_run(template_path, root_path);
      _exit(0);
   }
   assert_child_exited_2(pid);

   char *stderr_text = test_read_text_file(stderr_path);
   test_require_substring(stderr_text, expected_stderr);
   free(stderr_text);
   invariant(remove(stderr_path) == 0);
}

static bool test_data_field_separator(char ch) {
   return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' ||
          ch == '\f';
}

enum {
   TEST_DATA_LINE_MAX = 2048,
};

static int test_data_field_count(const char *line) {
   invariant(line != NULL);
   int count = 0;
   bool in_field = false;
   for (const char *p = line; *p != '\0'; p++) {
      if (test_data_field_separator(*p)) {
         in_field = false;
      } else if (!in_field) {
         count++;
         in_field = true;
      }
   }
   return count;
}

static void test_trim_line_end(char *line) {
   invariant(line != NULL);
   size_t len = strlen(line);
   while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
      line[--len] = '\0';
   }
}

static void test_data_file_require_complete_line(FILE *file, const char *path,
                                                 const char *line) {
   invariant(file != NULL);
   invariant(path != NULL);
   invariant(line != NULL);

   if (strchr(line, '\n') != NULL) {
      return;
   }
   if (!feof(file)) {
      fprintf(stderr, "line too long in %s\n", path);
      abort();
   }
   fprintf(stderr, "unterminated line in %s\n", path);
   abort();
}

static void assert_data_file_schema_one_of(const char *path,
                                           const char *const *expected_headers,
                                           int expected_header_count) {
   invariant(path != NULL);
   invariant(expected_headers != NULL);
   invariant(expected_header_count > 0);

   FILE *raw = fopen_or_exit(path, "rb");
   unsigned char bytes[4096];
   for (;;) {
      size_t n = fread(bytes, 1, sizeof(bytes), raw);
      if (n > 0 && memchr(bytes, '\0', n) != NULL) {
         fprintf(stderr, "data file contains NUL byte in %s\n", path);
         abort();
      }
      if (n < sizeof(bytes)) {
         if (ferror(raw)) {
            perror(path);
            abort();
         }
         break;
      }
   }
   fclose_or_exit(raw, path);

   FILE *file = fopen_or_exit(path, "r");
   char line[TEST_DATA_LINE_MAX];
   invariant(test_fgets_or_abort(line, sizeof(line), file, path));
   test_data_file_require_complete_line(file, path, line);
   test_trim_line_end(line);

   const char *matched_header = NULL;
   for (int i = 0; i < expected_header_count; i++) {
      invariant(expected_headers[i] != NULL);
      if (strcmp(line, expected_headers[i]) == 0) {
         matched_header = expected_headers[i];
         break;
      }
   }
   if (matched_header == NULL) {
      fprintf(stderr, "unexpected header in %s: %s\n", path, line);
      abort();
   }

   int expected_fields = test_data_field_count(matched_header);
   invariant(expected_fields > 0);

   u64 rows = 0;
   while (test_fgets_or_abort(line, sizeof(line), file, path)) {
      rows++;
      test_data_file_require_complete_line(file, path, line);
      int fields = test_data_field_count(line);
      if (fields == 0) {
         fprintf(stderr, "blank row in %s row %llu\n", path,
                 (unsigned long long)rows);
         abort();
      }
      if (fields != expected_fields) {
         fprintf(stderr,
                 "unexpected field count in %s row %llu: expected %d, got %d\n",
                 path, (unsigned long long)rows, expected_fields, fields);
         abort();
      }
   }
   if (rows == 0) {
      fprintf(stderr, "missing data rows in %s\n", path);
      abort();
   }
   fclose_or_exit(file, path);
}

static void assert_data_file_schema(const char *path,
                                    const char *expected_header) {
   const char *headers[] = {expected_header};
   assert_data_file_schema_one_of(path, headers, 1);
}

static void assert_data_file_schema_aborts_for_bytes(const void *content,
                                                     size_t size) {
   invariant(size == 0 || content != NULL);

   char path[] = "/tmp/bst-data-schema-invalid-XXXXXX";
   int fd = mkstemp(path);
   invariant(fd >= 0);
   invariant(close(fd) == 0);
   FILE *file = fopen_or_exit(path, "wb");
   file_write_or_exit(content, size, file, path);
   fclose_or_exit(file, path);

   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      assert_data_file_schema(path,
                              BENCHMARK_SCHEMA_OPERATION_BENCHMARK_HEADER);
      _exit(0);
   }

   assert_child_aborted(pid);
   invariant(remove(path) == 0);
}

static void assert_data_file_schema_aborts_for_content(const char *content) {
   invariant(content != NULL);
   assert_data_file_schema_aborts_for_bytes(content, strlen(content));
}

static void assert_data_file_schema_rejects_empty_rows(void) {
   assert_data_file_schema_aborts_for_content(
       BENCHMARK_SCHEMA_OPERATION_BENCHMARK_HEADER "\n");
   assert_data_file_schema_aborts_for_content(
       BENCHMARK_SCHEMA_OPERATION_BENCHMARK_HEADER "\n"
                                                   "\n");
}

static void assert_data_file_schema_rejects_long_header(void) {
   char content[2100];
   memset(content, 'A', sizeof(content));
   content[sizeof(content) - 2] = '\n';
   content[sizeof(content) - 1] = '\0';
   assert_data_file_schema_aborts_for_content(content);
}

static void assert_data_file_schema_rejects_bad_row_arity(void) {
   assert_data_file_schema_aborts_for_content(
       BENCHMARK_SCHEMA_OPERATION_BENCHMARK_HEADER "\n"
                                                   "Uniform 1 2 3 4 5 6\n");
   assert_data_file_schema_aborts_for_content(
       BENCHMARK_SCHEMA_OPERATION_BENCHMARK_HEADER "\n"
                                                   "Uniform 1 2 3 4 5 6 7 8\n");
}

static void assert_data_file_schema_rejects_long_data_row(void) {
   const char *header = BENCHMARK_SCHEMA_OPERATION_BENCHMARK_HEADER;
   char content[TEST_DATA_LINE_MAX + 200];
   size_t offset = strlen(header);
   size_t data_len = TEST_DATA_LINE_MAX + 52;
   invariant(offset + data_len + 3 <= sizeof(content));

   memcpy(content, header, offset);
   content[offset++] = '\n';
   memset(content + offset, '1', data_len);
   offset += data_len;
   content[offset++] = '\n';
   content[offset] = '\0';

   assert_data_file_schema_aborts_for_content(content);
}

static void assert_data_file_schema_rejects_unterminated_lines(void) {
   assert_data_file_schema_aborts_for_content(
       BENCHMARK_SCHEMA_OPERATION_BENCHMARK_HEADER);
   assert_data_file_schema_aborts_for_content(
       BENCHMARK_SCHEMA_OPERATION_BENCHMARK_HEADER "\n"
                                                   "Uniform 1 1 1 1 1 1 1");
}

static void assert_data_file_schema_rejects_nul_bytes(void) {
   static const unsigned char content[] =
       BENCHMARK_SCHEMA_OPERATION_BENCHMARK_HEADER "\n"
                                                   "Uniform 1 1 1 1 1 1 1\0";
   assert_data_file_schema_aborts_for_bytes(content, sizeof(content));
}

static void assert_benchmark_schema_field_counts(void) {
   assert_int_equal(
       BENCHMARK_SCHEMA_BALANCER_MEASUREMENT_FIELDS,
       test_data_field_count(BENCHMARK_SCHEMA_BALANCER_MEASUREMENT_HEADER));
   assert_int_equal(
       BENCHMARK_SCHEMA_BALANCER_BENCHMARK_FIELDS,
       test_data_field_count(BENCHMARK_SCHEMA_BALANCER_BENCHMARK_HEADER));
   assert_int_equal(
       BENCHMARK_SCHEMA_OPERATION_BENCHMARK_FIELDS,
       test_data_field_count(BENCHMARK_SCHEMA_OPERATION_BENCHMARK_HEADER));
   assert_int_equal(
       BENCHMARK_SCHEMA_OPERATION_MEASUREMENT_FIELDS,
       test_data_field_count(BENCHMARK_SCHEMA_OPERATION_MEASUREMENT_HEADER));
}

enum {
   TEST_DATA_ROW_MAX_FIELDS = 16,
   TEST_DATA_FIELD_MAX_LEN = 128,
};

typedef struct {
   const char *path;
   const char *line;
   char fields[TEST_DATA_ROW_MAX_FIELDS][TEST_DATA_FIELD_MAX_LEN];
   int field_count;
} test_data_row_t;

static void test_data_row_abort(const test_data_row_t *row, const char *field,
                                const char *reason) {
   invariant(row != NULL);
   invariant(row->path != NULL);
   invariant(row->line != NULL);
   invariant(field != NULL);
   invariant(reason != NULL);

   fprintf(stderr, "invalid %s field in %s (%s): %s", field, row->path, reason,
           row->line);
   size_t len = strlen(row->line);
   if (len == 0 || row->line[len - 1] != '\n') {
      fputc('\n', stderr);
   }
   abort();
}

static test_data_row_t test_data_row_parse(const char *path, const char *line,
                                           int expected_fields) {
   invariant(path != NULL);
   invariant(line != NULL);
   invariant(expected_fields > 0);
   invariant(expected_fields <= TEST_DATA_ROW_MAX_FIELDS);

   test_data_row_t row = {
       .path = path,
       .line = line,
   };
   const char *p = line;
   while (*p != '\0') {
      while (test_data_field_separator(*p)) {
         p++;
      }
      if (*p == '\0') {
         break;
      }

      if (row.field_count == TEST_DATA_ROW_MAX_FIELDS) {
         fprintf(stderr, "too many fields in %s: %s", path, line);
         abort();
      }

      const char *start = p;
      while (*p != '\0' && !test_data_field_separator(*p)) {
         p++;
      }
      size_t len = (size_t)(p - start);
      if (len == 0 || len >= TEST_DATA_FIELD_MAX_LEN) {
         fprintf(stderr, "invalid field width in %s: %s", path, line);
         abort();
      }

      memcpy(row.fields[row.field_count], start, len);
      row.fields[row.field_count][len] = '\0';
      row.field_count++;
   }

   if (row.field_count != expected_fields) {
      fprintf(stderr, "unexpected field count in %s: expected %d, got %d: %s",
              path, expected_fields, row.field_count, line);
      abort();
   }
   return row;
}

static const char *test_data_row_string(const test_data_row_t *row, int index,
                                        const char *field) {
   invariant(row != NULL);
   invariant(index >= 0);
   invariant(index < row->field_count);
   invariant(field != NULL);
   const char *value = row->fields[index];
   if (value[0] == '\0') {
      test_data_row_abort(row, field, "empty string");
   }
   return value;
}

static u64 test_data_row_u64(const test_data_row_t *row, int index,
                             const char *field) {
   const char *token = test_data_row_string(row, index, field);
   if (token[0] == '+' || token[0] == '-') {
      test_data_row_abort(row, field, "unexpected sign");
   }

   u64 value = 0;
   for (const char *p = token; *p != '\0'; p++) {
      if (*p < '0' || *p > '9') {
         test_data_row_abort(row, field, "malformed unsigned integer");
      }
      u64 digit = (u64)(*p - '0');
      if (value > (UINT64_MAX - digit) / 10) {
         test_data_row_abort(row, field, "integer overflow");
      }
      value = value * 10 + digit;
   }

   return value;
}

static int test_data_row_nonnegative_int(const test_data_row_t *row, int index,
                                         const char *field) {
   u64 value = test_data_row_u64(row, index, field);
   if (value > (u64)INT_MAX) {
      test_data_row_abort(row, field, "integer overflow");
   }
   return (int)value;
}

static int test_data_row_int(const test_data_row_t *row, int index,
                             const char *field) {
   const char *token = test_data_row_string(row, index, field);
   bool negative = token[0] == '-';
   if (token[0] == '+') {
      test_data_row_abort(row, field, "unexpected sign");
   }
   if (negative) {
      token++;
      if (token[0] == '\0') {
         test_data_row_abort(row, field, "malformed integer");
      }
   }

   u64 value = 0;
   for (const char *p = token; *p != '\0'; p++) {
      if (*p < '0' || *p > '9') {
         test_data_row_abort(row, field, "malformed integer");
      }
      u64 digit = (u64)(*p - '0');
      if (value > (UINT64_MAX - digit) / 10) {
         test_data_row_abort(row, field, "integer overflow");
      }
      value = value * 10 + digit;
   }

   if (negative) {
      if (value > (u64)INT_MAX + 1ULL) {
         test_data_row_abort(row, field, "integer overflow");
      }
      if (value == (u64)INT_MAX + 1ULL) {
         return INT_MIN;
      }
      return -(int)value;
   }
   if (value > (u64)INT_MAX) {
      test_data_row_abort(row, field, "integer overflow");
   }
   return (int)value;
}

static bool test_data_row_decimal_float_token(const char *token) {
   invariant(token != NULL);
   const char *p = token;
   bool integer_digits = false;
   bool fraction_digits = false;

   while (*p >= '0' && *p <= '9') {
      integer_digits = true;
      p++;
   }
   if (*p == '.') {
      p++;
      while (*p >= '0' && *p <= '9') {
         fraction_digits = true;
         p++;
      }
   }
   if (!integer_digits && !fraction_digits) {
      return false;
   }

   if (*p == 'e' || *p == 'E') {
      p++;
      if (*p == '+' || *p == '-') {
         p++;
      }
      bool exponent_digits = false;
      while (*p >= '0' && *p <= '9') {
         exponent_digits = true;
         p++;
      }
      if (!exponent_digits) {
         return false;
      }
   }

   return *p == '\0';
}

static double test_data_row_double(const test_data_row_t *row, int index,
                                   const char *field) {
   const char *token = test_data_row_string(row, index, field);
   if (token[0] == '+' || token[0] == '-') {
      test_data_row_abort(row, field, "unexpected sign");
   }
   if (!test_data_row_decimal_float_token(token)) {
      test_data_row_abort(row, field, "malformed floating point value");
   }

   errno = 0;
   char *end = NULL;
   double value = strtod(token, &end);
   if (end == token || *end != '\0' || errno == ERANGE || !isfinite(value)) {
      test_data_row_abort(row, field, "malformed floating point value");
   }
   return value;
}

static void test_data_row_malformed_u64_child(void) {
   test_data_row_t row = test_data_row_parse("synthetic", "Uniform 12x", 2);
   (void)test_data_row_u64(&row, 1, "Scale");
}

static void test_data_row_signed_u64_child(void) {
   test_data_row_t row = test_data_row_parse("synthetic", "Uniform -1", 2);
   (void)test_data_row_u64(&row, 1, "Scale");
}

static void test_data_row_plus_u64_child(void) {
   test_data_row_t row = test_data_row_parse("synthetic", "Uniform +1", 2);
   (void)test_data_row_u64(&row, 1, "Scale");
}

static void test_data_row_overflow_u64_child(void) {
   test_data_row_t row =
       test_data_row_parse("synthetic", "Uniform 18446744073709551616", 2);
   (void)test_data_row_u64(&row, 1, "Scale");
}

static void test_data_row_malformed_double_child(void) {
   test_data_row_t row = test_data_row_parse("synthetic", "Uniform 1.5x", 2);
   (void)test_data_row_double(&row, 1, "AveragePathLength");
}

static void test_data_row_nonfinite_double_child(void) {
   test_data_row_t row = test_data_row_parse("synthetic", "Uniform inf", 2);
   (void)test_data_row_double(&row, 1, "AveragePathLength");
}

static void test_data_row_hex_double_child(void) {
   test_data_row_t row = test_data_row_parse("synthetic", "Uniform 0x1p+2", 2);
   (void)test_data_row_double(&row, 1, "AveragePathLength");
}

static void test_data_row_overflow_int_child(void) {
   test_data_row_t row =
       test_data_row_parse("synthetic", "Uniform 2147483648", 2);
   (void)test_data_row_nonnegative_int(&row, 1, "MaximumPathLength");
}

static void test_data_row_plus_int_child(void) {
   test_data_row_t row = test_data_row_parse("synthetic", "Uniform +1", 2);
   (void)test_data_row_int(&row, 1, "MaximumPathLength");
}

static void test_data_row_positive_overflow_signed_int_child(void) {
   test_data_row_t row =
       test_data_row_parse("synthetic", "Uniform 2147483648", 2);
   (void)test_data_row_int(&row, 1, "MaximumPathLength");
}

static void test_data_row_overflow_signed_int_child(void) {
   test_data_row_t row =
       test_data_row_parse("synthetic", "Uniform -2147483649", 2);
   (void)test_data_row_int(&row, 1, "MaximumPathLength");
}

static void assert_benchmark_data_parser_aborts(void (*fn)(void)) {
   invariant(fn != NULL);

   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      fn();
      _exit(0);
   }
   assert_child_aborted(pid);
}

static void test_data_row_format_token(char *line, size_t line_size,
                                       const char *token) {
   invariant(line != NULL);
   invariant(line_size > 0);
   invariant(token != NULL);
   int len = snprintf(line, line_size, "Uniform %s", token);
   invariant(len > 0);
   invariant((size_t)len < line_size);
}

static void
assert_benchmark_data_parser_rejects_double_token(const char *token) {
   char line[128];
   test_data_row_format_token(line, sizeof(line), token);

   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      test_data_row_t row = test_data_row_parse("synthetic", line, 2);
      (void)test_data_row_double(&row, 1, "AveragePathLength");
      _exit(0);
   }
   assert_child_aborted(pid);
}

static void assert_benchmark_data_parser_accepts_double_token(const char *token,
                                                              double expected) {
   char line[128];
   test_data_row_format_token(line, sizeof(line), token);
   test_data_row_t row = test_data_row_parse("synthetic", line, 2);
   assert_double_equal(expected,
                       test_data_row_double(&row, 1, "AveragePathLength"));
}

typedef struct {
   const char *token;
   double expected;
} test_data_row_double_token_case_t;

static void assert_benchmark_data_parser_rejects_malformed_numbers(void) {
   assert_benchmark_data_parser_aborts(test_data_row_malformed_u64_child);
   assert_benchmark_data_parser_aborts(test_data_row_signed_u64_child);
   assert_benchmark_data_parser_aborts(test_data_row_plus_u64_child);
   assert_benchmark_data_parser_aborts(test_data_row_overflow_u64_child);
   assert_benchmark_data_parser_aborts(test_data_row_malformed_double_child);
   assert_benchmark_data_parser_aborts(test_data_row_nonfinite_double_child);
   assert_benchmark_data_parser_aborts(test_data_row_hex_double_child);
   static const char *const rejected_double_tokens[] = {
       "+1.0", "-1.0", ".",        ".e1", "1e",       "1e+",   "1e++2", "1e309",
       "nan",  "NaN",  "infinity", "INF", "0x1.8p+2", "1_000", "1,000", "--1",
   };
   for (u64 i = 0;
        i < sizeof(rejected_double_tokens) / sizeof(rejected_double_tokens[0]);
        i++) {
      assert_benchmark_data_parser_rejects_double_token(
          rejected_double_tokens[i]);
   }
   assert_benchmark_data_parser_aborts(test_data_row_overflow_int_child);
   assert_benchmark_data_parser_aborts(test_data_row_plus_int_child);
   assert_benchmark_data_parser_aborts(
       test_data_row_positive_overflow_signed_int_child);
   assert_benchmark_data_parser_aborts(test_data_row_overflow_signed_int_child);

   test_data_row_t row =
       test_data_row_parse("synthetic", "Uniform 1.0049995e+06", 2);
   double generated = test_data_row_double(&row, 1, "AveragePathLength");
   invariant(generated > 1000000.0);
   invariant(generated < 1005000.0);

   static const test_data_row_double_token_case_t accepted_double_tokens[] = {
       {"0", 0.0}, {".5", 0.5}, {"1.", 1.0}, {"1e+3", 1000.0}, {"1E-3", 1E-3},
   };
   for (u64 i = 0;
        i < sizeof(accepted_double_tokens) / sizeof(accepted_double_tokens[0]);
        i++) {
      assert_benchmark_data_parser_accepts_double_token(
          accepted_double_tokens[i].token, accepted_double_tokens[i].expected);
   }

   row = test_data_row_parse("synthetic", "Uniform -1", 2);
   assert_int_equal(-1, test_data_row_int(&row, 1, "MaximumPathLength"));

   row = test_data_row_parse("synthetic", "Uniform -2147483648", 2);
   assert_int_equal(INT_MIN, test_data_row_int(&row, 1, "MaximumPathLength"));
}

static bool test_data_hidden_entry(const char *name) {
   invariant(name != NULL);
   return name[0] == '.';
}

enum { test_operation_data_operation_count = 4 };
enum {
   test_operation_data_distribution_count = 5,
   test_operation_data_benchmark_iterations = 10,
   test_operation_data_synthetic_iterations = 1,
   test_operation_data_insert_delete_target_size = 1000000,
   test_operation_data_legacy_persistent_measurement_target_size = 100000,
};

static const char test_legacy_operation_benchmark_header[] =
    "Distribution Scale Size Step Position Iteration Duration";

static const char test_legacy_operation_measurement_header[] =
    "Distribution Scale Size Step Position PartitionCount PartitionDepth "
    "AveragePathLength MaximumPathLength Rotations Allocations";

static const char test_legacy_balancer_measurement_header[] =
    "Distribution Scale Size Iteration PartitionCount PartitionDepth "
    "AveragePathLength MaximumPathLength Rotations";

static const char *const
    test_operation_data_operation_names[test_operation_data_operation_count] = {
        "Insert",
        "InsertDelete",
        "InsertPersistent",
        "InsertDeletePersistent",
};

typedef struct {
   bool *trees;
   u64 tree_files;
} test_operation_data_signature_t;

static int test_operation_data_operation_index(const char *name) {
   invariant(name != NULL);
   for (int i = 0; i < test_operation_data_operation_count; i++) {
      if (strcmp(name, test_operation_data_operation_names[i]) == 0) {
         return i;
      }
   }
   return -1;
}

static int test_tree_registry_index(const char *name) {
   invariant(name != NULL);
   for (int i = 0; i < tree_registry_count; i++) {
      const list_vtable_t *vt = tree_registry_all[i];
      invariant(vt != NULL);
      if (strcmp(name, vt->name) == 0) {
         return i;
      }
   }
   return -1;
}

static int test_balancer_registry_index(const char *name) {
   invariant(name != NULL);
   for (int i = 0; i < balancer_registry_count; i++) {
      const balancer_t *balancer = balancer_registry_all[i];
      balancer_require(balancer);
      if (strcmp(name, balancer->name) == 0) {
         return i;
      }
   }
   return -1;
}

static int test_operation_data_distribution_index(const char *name) {
   invariant(name != NULL);
   for (int i = 0; i < dist_benchmark_count; i++) {
      const dist_spec_t *spec = dist_benchmark[i];
      dist_spec_require(spec);
      if (strcmp(name, spec->name) == 0) {
         return i;
      }
   }
   return -1;
}

static bool test_operation_data_distribution_rows_match(
    const u64 *counts, const u64 *last_positions, const char *distribution,
    u64 count, u64 last_position) {
   invariant(counts != NULL);
   invariant(last_positions != NULL);
   int index = test_operation_data_distribution_index(distribution);
   invariant(index >= 0);
   return counts[index] == count && last_positions[index] == last_position;
}

static bool
test_operation_data_complete_distribution_coverage(const u64 *counts,
                                                   const u64 *last_positions) {
   invariant(counts != NULL);
   invariant(last_positions != NULL);
   invariant(dist_benchmark_count > 0);

   u64 expected_count = counts[0];
   u64 expected_position = last_positions[0];
   if (expected_count == 0 || expected_position == 0) {
      return false;
   }
   for (int i = 1; i < dist_benchmark_count; i++) {
      if (counts[i] != expected_count ||
          last_positions[i] != expected_position) {
         return false;
      }
   }
   return true;
}

typedef struct {
   const char *distribution;
   u64 rows;
   u64 last_position;
} test_operation_data_distribution_expectation_t;

typedef struct {
   const char *operation;
   const char *tree;
   test_operation_data_distribution_expectation_t
       distributions[test_operation_data_distribution_count];
} test_partial_operation_measurement_t;

typedef struct {
   u64 *positions;
   u64 count;
   u64 cap;
} test_operation_data_positions_t;

static void
test_operation_data_positions_push(test_operation_data_positions_t *positions,
                                   u64 position) {
   invariant(positions != NULL);
   invariant(position > 0);

   if (positions->count == positions->cap) {
      invariant(positions->count < UINT64_MAX);
      u64 next_cap = bst_array_next_cap(positions->cap, positions->count + 1,
                                        16, sizeof(positions->positions[0]));
      positions->positions =
          positions->positions == NULL
              ? (u64 *)bst_malloc_array(next_cap,
                                        sizeof(positions->positions[0]))
              : (u64 *)bst_realloc_array(positions->positions, next_cap,
                                         sizeof(positions->positions[0]));
      positions->cap = next_cap;
   }

   positions->positions[positions->count++] = position;
}

static void test_operation_data_positions_reset(
    test_operation_data_positions_t *positions) {
   invariant(positions != NULL);
   positions->count = 0;
}

static void test_operation_data_positions_dispose(
    test_operation_data_positions_t *positions) {
   if (positions == NULL) {
      return;
   }
   free(positions->positions);
   positions->positions = NULL;
   positions->count = 0;
   positions->cap = 0;
}

static bool test_operation_data_insert_operation(const char *operation) {
   invariant(operation != NULL);
   return strcmp(operation, "Insert") == 0 ||
          strcmp(operation, "InsertPersistent") == 0;
}

static bool test_operation_data_insert_delete_operation(const char *operation) {
   invariant(operation != NULL);
   return strcmp(operation, "InsertDelete") == 0 ||
          strcmp(operation, "InsertDeletePersistent") == 0;
}

static bool test_operation_data_allowed_insert_delete_target(
    const char *operation, bool has_iterations, u64 target) {
   invariant(operation != NULL);
   if (target == test_operation_data_insert_delete_target_size) {
      return true;
   }
   return !has_iterations && strcmp(operation, "InsertDeletePersistent") == 0 &&
          target ==
              test_operation_data_legacy_persistent_measurement_target_size;
}

static u64 test_operation_data_expected_insert_delete_size(u64 scale,
                                                           u64 position) {
   invariant(scale > 0);
   invariant(position > 0);
   if (position <= scale) {
      return position;
   }
   return ((position - scale) % 2) == 0 ? scale : scale - 1;
}

static void test_operation_data_require_size(const char *operation,
                                             const char *path, const char *line,
                                             bool has_explicit_steps, u64 scale,
                                             u64 size, u64 position,
                                             bool *saw_plateau,
                                             u64 *last_size) {
   invariant(operation != NULL);
   invariant(path != NULL);
   invariant(line != NULL);
   invariant(scale > 0);
   invariant(position > 0);
   invariant(saw_plateau != NULL);
   invariant(last_size != NULL);

   if (test_operation_data_insert_operation(operation)) {
      if (size == 0) {
         fprintf(stderr, "zero operation data size in %s: %s", path, line);
         abort();
      }
      if (size != position) {
         fprintf(stderr,
                 "unexpected operation data size in %s: expected %llu: %s",
                 path, (unsigned long long)position, line);
         abort();
      }
      *last_size = size;
      return;
   }

   if (test_operation_data_insert_delete_operation(operation)) {
      if (has_explicit_steps) {
         u64 expected =
             test_operation_data_expected_insert_delete_size(scale, position);
         if (size != expected) {
            fprintf(stderr,
                    "unexpected operation data size in %s: expected %llu: %s",
                    path, (unsigned long long)expected, line);
            abort();
         }
         *last_size = size;
         return;
      }
      if (size == 0) {
         fprintf(stderr, "zero legacy operation data size in %s: %s", path,
                 line);
         abort();
      }
      if (size > position) {
         fprintf(stderr, "operation data size exceeds position in %s: %s", path,
                 line);
         abort();
      }
      if (*last_size == 0) {
         if (size != position) {
            fprintf(stderr,
                    "operation data starts after the size plateau in %s: %s",
                    path, line);
            abort();
         }
      } else if (*saw_plateau) {
         if (size != *last_size) {
            fprintf(stderr,
                    "operation data size changes after plateau in %s: %s", path,
                    line);
            abort();
         }
      } else if (size == *last_size) {
         *saw_plateau = true;
      } else if (size <= *last_size || size != position) {
         fprintf(stderr, "nonmonotonic operation data size in %s: %s", path,
                 line);
         abort();
      }
      *last_size = size;
      return;
   }

   bst_unreachable();
}

static void test_operation_data_require_final_size_targets(
    const char *operation, const char *data_name, const char *tree,
    bool has_iterations, bool has_explicit_steps, u64 scale, u64 steps,
    const u64 *counts, const u64 *last_sizes) {
   invariant(operation != NULL);
   invariant(data_name != NULL);
   invariant(tree != NULL);
   invariant(scale > 0);
   invariant(steps > 0);
   invariant(counts != NULL);
   invariant(last_sizes != NULL);

   if (!test_operation_data_insert_delete_operation(operation)) {
      return;
   }
   for (int i = 0; i < dist_benchmark_count; i++) {
      if (counts[i] == 0) {
         continue;
      }
      u64 target = last_sizes[i];
      bool allowed =
          has_explicit_steps
              ? target == test_operation_data_expected_insert_delete_size(scale,
                                                                          steps)
              : test_operation_data_allowed_insert_delete_target(
                    operation, has_iterations, target);
      if (!allowed) {
         fprintf(
             stderr,
             "unexpected %s insert-delete target size in %s/%s for %s: %llu\n",
             data_name, operation, tree, dist_benchmark[i]->name,
             (unsigned long long)target);
         abort();
      }
   }
}

static void assert_operation_benchmark_row(const char *path, const char *line,
                                           u64 duration) {
   invariant(path != NULL);
   invariant(line != NULL);

   if (duration == 0) {
      fprintf(stderr, "nonpositive operation benchmark duration in %s: %s",
              path, line);
      abort();
   }
}

static bool measurement_average_path_length_too_low(u64 size,
                                                    double average_path_length,
                                                    int maximum_path_length) {
   invariant(size > 0);
   invariant(maximum_path_length >= 0);
   invariant((u64)maximum_path_length < size);
   invariant(size <=
             nodePathMetricsMaximumNodesForHeight((u64)maximum_path_length));
   invariant(isfinite(average_path_length));
   invariant(average_path_length >= 0.0);

   __uint128_t minimum_total_depth = nodePathMetricsMinimumTotalDepthForHeight(
       size, (u64)maximum_path_length);
   long double minimum_average =
       (long double)minimum_total_depth / (long double)size;
   long double tolerance = MAX(1.0L, fabsl(minimum_average)) * 0.00001L;
   return (long double)average_path_length + tolerance < minimum_average;
}

static bool measurement_average_path_length_too_high(u64 size,
                                                     double average_path_length,
                                                     int maximum_path_length) {
   invariant(size > 0);
   invariant(maximum_path_length >= 0);
   invariant((u64)maximum_path_length < size);
   invariant(size <=
             nodePathMetricsMaximumNodesForHeight((u64)maximum_path_length));
   invariant(isfinite(average_path_length));
   invariant(average_path_length >= 0.0);

   __uint128_t maximum_total_depth = nodePathMetricsMaximumTotalDepthForHeight(
       size, (u64)maximum_path_length);
   long double maximum_average =
       (long double)maximum_total_depth / (long double)size;
   long double tolerance = MAX(1.0L, fabsl(maximum_average)) * 0.00001L;
   return (long double)average_path_length - tolerance > maximum_average;
}

static void assert_operation_measurement_row(const char *path, const char *line,
                                             u64 size, u64 partition_count,
                                             u64 partition_depth,
                                             double average_path_length,
                                             int maximum_path_length,
                                             u64 rotations, u64 allocations) {
   invariant(path != NULL);
   invariant(line != NULL);

   if (!isfinite(average_path_length) || average_path_length < 0.0 ||
       (partition_count == 0 && partition_depth != 0)) {
      fprintf(stderr, "invalid operation measurement value in %s: %s", path,
              line);
      abort();
   }
   if (size == 0) {
      if (partition_count != 0 || partition_depth != 0 ||
          average_path_length != 0.0 || maximum_path_length != -1 ||
          rotations != 0 || allocations != 0) {
         fprintf(stderr, "invalid empty operation measurement value in %s: %s",
                 path, line);
         abort();
      }
      return;
   }
   if (maximum_path_length < 0 ||
       average_path_length > (double)maximum_path_length ||
       (u64)maximum_path_length >= size ||
       size > nodePathMetricsMaximumNodesForHeight((u64)maximum_path_length) ||
       measurement_average_path_length_too_low(size, average_path_length,
                                               maximum_path_length) ||
       measurement_average_path_length_too_high(size, average_path_length,
                                                maximum_path_length)) {
      fprintf(stderr, "invalid operation measurement value in %s: %s", path,
              line);
      abort();
   }
}

static void test_operation_measurement_empty_partition_child(void) {
   assert_operation_measurement_row(
       "synthetic", "Uniform 1 1 0 1 1 1 1 0 -1 0 0\n", 0, 1, 1, 0.0, -1, 0, 0);
}

static void test_operation_measurement_impossible_path_child(void) {
   assert_operation_measurement_row(
       "synthetic", "Uniform 3 3 3 1 3 0 0 1 3 0 0\n", 3, 0, 0, 1.0, 3, 0, 0);
}

static void test_operation_measurement_height_too_low_child(void) {
   assert_operation_measurement_row(
       "synthetic", "Uniform 4 4 4 1 4 0 0 0 0 1 1\n", 4, 0, 0, 1.0, 1, 1, 1);
}

static void test_operation_measurement_average_too_low_child(void) {
   assert_operation_measurement_row(
       "synthetic", "Uniform 4 4 4 1 4 0 0 0.5 2 1 1\n", 4, 0, 0, 0.5, 2, 1, 1);
}

static void test_operation_measurement_average_too_high_child(void) {
   assert_operation_measurement_row(
       "synthetic", "Uniform 4 4 4 1 4 0 0 1.5 2 1 1\n", 4, 0, 0, 1.5, 2, 1, 1);
}

static void test_operation_measurement_average_exceeds_max_child(void) {
   assert_operation_measurement_row(
       "synthetic", "Uniform 3 3 3 1 3 0 0 2.5 2 0 0\n", 3, 0, 0, 2.5, 2, 0, 0);
}

static void test_operation_measurement_orphan_partition_depth_child(void) {
   assert_operation_measurement_row(
       "synthetic", "Uniform 3 3 3 1 3 0 1 1 2 0 0\n", 3, 0, 1, 1.0, 2, 0, 0);
}

static void test_operation_measurement_empty_rotations_child(void) {
   assert_operation_measurement_row(
       "synthetic", "Uniform 1 1 0 1 1 0 0 0 -1 1 0\n", 0, 0, 0, 0.0, -1, 1, 0);
}

static void test_operation_measurement_empty_allocations_child(void) {
   assert_operation_measurement_row(
       "synthetic", "Uniform 1 1 0 1 1 0 0 0 -1 0 1\n", 0, 0, 0, 0.0, -1, 0, 1);
}

static void
assert_operation_measurement_row_validator_accepts_root_partition(void) {
   assert_operation_measurement_row(
       "synthetic", "Uniform 1 1 1 1 1 1 0 0 0 0 0\n", 1, 1, 0, 0.0, 0, 0, 0);
}

static void assert_operation_measurement_row_validator_aborts(void) {
   assert_benchmark_data_parser_aborts(
       test_operation_measurement_empty_partition_child);
   assert_benchmark_data_parser_aborts(
       test_operation_measurement_impossible_path_child);
   assert_benchmark_data_parser_aborts(
       test_operation_measurement_height_too_low_child);
   assert_benchmark_data_parser_aborts(
       test_operation_measurement_average_too_low_child);
   assert_benchmark_data_parser_aborts(
       test_operation_measurement_average_too_high_child);
   assert_benchmark_data_parser_aborts(
       test_operation_measurement_average_exceeds_max_child);
   assert_benchmark_data_parser_aborts(
       test_operation_measurement_orphan_partition_depth_child);
   assert_benchmark_data_parser_aborts(
       test_operation_measurement_empty_rotations_child);
   assert_benchmark_data_parser_aborts(
       test_operation_measurement_empty_allocations_child);
}

static void test_operation_data_require_sample_schedule(
    const char *data_name, const char *operation, const char *tree,
    const char *path, const char *distribution, u64 iteration, u64 data_steps,
    const test_operation_data_positions_t *positions, bool complete_iteration) {
   invariant(data_name != NULL);
   invariant(operation != NULL);
   invariant(tree != NULL);
   invariant(path != NULL);
   invariant(distribution != NULL);
   invariant(iteration > 0);
   invariant(data_steps > 0);
   invariant(positions != NULL);
   invariant(positions->count > 0);
   invariant(positions->positions != NULL);

   u64 scheduled_samples = positions->count;
   if (!complete_iteration) {
      u64 observed = positions->count;
      scheduled_samples = test_infer_sample_schedule_from_position(
          data_steps, observed, positions->positions[observed - 1]);
      if (scheduled_samples == 0) {
         fprintf(stderr,
                 "cannot infer %s sample schedule in %s (%s/%s for %s "
                 "iteration %llu): rows=%llu last_position=%llu steps=%llu\n",
                 data_name, path, operation, tree, distribution,
                 (unsigned long long)iteration, (unsigned long long)observed,
                 (unsigned long long)positions->positions[observed - 1],
                 (unsigned long long)data_steps);
         abort();
      }
   }
   invariant(scheduled_samples > 0);

   if (positions->count > scheduled_samples) {
      fprintf(stderr,
              "too many %s sample rows in %s (%s/%s for %s iteration %llu): "
              "rows=%llu samples=%llu\n",
              data_name, path, operation, tree, distribution,
              (unsigned long long)iteration,
              (unsigned long long)positions->count,
              (unsigned long long)scheduled_samples);
      abort();
   }

   sample_schedule_t schedule = {
       .total = data_steps,
       .samples = scheduled_samples,
   };
   sample_schedule_require(schedule);
   for (u64 i = 0; i < positions->count; i++) {
      sample_schedule_point_t point = sample_schedule_point(schedule, i);
      u64 expected = point.position;
      if (positions->positions[i] != expected) {
         fprintf(stderr,
                 "off-schedule %s position in %s (%s/%s for %s iteration "
                 "%llu row %llu): expected %llu, got %llu\n",
                 data_name, path, operation, tree, distribution,
                 (unsigned long long)iteration,
                 (unsigned long long)point.ordinal,
                 (unsigned long long)expected,
                 (unsigned long long)positions->positions[i]);
         abort();
      }
   }
}

static void test_operation_data_require_completed_iteration(
    const char *data_name, const char *operation, const char *tree,
    const char *path, const char *distribution, u64 iteration, u64 data_steps,
    u64 last_position, u64 iteration_rows,
    const test_operation_data_positions_t *positions,
    u64 *expected_iteration_rows) {
   invariant(data_name != NULL);
   invariant(operation != NULL);
   invariant(tree != NULL);
   invariant(path != NULL);
   invariant(distribution != NULL);
   invariant(iteration > 0);
   invariant(data_steps > 0);
   invariant(positions != NULL);
   invariant(expected_iteration_rows != NULL);

   if (last_position != data_steps) {
      fprintf(stderr,
              "incomplete %s iteration in %s (%s/%s for %s iteration %llu): "
              "last_position=%llu steps=%llu\n",
              data_name, path, operation, tree, distribution,
              (unsigned long long)iteration, (unsigned long long)last_position,
              (unsigned long long)data_steps);
      abort();
   }
   if (iteration_rows == 0) {
      fprintf(stderr,
              "empty %s iteration in %s (%s/%s for %s iteration %llu)\n",
              data_name, path, operation, tree, distribution,
              (unsigned long long)iteration);
      abort();
   }
   invariant(positions->count == iteration_rows);
   test_operation_data_require_sample_schedule(data_name, operation, tree, path,
                                               distribution, iteration,
                                               data_steps, positions, true);
   if (*expected_iteration_rows == 0) {
      *expected_iteration_rows = iteration_rows;
      return;
   }
   if (iteration_rows != *expected_iteration_rows) {
      fprintf(stderr,
              "inconsistent %s iteration sample count in %s (%s/%s for %s "
              "iteration %llu: expected %llu, got %llu\n",
              data_name, path, operation, tree, distribution,
              (unsigned long long)iteration,
              (unsigned long long)*expected_iteration_rows,
              (unsigned long long)iteration_rows);
      abort();
   }
}

static void test_operation_data_require_benchmark_order(
    const char *data_name, const char *path, const char *line,
    const char *distribution, int distribution_index, u64 iteration,
    u64 position, u64 data_steps, u64 *expected_iteration,
    int *expected_distribution_index) {
   invariant(data_name != NULL);
   invariant(path != NULL);
   invariant(line != NULL);
   invariant(distribution != NULL);
   invariant(distribution_index >= 0);
   invariant(distribution_index < dist_benchmark_count);
   invariant(iteration > 0);
   invariant(position > 0);
   invariant(data_steps > 0);
   invariant(position <= data_steps);
   invariant(expected_iteration != NULL);
   invariant(*expected_iteration > 0);
   invariant(expected_distribution_index != NULL);
   invariant(*expected_distribution_index >= 0);
   invariant(*expected_distribution_index < dist_benchmark_count);

   if (iteration != *expected_iteration ||
       distribution_index != *expected_distribution_index) {
      fprintf(stderr,
              "out-of-order %s row in %s: expected %s iteration %llu, "
              "got %s iteration %llu: %s",
              data_name, path,
              dist_benchmark[*expected_distribution_index]->name,
              (unsigned long long)*expected_iteration, distribution,
              (unsigned long long)iteration, line);
      abort();
   }

   if (position == data_steps) {
      if (*expected_distribution_index + 1 < dist_benchmark_count) {
         (*expected_distribution_index)++;
      } else {
         *expected_distribution_index = 0;
         invariant(*expected_iteration < UINT64_MAX);
         (*expected_iteration)++;
      }
   }
}

static bool
test_known_partial_operation_measurement(const char *operation,
                                         const char *tree, const u64 *counts,
                                         const u64 *last_positions) {
   invariant(operation != NULL);
   invariant(tree != NULL);
   invariant(counts != NULL);
   invariant(last_positions != NULL);

   static const test_partial_operation_measurement_t partials[] = {
       {
           .operation = "Insert",
           .tree = "AVLBottomUp",
           .distributions =
               {
                   {"Uniform", 645, 6450000},
                   {"Normal", 0, 0},
                   {"Skewed", 0, 0},
                   {"Zipf", 0, 0},
                   {"Maximum", 0, 0},
               },
       },
       {
           .operation = "Insert",
           .tree = "Splay",
           .distributions =
               {
                   {"Uniform", 1000, 10000000},
                   {"Normal", 1000, 10000000},
                   {"Skewed", 1000, 10000000},
                   {"Zipf", 1000, 10000000},
                   {"Maximum", 958, 9580000},
               },
       },
       {
           .operation = "InsertPersistent",
           .tree = "TreapFingerTree",
           .distributions =
               {
                   {"Uniform", 1000, 10000000},
                   {"Normal", 1000, 10000000},
                   {"Skewed", 1000, 10000000},
                   {"Zipf", 971, 9710000},
                   {"Maximum", 0, 0},
               },
       },
       {
           .operation = "InsertDeletePersistent",
           .tree = "AVLTopDown",
           .distributions =
               {
                   {"Uniform", 1000, 10000000},
                   {"Normal", 246, 2460000},
                   {"Skewed", 0, 0},
                   {"Zipf", 0, 0},
                   {"Maximum", 0, 0},
               },
       },
       {
           .operation = "InsertDeletePersistent",
           .tree = "WBSTRelaxed",
           .distributions =
               {
                   {"Uniform", 1000, 10000000},
                   {"Normal", 101, 1010000},
                   {"Skewed", 0, 0},
                   {"Zipf", 0, 0},
                   {"Maximum", 0, 0},
               },
       },
   };

   invariant(dist_benchmark_count == test_operation_data_distribution_count);
   for (size_t i = 0; i < sizeof(partials) / sizeof(partials[0]); i++) {
      const test_partial_operation_measurement_t *partial = &partials[i];
      if (strcmp(operation, partial->operation) != 0 ||
          strcmp(tree, partial->tree) != 0) {
         continue;
      }
      for (int d = 0; d < dist_benchmark_count; d++) {
         const test_operation_data_distribution_expectation_t *expected =
             &partial->distributions[d];
         if (!test_operation_data_distribution_rows_match(
                 counts, last_positions, expected->distribution, expected->rows,
                 expected->last_position)) {
            return false;
         }
      }
      return true;
   }
   return false;
}

static void assert_operation_data_distribution_coverage(
    const char *data_name, const char *operation, const char *tree,
    const char *path, bool has_iterations, u64 required_iterations,
    bool allow_known_partials) {
   invariant(data_name != NULL);
   invariant(operation != NULL);
   invariant(tree != NULL);
   invariant(path != NULL);
   invariant(dist_benchmark_count > 0);
   if (!has_iterations) {
      invariant(required_iterations == 0 || required_iterations == 1);
   }

   u64 *counts =
       (u64 *)bst_calloc_array((u64)dist_benchmark_count, sizeof(counts[0]));
   u64 *last_positions = (u64 *)bst_calloc_array((u64)dist_benchmark_count,
                                                 sizeof(last_positions[0]));
   u64 *last_iterations = (u64 *)bst_calloc_array((u64)dist_benchmark_count,
                                                  sizeof(last_iterations[0]));
   u64 *last_sizes = (u64 *)bst_calloc_array((u64)dist_benchmark_count,
                                             sizeof(last_sizes[0]));
   u64 *iteration_rows = (u64 *)bst_calloc_array((u64)dist_benchmark_count,
                                                 sizeof(iteration_rows[0]));
   u64 *expected_iteration_rows = (u64 *)bst_calloc_array(
       (u64)dist_benchmark_count, sizeof(expected_iteration_rows[0]));
   bool *saw_plateaus = (bool *)bst_calloc_array((u64)dist_benchmark_count,
                                                 sizeof(saw_plateaus[0]));
   test_operation_data_positions_t *positions =
       (test_operation_data_positions_t *)bst_calloc_array(
           (u64)dist_benchmark_count, sizeof(positions[0]));
   u64 data_scale = 0;
   u64 data_steps = 0;
   int data_has_explicit_steps = -1;
   u64 expected_order_iteration = 1;
   int expected_order_distribution = 0;

   FILE *file = fopen_or_exit(path, "r");
   char line[TEST_DATA_LINE_MAX];
   invariant(test_fgets_or_abort(line, sizeof(line), file, path));
   test_data_file_require_complete_line(file, path, line);
   test_trim_line_end(line);
   const char *current_header =
       has_iterations ? BENCHMARK_SCHEMA_OPERATION_BENCHMARK_HEADER
                      : BENCHMARK_SCHEMA_OPERATION_MEASUREMENT_HEADER;
   const char *legacy_header = has_iterations
                                   ? test_legacy_operation_benchmark_header
                                   : test_legacy_operation_measurement_header;
   bool current_schema = strcmp(line, current_header) == 0;
   bool legacy_schema = strcmp(line, legacy_header) == 0;
   if (!current_schema && !legacy_schema) {
      fprintf(stderr, "unexpected operation data header in %s: %s\n", path,
              line);
      abort();
   }
   data_has_explicit_steps = current_schema ? 1 : 0;
   int current_fields = has_iterations
                            ? BENCHMARK_SCHEMA_OPERATION_BENCHMARK_FIELDS
                            : BENCHMARK_SCHEMA_OPERATION_MEASUREMENT_FIELDS;
   int legacy_fields = test_data_field_count(legacy_header);
   int expected_fields = current_schema ? current_fields : legacy_fields;

   while (test_fgets_or_abort(line, sizeof(line), file, path)) {
      test_data_file_require_complete_line(file, path, line);
      int fields = test_data_field_count(line);
      if (fields != expected_fields) {
         fprintf(stderr,
                 "unexpected field count in %s: expected %d, got %d: %s", path,
                 expected_fields, fields, line);
         abort();
      }
      bool has_steps = current_schema;
      test_data_row_t row = test_data_row_parse(path, line, fields);
      const char *distribution = test_data_row_string(&row, 0, "Distribution");
      u64 scale = test_data_row_u64(&row, 1, "Scale");
      u64 steps = has_steps ? test_data_row_u64(&row, 2, "Steps") : scale;
      int schedule_offset = has_steps ? 1 : 0;
      u64 size = test_data_row_u64(&row, 2 + schedule_offset, "Size");
      u64 step = test_data_row_u64(&row, 3 + schedule_offset, "Step");
      u64 position = test_data_row_u64(&row, 4 + schedule_offset, "Position");
      u64 iteration = 1;
      if (has_iterations) {
         iteration = test_data_row_u64(&row, 5 + schedule_offset, "Iteration");
         u64 duration =
             test_data_row_u64(&row, 6 + schedule_offset, "Duration");
         assert_operation_benchmark_row(path, line, duration);
      } else {
         u64 partition_count =
             test_data_row_u64(&row, 5 + schedule_offset, "PartitionCount");
         u64 partition_depth =
             test_data_row_u64(&row, 6 + schedule_offset, "PartitionDepth");
         double average_path_length = test_data_row_double(
             &row, 7 + schedule_offset, "AveragePathLength");
         int maximum_path_length =
             test_data_row_int(&row, 8 + schedule_offset, "MaximumPathLength");
         u64 rotations =
             test_data_row_u64(&row, 9 + schedule_offset, "Rotations");
         u64 allocations =
             test_data_row_u64(&row, 10 + schedule_offset, "Allocations");
         assert_operation_measurement_row(
             path, line, size, partition_count, partition_depth,
             average_path_length, maximum_path_length, rotations, allocations);
      }
      int index = test_operation_data_distribution_index(distribution);
      if (index < 0) {
         fprintf(stderr, "unexpected operation data distribution in %s: %s\n",
                 path, distribution);
         abort();
      }

      if (scale == 0 || steps == 0 || step == 0 || position == 0 ||
          iteration == 0) {
         fprintf(stderr, "nonpositive operation data schedule value in %s: %s",
                 path, line);
         abort();
      }
      if (data_scale == 0) {
         data_scale = scale;
      } else if (scale != data_scale) {
         fprintf(stderr, "inconsistent operation data scale in %s: %s", path,
                 line);
         abort();
      }
      if (data_steps == 0) {
         data_steps = steps;
      } else if (steps != data_steps) {
         fprintf(stderr, "inconsistent operation data step count in %s: %s",
                 path, line);
         abort();
      }
      if (position > data_steps) {
         fprintf(stderr, "operation data position exceeds steps in %s: %s",
                 path, line);
         abort();
      }
      if (has_iterations) {
         test_operation_data_require_benchmark_order(
             data_name, path, line, distribution, index, iteration, position,
             data_steps, &expected_order_iteration,
             &expected_order_distribution);
      }
      if (has_iterations && last_iterations[index] != iteration) {
         if (last_iterations[index] == 0) {
            if (iteration != 1) {
               fprintf(stderr,
                       "operation data starts at iteration %llu in %s: %s",
                       (unsigned long long)iteration, path, line);
               abort();
            }
         } else {
            u64 expected_iteration =
                size_after_inserting_one(last_iterations[index]);
            if (iteration != expected_iteration) {
               fprintf(stderr,
                       "nonsequential operation data iteration in %s: %s", path,
                       line);
               abort();
            }
            test_operation_data_require_completed_iteration(
                data_name, operation, tree, path, distribution,
                last_iterations[index], data_steps, last_positions[index],
                iteration_rows[index], &positions[index],
                &expected_iteration_rows[index]);
         }
         last_iterations[index] = iteration;
         last_positions[index] = 0;
         last_sizes[index] = 0;
         iteration_rows[index] = 0;
         test_operation_data_positions_reset(&positions[index]);
         saw_plateaus[index] = false;
      } else if (!has_iterations && last_iterations[index] == 0) {
         last_iterations[index] = 1;
      }
      if (position <= last_positions[index] ||
          step != position - last_positions[index]) {
         fprintf(stderr, "nonsequential operation data position in %s: %s",
                 path, line);
         abort();
      }
      test_operation_data_require_size(operation, path, line, has_steps, scale,
                                       size, position, &saw_plateaus[index],
                                       &last_sizes[index]);

      invariant(counts[index] < UINT64_MAX);
      counts[index]++;
      invariant(iteration_rows[index] < UINT64_MAX);
      iteration_rows[index]++;
      test_operation_data_positions_push(&positions[index], position);
      last_positions[index] = position;
   }
   fclose_or_exit(file, path);
   invariant(data_has_explicit_steps >= 0);

   if (has_iterations) {
      for (int i = 0; i < dist_benchmark_count; i++) {
         if (counts[i] == 0) {
            continue;
         }
         test_operation_data_require_completed_iteration(
             data_name, operation, tree, path, dist_benchmark[i]->name,
             last_iterations[i], data_steps, last_positions[i],
             iteration_rows[i], &positions[i], &expected_iteration_rows[i]);
      }
   } else {
      for (int i = 0; i < dist_benchmark_count; i++) {
         if (counts[i] == 0) {
            continue;
         }
         test_operation_data_require_sample_schedule(
             data_name, operation, tree, path, dist_benchmark[i]->name,
             last_iterations[i], data_steps, &positions[i],
             last_positions[i] == data_steps);
      }
   }

   bool complete = test_operation_data_complete_distribution_coverage(
       counts, last_positions);
   bool known_partial =
       allow_known_partials && test_known_partial_operation_measurement(
                                   operation, tree, counts, last_positions);
   test_operation_data_require_final_size_targets(
       operation, data_name, tree, has_iterations, data_has_explicit_steps != 0,
       data_scale, data_steps, counts, last_sizes);
   if (complete) {
      u64 expected_iterations = has_iterations ? required_iterations : 1;
      for (int i = 0; i < dist_benchmark_count; i++) {
         if (last_positions[i] != data_steps) {
            fprintf(stderr,
                    "unexpected %s final position in %s/%s for %s: expected "
                    "%llu, got %llu\n",
                    data_name, operation, tree, dist_benchmark[i]->name,
                    (unsigned long long)data_steps,
                    (unsigned long long)last_positions[i]);
            abort();
         }
         if (has_iterations && expected_iterations == 0) {
            expected_iterations = last_iterations[i];
            invariant(expected_iterations > 0);
         }
         if (last_iterations[i] != expected_iterations) {
            fprintf(stderr,
                    "unexpected %s iteration count in %s/%s for %s: expected "
                    "%llu, got %llu\n",
                    data_name, operation, tree, dist_benchmark[i]->name,
                    (unsigned long long)expected_iterations,
                    (unsigned long long)last_iterations[i]);
            abort();
         }
      }
   }
   if (!complete && !known_partial) {
      fprintf(stderr, "incomplete %s distribution coverage in %s/%s\n",
              data_name, operation, tree);
      for (int i = 0; i < dist_benchmark_count; i++) {
         fprintf(stderr, "  %s: rows=%llu last_position=%llu\n",
                 dist_benchmark[i]->name, (unsigned long long)counts[i],
                 (unsigned long long)last_positions[i]);
      }
      abort();
   }

   free(counts);
   free(last_positions);
   free(last_iterations);
   free(last_sizes);
   free(iteration_rows);
   free(expected_iteration_rows);
   free(saw_plateaus);
   for (int i = 0; i < dist_benchmark_count; i++) {
      test_operation_data_positions_dispose(&positions[i]);
   }
   free(positions);
}

static void test_write_off_schedule_operation_data(const char *path) {
   invariant(path != NULL);
   invariant(dist_benchmark_count > 0);

   FILE *file = fopen_or_exit(path, "w");
   file_printf_or_exit(file, path, "%s\n",
                       BENCHMARK_SCHEMA_OPERATION_BENCHMARK_HEADER);
   for (int i = 0; i < dist_benchmark_count; i++) {
      const dist_spec_t *dist = dist_benchmark[i];
      dist_spec_require(dist);
      file_printf_or_exit(file, path, "%s 10 10 4 4 4 1 1\n", dist->name);
      file_printf_or_exit(file, path, "%s 10 10 10 6 10 1 1\n", dist->name);
   }
   fclose_or_exit(file, path);
}

static void
test_write_distribution_outer_operation_benchmark_data(const char *path) {
   invariant(path != NULL);
   invariant(dist_benchmark_count > 0);

   FILE *file = fopen_or_exit(path, "w");
   file_printf_or_exit(file, path, "%s\n",
                       BENCHMARK_SCHEMA_OPERATION_BENCHMARK_HEADER);
   for (int i = 0; i < dist_benchmark_count; i++) {
      const dist_spec_t *dist = dist_benchmark[i];
      dist_spec_require(dist);
      for (u64 iteration = 1; iteration <= 2; iteration++) {
         file_printf_or_exit(file, path, "%s 2 2 1 1 1 %llu 1\n", dist->name,
                             (unsigned long long)iteration);
         file_printf_or_exit(file, path, "%s 2 2 2 1 2 %llu 1\n", dist->name,
                             (unsigned long long)iteration);
      }
   }
   fclose_or_exit(file, path);
}

static void
test_write_zero_duration_operation_benchmark_data(const char *path) {
   invariant(path != NULL);
   invariant(dist_benchmark_count > 0);

   FILE *file = fopen_or_exit(path, "w");
   file_printf_or_exit(file, path, "%s\n",
                       BENCHMARK_SCHEMA_OPERATION_BENCHMARK_HEADER);
   for (int i = 0; i < dist_benchmark_count; i++) {
      const dist_spec_t *dist = dist_benchmark[i];
      dist_spec_require(dist);
      file_printf_or_exit(file, path, "%s 2 2 1 1 1 1 0\n", dist->name);
      file_printf_or_exit(file, path, "%s 2 2 2 1 2 1 1\n", dist->name);
   }
   fclose_or_exit(file, path);
}

static void test_write_wrong_header_operation_benchmark_data(const char *path) {
   invariant(path != NULL);
   invariant(dist_benchmark_count > 0);

   FILE *file = fopen_or_exit(path, "w");
   file_printf_or_exit(
       file, path,
       "Distribution Scale Steps Size Step Position Iteration Time\n");
   for (int i = 0; i < dist_benchmark_count; i++) {
      const dist_spec_t *dist = dist_benchmark[i];
      dist_spec_require(dist);
      file_printf_or_exit(file, path, "%s 2 2 1 1 1 1 1\n", dist->name);
      file_printf_or_exit(file, path, "%s 2 2 2 1 2 1 1\n", dist->name);
   }
   fclose_or_exit(file, path);
}

static void
test_write_wrong_header_operation_measurement_data(const char *path) {
   invariant(path != NULL);
   invariant(dist_benchmark_count > 0);

   FILE *file = fopen_or_exit(path, "w");
   file_printf_or_exit(
       file, path,
       "Distribution Scale Steps Size Step Position PartitionCount "
       "PartitionDepth AveragePathLength MaximumPathLength Rotations "
       "AllocationCount\n");
   for (int i = 0; i < dist_benchmark_count; i++) {
      const dist_spec_t *dist = dist_benchmark[i];
      dist_spec_require(dist);
      file_printf_or_exit(file, path, "%s 2 2 1 1 1 0 0 0 0 0 0\n", dist->name);
      file_printf_or_exit(file, path, "%s 2 2 2 1 2 0 0 0.5 1 0 0\n",
                          dist->name);
   }
   fclose_or_exit(file, path);
}

static void test_write_unterminated_operation_benchmark_data(const char *path) {
   invariant(path != NULL);
   invariant(dist_benchmark_count > 0);

   FILE *file = fopen_or_exit(path, "w");
   file_printf_or_exit(file, path, "%s\n",
                       BENCHMARK_SCHEMA_OPERATION_BENCHMARK_HEADER);
   for (int i = 0; i < dist_benchmark_count; i++) {
      const dist_spec_t *dist = dist_benchmark[i];
      dist_spec_require(dist);
      file_printf_or_exit(file, path, "%s 2 2 1 1 1 1 1\n", dist->name);
      if (i + 1 == dist_benchmark_count) {
         file_printf_or_exit(file, path, "%s 2 2 2 1 2 1 1", dist->name);
      } else {
         file_printf_or_exit(file, path, "%s 2 2 2 1 2 1 1\n", dist->name);
      }
   }
   fclose_or_exit(file, path);
}

static void assert_operation_data_accepts_independent_scale_and_steps(void) {
   char output_dir[] = "/tmp/bst-operation-data-scale-steps-XXXXXX";
   char *root = mkdtemp(output_dir);
   invariant(root != NULL);

   char path[512];
   path_format(path, sizeof(path), "%s/scale-steps.txt", root);

   FILE *file = fopen_or_exit(path, "w");
   file_printf_or_exit(file, path, "%s\n",
                       BENCHMARK_SCHEMA_OPERATION_BENCHMARK_HEADER);
   for (int i = 0; i < dist_benchmark_count; i++) {
      const dist_spec_t *dist = dist_benchmark[i];
      dist_spec_require(dist);
      for (u64 iteration = 1;
           iteration <= test_operation_data_synthetic_iterations; iteration++) {
         file_printf_or_exit(file, path, "%s 3 6 2 2 2 %llu 1\n", dist->name,
                             (unsigned long long)iteration);
         file_printf_or_exit(file, path, "%s 3 6 4 2 4 %llu 1\n", dist->name,
                             (unsigned long long)iteration);
         file_printf_or_exit(file, path, "%s 3 6 6 2 6 %llu 1\n", dist->name,
                             (unsigned long long)iteration);
      }
   }
   fclose_or_exit(file, path);

   assert_operation_data_distribution_coverage(
       "synthetic operation benchmark data", "Insert", "Splay", path, true, 0,
       false);

   invariant(remove(path) == 0);
   invariant(rmdir(root) == 0);
}

static void assert_operation_data_accepts_current_insert_delete_schedule(void) {
   char output_dir[] = "/tmp/bst-operation-data-insert-delete-XXXXXX";
   char *root = mkdtemp(output_dir);
   invariant(root != NULL);

   char benchmark_path[512];
   path_format(benchmark_path, sizeof(benchmark_path),
               "%s/insert-delete-benchmark.txt", root);

   FILE *file = fopen_or_exit(benchmark_path, "w");
   file_printf_or_exit(file, benchmark_path, "%s\n",
                       BENCHMARK_SCHEMA_OPERATION_BENCHMARK_HEADER);
   for (int i = 0; i < dist_benchmark_count; i++) {
      const dist_spec_t *dist = dist_benchmark[i];
      dist_spec_require(dist);
      for (u64 iteration = 1;
           iteration <= test_operation_data_synthetic_iterations; iteration++) {
         file_printf_or_exit(file, benchmark_path, "%s 3 6 2 2 2 %llu 1\n",
                             dist->name, (unsigned long long)iteration);
         file_printf_or_exit(file, benchmark_path, "%s 3 6 2 2 4 %llu 1\n",
                             dist->name, (unsigned long long)iteration);
         file_printf_or_exit(file, benchmark_path, "%s 3 6 2 2 6 %llu 1\n",
                             dist->name, (unsigned long long)iteration);
      }
   }
   fclose_or_exit(file, benchmark_path);

   assert_operation_data_distribution_coverage(
       "synthetic operation benchmark data", "InsertDelete", "Splay",
       benchmark_path, true, 0, false);

   char measurement_path[512];
   path_format(measurement_path, sizeof(measurement_path),
               "%s/insert-delete-measurement.txt", root);

   file = fopen_or_exit(measurement_path, "w");
   file_printf_or_exit(file, measurement_path, "%s\n",
                       BENCHMARK_SCHEMA_OPERATION_MEASUREMENT_HEADER);
   for (int i = 0; i < dist_benchmark_count; i++) {
      const dist_spec_t *dist = dist_benchmark[i];
      dist_spec_require(dist);
      file_printf_or_exit(file, measurement_path, "%s 1 2 1 1 1 0 0 0 0 0 1\n",
                          dist->name);
      file_printf_or_exit(file, measurement_path, "%s 1 2 0 1 2 0 0 0 -1 0 0\n",
                          dist->name);
   }
   fclose_or_exit(file, measurement_path);

   assert_operation_data_distribution_coverage(
       "synthetic operation measurement data", "InsertDelete", "Splay",
       measurement_path, false, 0, false);

   invariant(remove(benchmark_path) == 0);
   invariant(remove(measurement_path) == 0);
   invariant(rmdir(root) == 0);
}

typedef void (*test_write_operation_data_fn)(const char *path);

static void
assert_operation_data_validator_aborts(test_write_operation_data_fn write_data,
                                       const char *filename,
                                       bool has_iterations) {
   invariant(write_data != NULL);
   invariant(filename != NULL);

   char output_dir[] = "/tmp/bst-operation-data-invalid-XXXXXX";
   char *root = mkdtemp(output_dir);
   invariant(root != NULL);

   char path[512];
   path_format(path, sizeof(path), "%s/%s", root, filename);
   write_data(path);

   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      assert_operation_data_distribution_coverage(
          has_iterations ? "synthetic operation benchmark data"
                         : "synthetic operation measurement data",
          "Insert", "Splay", path, has_iterations, 0, false);
      _exit(0);
   }
   assert_child_aborted(pid);

   invariant(remove(path) == 0);
   invariant(rmdir(root) == 0);
}

static void assert_operation_data_rejects_distribution_outer_order(void) {
   assert_operation_data_validator_aborts(
       test_write_distribution_outer_operation_benchmark_data,
       "distribution-outer.txt", true);
}

static void assert_operation_data_schedule_validator_aborts(void) {
   assert_operation_data_validator_aborts(
       test_write_off_schedule_operation_data, "off-schedule.txt", true);
   assert_operation_data_validator_aborts(
       test_write_wrong_header_operation_benchmark_data,
       "wrong-benchmark-header.txt", true);
   assert_operation_data_validator_aborts(
       test_write_wrong_header_operation_measurement_data,
       "wrong-measurement-header.txt", false);
   assert_operation_data_validator_aborts(
       test_write_unterminated_operation_benchmark_data,
       "unterminated-benchmark-row.txt", true);
}

static void assert_operation_data_duration_validator_aborts(void) {
   assert_operation_data_validator_aborts(
       test_write_zero_duration_operation_benchmark_data, "zero-duration.txt",
       true);
}

static void
test_operation_data_signature_dispose(test_operation_data_signature_t *sig) {
   invariant(sig != NULL);
   free(sig->trees);
   sig->trees = NULL;
   sig->tree_files = 0;
}

static void assert_operation_data_signatures_equal(
    const char *left_name, const test_operation_data_signature_t *left,
    const char *right_name, const test_operation_data_signature_t *right) {
   invariant(left_name != NULL);
   invariant(left != NULL);
   invariant(left->trees != NULL);
   invariant(left->tree_files > 0);
   invariant(right_name != NULL);
   invariant(right != NULL);
   invariant(right->trees != NULL);
   invariant(right->tree_files > 0);

   if (left->tree_files != right->tree_files) {
      fprintf(stderr,
              "operation data tree count mismatch: %s has %llu, %s has %llu\n",
              left_name, (unsigned long long)left->tree_files, right_name,
              (unsigned long long)right->tree_files);
      abort();
   }

   for (int i = 0; i < tree_registry_count; i++) {
      if (left->trees[i] != right->trees[i]) {
         const char *tree_name = tree_registry_all[i]->name;
         fprintf(stderr,
                 "operation data tree set mismatch: %s is %s in %s but %s "
                 "in %s\n",
                 tree_name, left->trees[i] ? "present" : "absent", left_name,
                 right->trees[i] ? "present" : "absent", right_name);
         abort();
      }
   }
}

static bool test_operation_data_known_unbenchmarked_tree(const char *tree) {
   invariant(tree != NULL);
   return strcmp(tree, "AA") == 0 || strcmp(tree, "LLRB") == 0 ||
          strcmp(tree, "TreapBottomUp") == 0;
}

static void assert_operation_data_registry_coverage(
    const char *data_name, const test_operation_data_signature_t *sig) {
   invariant(data_name != NULL);
   invariant(sig != NULL);
   invariant(sig->trees != NULL);
   invariant(sig->tree_files > 0);

   for (int i = 0; i < tree_registry_count; i++) {
      const list_vtable_t *vt = tree_registry_all[i];
      list_vtable_require(vt);
      if (sig->trees[i]) {
         continue;
      }
      if (test_operation_data_known_unbenchmarked_tree(vt->name)) {
         continue;
      }
      fprintf(stderr, "%s is missing registered tree: %s\n", data_name,
              vt->name);
      abort();
   }
}

static const char *test_plot_group_assignment(const char *plot_name,
                                              const char *plot,
                                              const char *group) {
   invariant(plot_name != NULL);
   invariant(plot != NULL);
   invariant(group != NULL);
   invariant(group[0] != '\0');

   size_t group_len = strlen(group);
   const char *assignment = NULL;
   const char *p = plot;
   while ((p = strstr(p, group)) != NULL) {
      const char *line = p;
      if (line != plot && line[-1] != '\n') {
         p += group_len;
         continue;
      }

      const char *after = line + group_len;
      while (*after == ' ' || *after == '\t') {
         after++;
      }
      if (*after != '=') {
         p += group_len;
         continue;
      }
      after++;
      while (*after == ' ' || *after == '\t') {
         after++;
      }
      if (*after != '"') {
         fprintf(stderr, "%s plot group is not quoted: %s\n", plot_name, group);
         abort();
      }
      if (assignment != NULL) {
         fprintf(stderr, "%s plot group is defined multiple times: %s\n",
                 plot_name, group);
         abort();
      }
      assignment = after + 1;
      p = assignment;
   }

   if (assignment != NULL) {
      return assignment;
   }

   fprintf(stderr, "missing %s plot group: %s\n", plot_name, group);
   abort();
}

static bool test_plot_assignment_next_token(const char **cursor, char *token,
                                            size_t token_size) {
   invariant(cursor != NULL);
   invariant(*cursor != NULL);
   invariant(token != NULL);
   invariant(token_size > 1);

   const char *p = *cursor;
   while (*p == ' ' || *p == '\t' || *p == '\\' || *p == '\n' || *p == '\r') {
      p++;
   }
   if (*p == '"') {
      *cursor = p;
      return false;
   }
   if (*p == '\0') {
      fprintf(stderr, "unterminated plot assignment\n");
      abort();
   }

   size_t len = 0;
   while (*p != '\0' && *p != '"' && *p != ' ' && *p != '\t' && *p != '\\' &&
          *p != '\n' && *p != '\r') {
      invariant(len + 1 < token_size);
      token[len++] = *p++;
   }
   if (len == 0) {
      fprintf(stderr, "empty plot assignment token\n");
      abort();
   }
   token[len] = '\0';
   *cursor = p;
   return true;
}

typedef struct {
   char **names;
   u64 count;
   u64 cap;
} test_plot_name_set_t;

static char *test_strdup(const char *s) {
   invariant(s != NULL);
   size_t len = strlen(s);
   invariant(len < SIZE_MAX);
   char *copy = (char *)bst_malloc(len + 1);
   memcpy(copy, s, len + 1);
   return copy;
}

static void test_plot_name_set_insert(test_plot_name_set_t *set,
                                      const char *plot_name,
                                      const char *assignment,
                                      const char *name) {
   invariant(set != NULL);
   invariant(plot_name != NULL);
   invariant(assignment != NULL);
   invariant(name != NULL);

   for (u64 i = 0; i < set->count; i++) {
      if (strcmp(set->names[i], name) == 0) {
         fprintf(stderr, "%s plot %s repeats name: %s\n", plot_name, assignment,
                 name);
         abort();
      }
   }

   if (set->count == set->cap) {
      invariant(set->count < UINT64_MAX);
      u64 next_cap = bst_array_next_cap(set->cap, set->count + 1, 8,
                                        sizeof(set->names[0]));
      set->names =
          set->names == NULL
              ? (char **)bst_malloc_array(next_cap, sizeof(set->names[0]))
              : (char **)bst_realloc_array(set->names, next_cap,
                                           sizeof(set->names[0]));
      set->cap = next_cap;
   }

   set->names[set->count++] = test_strdup(name);
}

static void test_plot_name_set_dispose(test_plot_name_set_t *set) {
   invariant(set != NULL);
   for (u64 i = 0; i < set->count; i++) {
      free(set->names[i]);
   }
   free(set->names);
   *set = (test_plot_name_set_t){0};
}

static void assert_operation_plot_operations_match_data(const char *plot) {
   invariant(plot != NULL);

   bool seen[test_operation_data_operation_count] = {false};
   const char *p = test_plot_group_assignment("operation", plot, "OPERATIONS");
   u64 count = 0;
   for (;;) {
      char operation[128];
      if (!test_plot_assignment_next_token(&p, operation, sizeof(operation))) {
         break;
      }
      int index = test_operation_data_operation_index(operation);
      if (index < 0) {
         fprintf(stderr,
                 "operation plot OPERATIONS references unknown operation: %s\n",
                 operation);
         abort();
      }
      if (seen[index]) {
         fprintf(stderr, "operation plot OPERATIONS repeats operation: %s\n",
                 operation);
         abort();
      }
      seen[index] = true;
      count++;
   }
   invariant(*p == '"');
   if (count != test_operation_data_operation_count) {
      fprintf(stderr,
              "operation plot OPERATIONS has %llu entries, expected %d\n",
              (unsigned long long)count, test_operation_data_operation_count);
      abort();
   }
   for (int i = 0; i < test_operation_data_operation_count; i++) {
      if (!seen[i]) {
         fprintf(stderr, "operation plot OPERATIONS is missing operation: %s\n",
                 test_operation_data_operation_names[i]);
         abort();
      }
   }
}

static int test_distribution_slice_index(const dist_spec_t *const *dists,
                                         int count, const char *name) {
   invariant(dists != NULL);
   invariant(count > 0);
   invariant(name != NULL);
   for (int i = 0; i < count; i++) {
      dist_spec_require(dists[i]);
      if (strcmp(name, dists[i]->name) == 0) {
         return i;
      }
   }
   return -1;
}

static void
assert_plot_distributions_match_data(const char *plot_name, const char *plot,
                                     const dist_spec_t *const *dists,
                                     int distribution_count) {
   invariant(plot_name != NULL);
   invariant(plot != NULL);
   invariant(dists != NULL);
   invariant(distribution_count > 0);

   bool *seen =
       (bool *)bst_calloc_array((u64)distribution_count, sizeof(seen[0]));
   const char *p = test_plot_group_assignment(plot_name, plot, "DISTRIBUTIONS");
   u64 count = 0;
   for (;;) {
      char distribution[128];
      if (!test_plot_assignment_next_token(&p, distribution,
                                           sizeof(distribution))) {
         break;
      }
      int index = test_distribution_slice_index(dists, distribution_count,
                                                distribution);
      if (index < 0) {
         fprintf(
             stderr,
             "%s plot DISTRIBUTIONS references unexpected distribution: %s\n",
             plot_name, distribution);
         abort();
      }
      if (seen[index]) {
         fprintf(stderr, "%s plot DISTRIBUTIONS repeats distribution: %s\n",
                 plot_name, distribution);
         abort();
      }
      if (index != (int)count) {
         fprintf(stderr,
                 "%s plot DISTRIBUTIONS has %s at position %llu, expected %s\n",
                 plot_name, distribution, (unsigned long long)(count + 1),
                 dists[count]->name);
         abort();
      }
      seen[index] = true;
      count++;
   }
   invariant(*p == '"');
   if (count != (u64)distribution_count) {
      fprintf(stderr, "%s plot DISTRIBUTIONS has %llu entries, expected %d\n",
              plot_name, (unsigned long long)count, distribution_count);
      abort();
   }
   for (int i = 0; i < distribution_count; i++) {
      if (!seen[i]) {
         fprintf(stderr, "%s plot DISTRIBUTIONS is missing distribution: %s\n",
                 plot_name, dists[i]->name);
         abort();
      }
   }
   free(seen);
}

static bool test_line_contains(const char *line, size_t line_len,
                               const char *needle) {
   invariant(line != NULL);
   invariant(needle != NULL);
   size_t needle_len = strlen(needle);
   invariant(needle_len > 0);
   if (needle_len > line_len) {
      return false;
   }
   for (size_t i = 0; i <= line_len - needle_len; i++) {
      if (strncmp(line + i, needle, needle_len) == 0) {
         return true;
      }
   }
   return false;
}

static size_t test_plot_code_line_len(const char *line, size_t line_len) {
   invariant(line != NULL);

   bool in_single_quote = false;
   bool in_double_quote = false;
   bool escaped = false;
   for (size_t i = 0; i < line_len; i++) {
      char ch = line[i];
      if (escaped) {
         escaped = false;
         continue;
      }
      if ((in_single_quote || in_double_quote) && ch == '\\') {
         escaped = true;
         continue;
      }
      if (ch == '\'' && !in_double_quote) {
         in_single_quote = !in_single_quote;
         continue;
      }
      if (ch == '"' && !in_single_quote) {
         in_double_quote = !in_double_quote;
         continue;
      }
      if (ch == '#' && !in_single_quote && !in_double_quote) {
         return i;
      }
   }
   return line_len;
}

static bool test_header_has_field(const char *header, const char *field) {
   invariant(header != NULL);
   invariant(field != NULL);
   invariant(field[0] != '\0');

   size_t field_len = strlen(field);
   const char *p = header;
   while (*p != '\0') {
      while (test_data_field_separator(*p)) {
         p++;
      }
      if (*p == '\0') {
         break;
      }
      const char *start = p;
      while (*p != '\0' && !test_data_field_separator(*p)) {
         p++;
      }
      if ((size_t)(p - start) == field_len &&
          strncmp(start, field, field_len) == 0) {
         return true;
      }
   }
   return false;
}

static void assert_plot_line_columns_match_header(const char *plot_name,
                                                  const char *header,
                                                  const char *line,
                                                  size_t line_len) {
   invariant(plot_name != NULL);
   invariant(line != NULL);

   static const char marker[] = "column(";
   const size_t marker_len = sizeof(marker) - 1;
   bool in_single_quote = false;
   bool in_double_quote = false;
   bool escaped = false;
   for (size_t i = 0; i < line_len; i++) {
      char ch = line[i];
      if (escaped) {
         escaped = false;
         continue;
      }
      if ((in_single_quote || in_double_quote) && ch == '\\') {
         escaped = true;
         continue;
      }
      if (ch == '\'' && !in_double_quote) {
         in_single_quote = !in_single_quote;
         continue;
      }
      if (ch == '"' && !in_single_quote) {
         in_double_quote = !in_double_quote;
         continue;
      }
      if (in_single_quote || in_double_quote || i + marker_len > line_len ||
          strncmp(line + i, marker, marker_len) != 0) {
         continue;
      }
      size_t cursor = i + marker_len;
      if (cursor == line_len || (line[cursor] != '\'' && line[cursor] != '"')) {
         continue;
      }
      char quote = line[cursor++];
      if (header == NULL) {
         fprintf(
             stderr,
             "%s plot references a data column before DATA is selected: %.*s\n",
             plot_name, (int)line_len, line);
         abort();
      }
      char column[128];
      size_t len = 0;
      while (cursor < line_len && line[cursor] != quote) {
         invariant(len + 1 < sizeof(column));
         column[len++] = line[cursor++];
      }
      if (cursor == line_len || len == 0) {
         fprintf(stderr, "%s plot has malformed column reference: %.*s\n",
                 plot_name, (int)line_len, line);
         abort();
      }
      column[len] = '\0';
      if (!test_header_has_field(header, column)) {
         fprintf(stderr, "%s plot references missing data column %s in: %.*s\n",
                 plot_name, column, (int)line_len, line);
         abort();
      }
      i = cursor;
   }
}

static void assert_plot_columns_match_data(const char *plot_name,
                                           const char *plot,
                                           const char *measurement_marker,
                                           const char *measurement_header,
                                           const char *benchmark_marker,
                                           const char *benchmark_header) {
   invariant(plot_name != NULL);
   invariant(plot != NULL);
   invariant(measurement_marker != NULL);
   invariant(measurement_header != NULL);
   invariant(benchmark_marker != NULL);
   invariant(benchmark_header != NULL);

   const char *header = NULL;
   const char *line = plot;
   while (*line != '\0') {
      const char *end = strchr(line, '\n');
      size_t line_len = end == NULL ? strlen(line) : (size_t)(end - line);
      size_t code_len = test_plot_code_line_len(line, line_len);
      if (test_line_contains(line, code_len, measurement_marker)) {
         header = measurement_header;
      }
      if (test_line_contains(line, code_len, benchmark_marker)) {
         header = benchmark_header;
      }
      assert_plot_line_columns_match_header(plot_name, header, line, code_len);
      if (end == NULL) {
         break;
      }
      line = end + 1;
   }
}

static void
assert_synthetic_operation_plot_columns_match_data(const char *plot) {
   // Operation benchmark data may be checked in with the legacy schema while
   // new producers emit Steps. Plots must stay on the common column surface.
   assert_plot_columns_match_data(
       "synthetic operation", plot, "/operations/measurements/",
       test_legacy_operation_measurement_header, "/operations/benchmarks/",
       test_legacy_operation_benchmark_header);
}

static void
assert_operation_plot_group_data(const char *data_name,
                                 const test_operation_data_signature_t *sig,
                                 const char *plot, const char *group) {
   invariant(data_name != NULL);
   invariant(data_name[0] != '\0');
   invariant(sig != NULL);
   invariant(sig->trees != NULL);
   invariant(plot != NULL);
   invariant(group != NULL);

   bool *seen =
       (bool *)bst_calloc_array((u64)tree_registry_count, sizeof(seen[0]));
   const char *p = test_plot_group_assignment("operation", plot, group);
   bool saw_tree = false;
   for (;;) {
      char tree_name[128];
      if (!test_plot_assignment_next_token(&p, tree_name, sizeof(tree_name))) {
         break;
      }

      int tree_index = test_tree_registry_index(tree_name);
      if (tree_index < 0) {
         fprintf(stderr,
                 "operation plot group %s references unknown tree: %s\n", group,
                 tree_name);
         abort();
      }
      if (!sig->trees[tree_index]) {
         fprintf(stderr,
                 "operation plot group %s references missing %s tree: %s\n",
                 group, data_name, tree_name);
         abort();
      }
      if (seen[tree_index]) {
         fprintf(stderr, "operation plot group %s repeats tree: %s\n", group,
                 tree_name);
         abort();
      }
      seen[tree_index] = true;
      saw_tree = true;
   }
   invariant(*p == '"');
   invariant(saw_tree);
   free(seen);
}

static void assert_operation_plot_groups_match_data(
    const char *data_name, const test_operation_data_signature_t *sig) {
   invariant(data_name != NULL);
   invariant(data_name[0] != '\0');
   invariant(sig != NULL);

   char *plot = test_read_text_file(
       "docs/plots/benchmarks/operations/operations.gnuplot");
   assert_plot_columns_match_data(
       "operation", plot, "/operations/measurements/",
       test_legacy_operation_measurement_header, "/operations/benchmarks/",
       test_legacy_operation_benchmark_header);
   assert_operation_plot_operations_match_data(plot);
   assert_plot_distributions_match_data("operation", plot, dist_benchmark,
                                        dist_benchmark_count);
   const char *p = test_plot_group_assignment("operation", plot, "GROUPS");
   test_plot_name_set_t groups = {0};
   bool saw_group = false;
   for (;;) {
      char group[128];
      if (!test_plot_assignment_next_token(&p, group, sizeof(group))) {
         break;
      }
      test_plot_name_set_insert(&groups, "operation", "GROUPS", group);
      assert_operation_plot_group_data(data_name, sig, plot, group);
      saw_group = true;
   }
   invariant(*p == '"');
   invariant(saw_group);
   test_plot_name_set_dispose(&groups);
   free(plot);
}

static void assert_balancer_plot_group_data(const char *plot,
                                            const char *group) {
   invariant(plot != NULL);
   invariant(group != NULL);

   bool *seen =
       (bool *)bst_calloc_array((u64)balancer_registry_count, sizeof(seen[0]));
   const char *p = test_plot_group_assignment("balancer", plot, group);
   bool saw_balancer = false;
   for (;;) {
      char balancer_name[128];
      if (!test_plot_assignment_next_token(&p, balancer_name,
                                           sizeof(balancer_name))) {
         break;
      }

      int balancer_index = test_balancer_registry_index(balancer_name);
      if (balancer_index < 0) {
         fprintf(stderr,
                 "balancer plot group %s references unknown balancer: %s\n",
                 group, balancer_name);
         abort();
      }
      if (seen[balancer_index]) {
         fprintf(stderr, "balancer plot group %s repeats balancer: %s\n", group,
                 balancer_name);
         abort();
      }
      seen[balancer_index] = true;
      saw_balancer = true;
   }
   invariant(*p == '"');
   invariant(saw_balancer);
   free(seen);
}

static void assert_balancer_plot_groups_match_data(void) {
   char *plot =
       test_read_text_file("docs/plots/benchmarks/balancers/balancers.gnuplot");
   assert_plot_columns_match_data("balancer", plot, "/balancers/measurements",
                                  BENCHMARK_SCHEMA_BALANCER_MEASUREMENT_HEADER,
                                  "/balancers/benchmarks",
                                  BENCHMARK_SCHEMA_BALANCER_BENCHMARK_HEADER);
   assert_plot_distributions_match_data("balancer", plot, dist_uniform,
                                        dist_uniform_count);
   const char *p = test_plot_group_assignment("balancer", plot, "GROUPS");
   test_plot_name_set_t groups = {0};
   bool saw_group = false;
   for (;;) {
      char group[128];
      if (!test_plot_assignment_next_token(&p, group, sizeof(group))) {
         break;
      }
      test_plot_name_set_insert(&groups, "balancer", "GROUPS", group);
      assert_balancer_plot_group_data(plot, group);
      saw_group = true;
   }
   invariant(*p == '"');
   invariant(saw_group);
   test_plot_name_set_dispose(&groups);
   free(plot);
}

static void test_plot_name_set_duplicate_child(void) {
   test_plot_name_set_t names = {0};
   test_plot_name_set_insert(&names, "synthetic", "GROUPS", "A");
   test_plot_name_set_insert(&names, "synthetic", "GROUPS", "A");
   test_plot_name_set_dispose(&names);
}

static void test_operation_plot_duplicate_tree_child(void) {
   invariant(tree_registry_count > 0);
   const list_vtable_t *vt = tree_registry_all[0];
   invariant(vt != NULL);
   invariant(vt->name != NULL);

   bool *trees =
       (bool *)bst_calloc_array((u64)tree_registry_count, sizeof(trees[0]));
   trees[0] = true;
   test_operation_data_signature_t sig = {
       .trees = trees,
       .tree_files = 1,
   };

   char plot[512];
   int written =
       snprintf(plot, sizeof(plot), "G = \"%s %s\"\n", vt->name, vt->name);
   invariant(written > 0);
   invariant((size_t)written < sizeof(plot));

   assert_operation_plot_group_data("synthetic data", &sig, plot, "G");
   test_operation_data_signature_dispose(&sig);
}

static void test_balancer_plot_duplicate_balancer_child(void) {
   invariant(balancer_registry_count > 0);
   const balancer_t *balancer = balancer_registry_all[0];
   balancer_require(balancer);

   char plot[512];
   int written = snprintf(plot, sizeof(plot), "G = \"%s %s\"\n", balancer->name,
                          balancer->name);
   invariant(written > 0);
   invariant((size_t)written < sizeof(plot));

   assert_balancer_plot_group_data(plot, "G");
}

static void test_plot_distribution_order_child(void) {
   assert_plot_distributions_match_data(
       "synthetic", "DISTRIBUTIONS = \"Normal Uniform Skewed Zipf Maximum\"\n",
       dist_benchmark, dist_benchmark_count);
}

static void test_plot_duplicate_assignment_child(void) {
   (void)test_plot_group_assignment("synthetic",
                                    "GROUPS = \"A\"\n"
                                    "GROUPS = \"B\"\n",
                                    "GROUPS");
}

static void test_plot_column_before_data_child(void) {
   assert_synthetic_operation_plot_columns_match_data(
       "plot DATA using (column('Duration'))\n");
}

static void assert_plot_column_quote_variants_pass(void) {
   assert_synthetic_operation_plot_columns_match_data(
       "DATA = 'docs/benchmarks/data/operations/benchmarks/Insert/Splay'\n"
       "plot DATA using (column('Duration')), \\\n"
       "               DATA using (column(\"Duration\"))\n");
}

static void assert_plot_comments_ignored_pass(void) {
   assert_synthetic_operation_plot_columns_match_data(
       "# column('Duration') before DATA is a comment\n"
       "DATA = 'docs/benchmarks/data/operations/benchmarks/Insert/Splay'\n"
       "plot DATA using (column('Duration')) # column('DefinitelyMissing')\n");
}

static void assert_plot_quoted_column_text_ignored_pass(void) {
   assert_synthetic_operation_plot_columns_match_data(
       "DATA = 'docs/benchmarks/data/operations/benchmarks/Insert/Splay'\n"
       "plot DATA using (column('Duration')) "
       "title \"column('DefinitelyMissing')\", \\\n"
       "     DATA using 1 title 'column(\"DefinitelyMissing\")'\n");
}

static void test_plot_missing_data_column_child(void) {
   assert_synthetic_operation_plot_columns_match_data(
       "DATA = 'docs/benchmarks/data/operations/benchmarks/Insert/Splay'\n"
       "plot DATA using (column('DefinitelyMissing'))\n");
}

static void test_plot_current_only_operation_column_child(void) {
   assert_synthetic_operation_plot_columns_match_data(
       "DATA = 'docs/benchmarks/data/operations/benchmarks/Insert/Splay'\n"
       "plot DATA using (column('Steps'))\n");
}

static void test_plot_quoted_hash_before_missing_column_child(void) {
   assert_synthetic_operation_plot_columns_match_data(
       "DATA = 'docs/benchmarks/data/operations/benchmarks/Insert/Splay'\n"
       "plot DATA using (0) title \"#\", "
       "DATA using (column('DefinitelyMissing'))\n");
}

static void test_plot_malformed_column_child(void) {
   assert_synthetic_operation_plot_columns_match_data(
       "DATA = 'docs/benchmarks/data/operations/benchmarks/Insert/Splay'\n"
       "plot DATA using (column('Duration)\n");
}

static void assert_plot_validator_aborts(void (*fn)(void)) {
   invariant(fn != NULL);

   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      fn();
      _exit(0);
   }
   assert_child_aborted(pid);
}

static void assert_plot_duplicate_validators_fail(void) {
   assert_plot_validator_aborts(test_plot_name_set_duplicate_child);
   assert_plot_validator_aborts(test_operation_plot_duplicate_tree_child);
   assert_plot_validator_aborts(test_balancer_plot_duplicate_balancer_child);
   assert_plot_validator_aborts(test_plot_distribution_order_child);
   assert_plot_validator_aborts(test_plot_duplicate_assignment_child);
}

static void assert_plot_column_validators_fail(void) {
   assert_plot_validator_aborts(test_plot_column_before_data_child);
   assert_plot_validator_aborts(test_plot_missing_data_column_child);
   assert_plot_validator_aborts(test_plot_current_only_operation_column_child);
   assert_plot_validator_aborts(
       test_plot_quoted_hash_before_missing_column_child);
   assert_plot_validator_aborts(test_plot_malformed_column_child);
}

static void assert_benchmark_plot_size_normalization_contract(void) {
   char *functions = test_read_text_file("docs/plots/functions.gnuplot");
   test_require_substring(functions,
                          "path_log2_size(size) = size > 1 ? log2(size) : NaN");
   test_require_substring(functions, "mkdir(file) = sprintf(\"mkdir -p "
                                     "\\\"$(dirname \\\"%s\\\")\\\"\", file)");
   free(functions);

   char *operation_plot = test_read_text_file(
       "docs/plots/benchmarks/operations/operations.gnuplot");
   char *operation_lines =
       test_read_text_file("docs/plots/benchmarks/operations/lines.gnuplot");
   test_require_substring(operation_plot,
                          "load \"docs/plots/functions.gnuplot\"");
   test_require_substring(operation_lines,
                          "load \"docs/plots/functions.gnuplot\"");
   test_require_substring(operation_plot, "path_log2_size(column('Size'))");
   test_forbid_substring("operation plot", operation_plot,
                         "log2(column('Size'))");
   test_forbid_substring("operation plot", operation_plot,
                         "log2(column(\"Size\"))");
   free(operation_lines);
   free(operation_plot);

   char *balancer_plot =
       test_read_text_file("docs/plots/benchmarks/balancers/balancers.gnuplot");
   char *balancer_lines =
       test_read_text_file("docs/plots/benchmarks/balancers/lines.gnuplot");
   test_require_substring(balancer_plot,
                          "load \"docs/plots/functions.gnuplot\"");
   test_require_substring(balancer_lines,
                          "load \"docs/plots/functions.gnuplot\"");
   test_require_substring(balancer_plot, "path_log2_size(column('Size'))");
   test_forbid_substring("balancer plot", balancer_plot,
                         "log2(column('Size'))");
   test_forbid_substring("balancer plot", balancer_plot,
                         "log2(column(\"Size\"))");
   free(balancer_lines);
   free(balancer_plot);
}

static DIR *test_opendir_or_abort(const char *path) {
   invariant(path != NULL);
   DIR *dir = opendir(path);
   if (dir == NULL) {
      perror(path);
      abort();
   }
   return dir;
}

static void test_closedir_or_abort(DIR *dir, const char *path) {
   invariant(dir != NULL);
   invariant(path != NULL);
   if (closedir(dir) != 0) {
      perror(path);
      abort();
   }
}

static void test_readdir_error_or_abort(const char *path) {
   invariant(path != NULL);
   if (errno != 0) {
      perror(path);
      abort();
   }
}

static test_operation_data_signature_t
assert_operation_data_schema(const char *data_name, const char *root,
                             const char *expected_header, bool has_iterations,
                             bool allow_known_partials) {
   invariant(data_name != NULL);
   invariant(root != NULL);
   invariant(expected_header != NULL);

   DIR *ops = test_opendir_or_abort(root);
   bool seen_operations[test_operation_data_operation_count] = {false};
   bool *expected_trees = NULL;
   u64 expected_tree_files = 0;
   u64 files = 0;
   struct dirent *op_entry;
   for (;;) {
      errno = 0;
      op_entry = readdir(ops);
      if (op_entry == NULL) {
         break;
      }
      if (test_data_hidden_entry(op_entry->d_name)) {
         continue;
      }
      int op_index = test_operation_data_operation_index(op_entry->d_name);
      if (op_index < 0) {
         fprintf(stderr, "unexpected operation data directory: %s/%s\n", root,
                 op_entry->d_name);
         abort();
      }
      invariant(!seen_operations[op_index]);
      seen_operations[op_index] = true;

      char op_path[512];
      path_format(op_path, sizeof(op_path), "%s/%s", root, op_entry->d_name);
      if (!path_is_directory(op_path)) {
         fprintf(stderr, "operation data entry is not a directory: %s\n",
                 op_path);
         abort();
      }

      DIR *trees = test_opendir_or_abort(op_path);
      invariant(tree_registry_count > 0);
      bool *tree_seen = (bool *)bst_calloc_array((u64)tree_registry_count,
                                                 sizeof(*tree_seen));
      u64 tree_files = 0;
      struct dirent *tree_entry;
      for (;;) {
         errno = 0;
         tree_entry = readdir(trees);
         if (tree_entry == NULL) {
            break;
         }
         if (test_data_hidden_entry(tree_entry->d_name)) {
            continue;
         }

         int tree_index = test_tree_registry_index(tree_entry->d_name);
         if (tree_index < 0) {
            fprintf(stderr, "unexpected operation data tree: %s/%s\n", op_path,
                    tree_entry->d_name);
            abort();
         }
         invariant(!tree_seen[tree_index]);
         tree_seen[tree_index] = true;

         char data_path[512];
         path_format(data_path, sizeof(data_path), "%s/%s", op_path,
                     tree_entry->d_name);
         if (path_is_directory(data_path)) {
            fprintf(stderr, "operation data tree entry is a directory: %s\n",
                    data_path);
            abort();
         }

         if (has_iterations) {
            const char *headers[] = {
                expected_header,
                test_legacy_operation_benchmark_header,
            };
            assert_data_file_schema_one_of(
                data_path, headers,
                (int)(sizeof(headers) / sizeof(headers[0])));
         } else {
            const char *headers[] = {
                expected_header,
                test_legacy_operation_measurement_header,
            };
            assert_data_file_schema_one_of(
                data_path, headers,
                (int)(sizeof(headers) / sizeof(headers[0])));
         }
         assert_operation_data_distribution_coverage(
             data_name, op_entry->d_name, tree_entry->d_name, data_path,
             has_iterations,
             has_iterations ? test_operation_data_benchmark_iterations : 0,
             allow_known_partials);
         tree_files++;
      }
      test_readdir_error_or_abort(op_path);
      test_closedir_or_abort(trees, op_path);

      invariant(tree_files > 0);
      if (expected_trees == NULL) {
         expected_trees = tree_seen;
         expected_tree_files = tree_files;
      } else {
         if (tree_files != expected_tree_files) {
            fprintf(stderr, "operation data tree count mismatch in %s\n",
                    op_path);
            abort();
         }
         for (int i = 0; i < tree_registry_count; i++) {
            if (tree_seen[i] != expected_trees[i]) {
               const char *tree_name = tree_registry_all[i]->name;
               fprintf(stderr,
                       "operation data tree set mismatch in %s: %s is %s "
                       "but expected %s\n",
                       op_path, tree_name, tree_seen[i] ? "present" : "absent",
                       expected_trees[i] ? "present" : "absent");
               abort();
            }
         }
         free(tree_seen);
      }
      files += tree_files;
   }
   test_readdir_error_or_abort(root);
   test_closedir_or_abort(ops, root);
   for (int i = 0; i < test_operation_data_operation_count; i++) {
      if (!seen_operations[i]) {
         fprintf(stderr, "missing operation data directory: %s/%s\n", root,
                 test_operation_data_operation_names[i]);
         abort();
      }
   }
   invariant(files > 0);
   invariant(expected_trees != NULL);
   return (test_operation_data_signature_t){
       .trees = expected_trees,
       .tree_files = expected_tree_files,
   };
}

typedef enum {
   test_balancer_data_benchmark,
   test_balancer_data_measurement,
} test_balancer_data_kind_t;

static bool test_known_partial_balancer_data(const char *balancer_name,
                                             test_balancer_data_kind_t kind,
                                             u64 samples, u64 last_size) {
   invariant(balancer_name != NULL);
   return kind == test_balancer_data_measurement &&
          strcmp(balancer_name, "Median") == 0 && samples == 249 &&
          last_size == 249000;
}

static void assert_balancer_benchmark_row(const char *path, const char *line,
                                          u64 iterations, u64 average_duration,
                                          u64 total_duration) {
   if (iterations == 0 || average_duration == 0 || total_duration == 0) {
      fprintf(stderr, "nonpositive balancer benchmark value in %s: %s", path,
              line);
      abort();
   }
   if (total_duration < iterations ||
       average_duration !=
           duration_average_nonzero(total_duration, iterations)) {
      fprintf(stderr, "inconsistent balancer benchmark duration in %s: %s",
              path, line);
      abort();
   }
}

static void assert_balancer_measurement_row(const char *path, const char *line,
                                            u64 size, u64 iteration,
                                            u64 partition_count,
                                            u64 partition_depth,
                                            double average_path_length,
                                            int maximum_path_length) {
   if (iteration == 0 || !isfinite(average_path_length) ||
       average_path_length < 0.0 || maximum_path_length < 0 ||
       average_path_length > (double)maximum_path_length ||
       (u64)maximum_path_length >= size ||
       size > nodePathMetricsMaximumNodesForHeight((u64)maximum_path_length) ||
       measurement_average_path_length_too_low(size, average_path_length,
                                               maximum_path_length) ||
       measurement_average_path_length_too_high(size, average_path_length,
                                                maximum_path_length) ||
       (partition_count == 0 && partition_depth != 0)) {
      fprintf(stderr, "invalid balancer measurement value in %s: %s", path,
              line);
      abort();
   }
}

static void test_balancer_measurement_height_too_low_child(void) {
   assert_balancer_measurement_row("synthetic", "Uniform 4 4 1 0 0 1 1 0 0\n",
                                   4, 1, 0, 0, 1.0, 1);
}

static void test_balancer_measurement_empty_size_child(void) {
   assert_balancer_measurement_row("synthetic", "Uniform 0 0 1 0 0 0 0 0 0\n",
                                   0, 1, 0, 0, 0.0, 0);
}

static void test_balancer_measurement_zero_iteration_child(void) {
   assert_balancer_measurement_row("synthetic", "Uniform 1 1 0 0 0 0 0 0 0\n",
                                   1, 0, 0, 0, 0.0, 0);
}

static void test_balancer_measurement_average_too_low_child(void) {
   assert_balancer_measurement_row("synthetic", "Uniform 4 4 1 0 0 0.5 2 0 0\n",
                                   4, 1, 0, 0, 0.5, 2);
}

static void test_balancer_measurement_average_too_high_child(void) {
   assert_balancer_measurement_row("synthetic", "Uniform 4 4 1 0 0 1.5 2 0 0\n",
                                   4, 1, 0, 0, 1.5, 2);
}

static void test_balancer_measurement_average_exceeds_max_child(void) {
   assert_balancer_measurement_row("synthetic", "Uniform 3 3 1 0 0 2.5 2 0 0\n",
                                   3, 1, 0, 0, 2.5, 2);
}

static void test_balancer_measurement_orphan_partition_depth_child(void) {
   assert_balancer_measurement_row("synthetic", "Uniform 3 3 1 0 1 1 2 0 0\n",
                                   3, 1, 0, 1, 1.0, 2);
}

static void
assert_balancer_measurement_row_validator_accepts_root_partition(void) {
   assert_balancer_measurement_row("synthetic", "Uniform 1 1 1 1 0 0 0 0 0\n",
                                   1, 1, 1, 0, 0.0, 0);
}

static void assert_balancer_measurement_row_validator_aborts(void) {
   assert_benchmark_data_parser_aborts(
       test_balancer_measurement_height_too_low_child);
   assert_benchmark_data_parser_aborts(
       test_balancer_measurement_empty_size_child);
   assert_benchmark_data_parser_aborts(
       test_balancer_measurement_zero_iteration_child);
   assert_benchmark_data_parser_aborts(
       test_balancer_measurement_average_too_low_child);
   assert_benchmark_data_parser_aborts(
       test_balancer_measurement_average_too_high_child);
   assert_benchmark_data_parser_aborts(
       test_balancer_measurement_average_exceeds_max_child);
   assert_benchmark_data_parser_aborts(
       test_balancer_measurement_orphan_partition_depth_child);
}

static void assert_balancer_data_schedule(const char *path,
                                          const char *balancer_name,
                                          const dist_spec_t *expected_dist,
                                          test_balancer_data_kind_t kind) {
   invariant(path != NULL);
   invariant(balancer_name != NULL);
   dist_spec_require(expected_dist);

   FILE *file = fopen_or_exit(path, "r");
   char line[TEST_DATA_LINE_MAX];
   invariant(test_fgets_or_abort(line, sizeof(line), file, path));
   test_data_file_require_complete_line(file, path, line);
   test_trim_line_end(line);
   const char *expected_header =
       kind == test_balancer_data_benchmark
           ? BENCHMARK_SCHEMA_BALANCER_BENCHMARK_HEADER
           : BENCHMARK_SCHEMA_BALANCER_MEASUREMENT_HEADER;
   bool current_header = strcmp(line, expected_header) == 0;
   bool legacy_measurement =
       kind == test_balancer_data_measurement &&
       strcmp(line, test_legacy_balancer_measurement_header) == 0;
   if (!current_header && !legacy_measurement) {
      fprintf(stderr, "unexpected balancer data header in %s: %s\n", path,
              line);
      abort();
   }

   u64 rows = 0;
   u64 data_scale = 0;
   u64 last_size = 0;
   u64 last_iteration = 0;
   u64 expected_iterations = 0;
   test_operation_data_positions_t positions = {0};
   while (test_fgets_or_abort(line, sizeof(line), file, path)) {
      test_data_file_require_complete_line(file, path, line);
      int expected_fields =
          kind == test_balancer_data_benchmark
              ? BENCHMARK_SCHEMA_BALANCER_BENCHMARK_FIELDS
          : legacy_measurement
              ? test_data_field_count(test_legacy_balancer_measurement_header)
              : BENCHMARK_SCHEMA_BALANCER_MEASUREMENT_FIELDS;
      test_data_row_t row = test_data_row_parse(path, line, expected_fields);
      const char *distribution = test_data_row_string(&row, 0, "Distribution");
      u64 scale = test_data_row_u64(&row, 1, "Scale");
      u64 size = test_data_row_u64(&row, 2, "Size");
      const char *iteration_field =
          kind == test_balancer_data_benchmark ? "Iterations" : "Iteration";
      u64 iteration = test_data_row_u64(&row, 3, iteration_field);
      if (kind == test_balancer_data_benchmark) {
         u64 average_duration = test_data_row_u64(&row, 4, "AverageDuration");
         u64 total_duration = test_data_row_u64(&row, 5, "TotalDuration");
         assert_balancer_benchmark_row(path, line, iteration, average_duration,
                                       total_duration);
      } else {
         u64 partition_count = test_data_row_u64(&row, 4, "PartitionCount");
         u64 partition_depth = test_data_row_u64(&row, 5, "PartitionDepth");
         double average_path_length =
             test_data_row_double(&row, 6, "AveragePathLength");
         int maximum_path_length =
             test_data_row_nonnegative_int(&row, 7, "MaximumPathLength");
         (void)test_data_row_u64(&row, 8, "Rotations");
         if (!legacy_measurement) {
            (void)test_data_row_u64(&row, 9, "Allocations");
         }
         assert_balancer_measurement_row(
             path, line, size, iteration, partition_count, partition_depth,
             average_path_length, maximum_path_length);
      }
      if (strcmp(distribution, expected_dist->name) != 0) {
         fprintf(stderr,
                 "unexpected balancer data distribution in %s: expected %s, "
                 "got %s\n",
                 path, expected_dist->name, distribution);
         abort();
      }
      if (scale == 0 || size == 0) {
         fprintf(stderr, "nonpositive balancer data schedule value in %s: %s",
                 path, line);
         abort();
      }
      if (data_scale == 0) {
         data_scale = scale;
      } else if (scale != data_scale) {
         fprintf(stderr, "inconsistent balancer data scale in %s: %s", path,
                 line);
         abort();
      }
      if (size > data_scale) {
         fprintf(stderr, "nonsequential balancer data size in %s: %s", path,
                 line);
         abort();
      }
      if (kind == test_balancer_data_benchmark) {
         if (size <= last_size) {
            fprintf(stderr, "nonsequential balancer data size in %s: %s", path,
                    line);
            abort();
         }
         test_operation_data_positions_push(&positions, size);
         last_size = size;
      } else {
         if (size < last_size) {
            fprintf(stderr, "nonsequential balancer data size in %s: %s", path,
                    line);
            abort();
         }
         if (size == last_size) {
            u64 expected_iteration = size_after_inserting_one(last_iteration);
            if (iteration != expected_iteration) {
               fprintf(stderr,
                       "nonsequential balancer measurement iteration in %s: %s",
                       path, line);
               abort();
            }
         } else {
            if (iteration != 1) {
               fprintf(stderr,
                       "balancer measurement size starts at iteration %llu "
                       "in %s: %s",
                       (unsigned long long)iteration, path, line);
               abort();
            }
            if (last_size != 0) {
               if (expected_iterations == 0) {
                  expected_iterations = last_iteration;
               } else if (last_iteration != expected_iterations) {
                  fprintf(stderr,
                          "inconsistent balancer measurement iteration count "
                          "in %s: %s",
                          path, line);
                  abort();
               }
            }
            test_operation_data_positions_push(&positions, size);
            last_size = size;
         }
         last_iteration = iteration;
      }
      rows++;
   }
   fclose_or_exit(file, path);

   invariant(rows > 0);
   if (kind == test_balancer_data_measurement) {
      if (expected_iterations != 0 && last_iteration != expected_iterations) {
         fprintf(stderr,
                 "inconsistent balancer measurement final iteration count "
                 "in %s\n",
                 path);
         abort();
      }
   } else {
      invariant(positions.count == rows);
   }
   invariant(positions.positions != NULL);
   bool known_partial = test_known_partial_balancer_data(
       balancer_name, kind, positions.count, last_size);

   u64 scheduled_samples = positions.count;
   if (last_size != data_scale) {
      scheduled_samples = test_infer_sample_schedule_from_position(
          data_scale, positions.count, last_size);
      if (scheduled_samples == 0) {
         fprintf(stderr,
                 "cannot infer balancer data sample schedule in %s: "
                 "rows=%llu last_size=%llu scale=%llu\n",
                 path, (unsigned long long)rows, (unsigned long long)last_size,
                 (unsigned long long)data_scale);
         abort();
      }
   }
   invariant(scheduled_samples > 0);
   if (positions.count > scheduled_samples) {
      fprintf(stderr,
              "too many balancer data rows in %s: rows=%llu samples=%llu\n",
              path, (unsigned long long)positions.count,
              (unsigned long long)scheduled_samples);
      abort();
   }
   sample_schedule_t schedule = {
       .total = data_scale,
       .samples = scheduled_samples,
   };
   sample_schedule_require(schedule);
   for (u64 i = 0; i < positions.count; i++) {
      sample_schedule_point_t point = sample_schedule_point(schedule, i);
      u64 expected_size = point.position;
      if (positions.positions[i] != expected_size) {
         fprintf(stderr,
                 "off-schedule balancer data size in %s row %llu: "
                 "expected %llu, got %llu\n",
                 path, (unsigned long long)point.ordinal,
                 (unsigned long long)expected_size,
                 (unsigned long long)positions.positions[i]);
         abort();
      }
   }
   if (last_size != data_scale && !known_partial) {
      fprintf(stderr,
              "incomplete balancer data coverage in %s: rows=%llu "
              "last_size=%llu scale=%llu\n",
              path, (unsigned long long)rows, (unsigned long long)last_size,
              (unsigned long long)data_scale);
      abort();
   }
   test_operation_data_positions_dispose(&positions);
}

static void assert_balancer_data_accepts_measurement_iterations(void) {
   char output_dir[] = "/tmp/bst-balancer-data-iterations-XXXXXX";
   char *root = mkdtemp(output_dir);
   invariant(root != NULL);

   char path[512];
   path_format(path, sizeof(path), "%s/measurements.txt", root);

   FILE *file = fopen_or_exit(path, "w");
   file_printf_or_exit(file, path, "%s\n",
                       BENCHMARK_SCHEMA_BALANCER_MEASUREMENT_HEADER);
   for (u64 size = 2; size <= 6; size += 2) {
      u64 maximum_path_length = size > 2 ? 2 : 1;
      double average_path_length =
          (double)nodePathMetricsMinimumTotalDepthForHeight(
              size, maximum_path_length) /
          (double)size;
      for (u64 iteration = 1; iteration <= 2; iteration++) {
         file_printf_or_exit(
             file, path, "Normal 6 %llu %llu 0 0 %.17g %llu 0 0\n",
             (unsigned long long)size, (unsigned long long)iteration,
             average_path_length, (unsigned long long)maximum_path_length);
      }
   }
   fclose_or_exit(file, path);

   assert_balancer_data_schedule(path, "Height", dist_find_or_exit("Normal"),
                                 test_balancer_data_measurement);

   invariant(remove(path) == 0);
   invariant(rmdir(root) == 0);
}

static void assert_balancer_data_accepts_legacy_measurement_header(void) {
   char output_dir[] = "/tmp/bst-balancer-data-legacy-measurement-XXXXXX";
   char *root = mkdtemp(output_dir);
   invariant(root != NULL);

   char path[512];
   path_format(path, sizeof(path), "%s/measurements.txt", root);

   FILE *file = fopen_or_exit(path, "w");
   file_printf_or_exit(file, path, "%s\n",
                       test_legacy_balancer_measurement_header);
   file_printf_or_exit(file, path, "Normal 6 2 1 0 0 0.5 1 0\n");
   file_printf_or_exit(file, path, "Normal 6 4 1 0 0 1 2 0\n");
   file_printf_or_exit(file, path, "Normal 6 6 1 0 0 1.3333333333333333 2 0\n");
   fclose_or_exit(file, path);

   assert_balancer_data_schedule(path, "Height", dist_find_or_exit("Normal"),
                                 test_balancer_data_measurement);

   invariant(remove(path) == 0);
   invariant(rmdir(root) == 0);
}

static void assert_balancer_data_accepts_benchmark_distribution(void) {
   char output_dir[] = "/tmp/bst-balancer-data-benchmark-dist-XXXXXX";
   char *root = mkdtemp(output_dir);
   invariant(root != NULL);

   char path[512];
   path_format(path, sizeof(path), "%s/benchmarks.txt", root);

   FILE *file = fopen_or_exit(path, "w");
   file_printf_or_exit(file, path, "%s\n",
                       BENCHMARK_SCHEMA_BALANCER_BENCHMARK_HEADER);
   file_printf_or_exit(file, path, "Normal 6 2 3 5 15\n");
   file_printf_or_exit(file, path, "Normal 6 4 4 7 28\n");
   file_printf_or_exit(file, path, "Normal 6 6 5 11 55\n");
   fclose_or_exit(file, path);

   assert_balancer_data_schedule(path, "Height", dist_find_or_exit("Normal"),
                                 test_balancer_data_benchmark);

   invariant(remove(path) == 0);
   invariant(rmdir(root) == 0);
}

static void test_write_off_schedule_balancer_benchmark_data(const char *path) {
   invariant(path != NULL);

   FILE *file = fopen_or_exit(path, "w");
   file_printf_or_exit(file, path, "%s\n",
                       BENCHMARK_SCHEMA_BALANCER_BENCHMARK_HEADER);
   file_printf_or_exit(file, path, "Uniform 6 2 3 5 15\n");
   file_printf_or_exit(file, path, "Uniform 6 5 4 7 28\n");
   file_printf_or_exit(file, path, "Uniform 6 6 5 11 55\n");
   fclose_or_exit(file, path);
}

static void
test_write_unexpected_distribution_balancer_benchmark_data(const char *path) {
   invariant(path != NULL);

   FILE *file = fopen_or_exit(path, "w");
   file_printf_or_exit(file, path, "%s\n",
                       BENCHMARK_SCHEMA_BALANCER_BENCHMARK_HEADER);
   file_printf_or_exit(file, path, "Normal 6 2 3 5 15\n");
   file_printf_or_exit(file, path, "Normal 6 4 4 7 28\n");
   file_printf_or_exit(file, path, "Normal 6 6 5 11 55\n");
   fclose_or_exit(file, path);
}

static void
test_write_inconsistent_duration_balancer_benchmark_data(const char *path) {
   invariant(path != NULL);

   FILE *file = fopen_or_exit(path, "w");
   file_printf_or_exit(file, path, "%s\n",
                       BENCHMARK_SCHEMA_BALANCER_BENCHMARK_HEADER);
   file_printf_or_exit(file, path, "Uniform 6 2 3 6 15\n");
   file_printf_or_exit(file, path, "Uniform 6 4 4 7 28\n");
   file_printf_or_exit(file, path, "Uniform 6 6 5 11 55\n");
   fclose_or_exit(file, path);
}

static void
test_write_impossible_duration_balancer_benchmark_data(const char *path) {
   invariant(path != NULL);

   FILE *file = fopen_or_exit(path, "w");
   file_printf_or_exit(file, path, "%s\n",
                       BENCHMARK_SCHEMA_BALANCER_BENCHMARK_HEADER);
   file_printf_or_exit(file, path, "Uniform 6 2 3 1 2\n");
   file_printf_or_exit(file, path, "Uniform 6 4 4 7 28\n");
   file_printf_or_exit(file, path, "Uniform 6 6 5 11 55\n");
   fclose_or_exit(file, path);
}

static void test_write_wrong_header_balancer_benchmark_data(const char *path) {
   invariant(path != NULL);

   FILE *file = fopen_or_exit(path, "w");
   file_printf_or_exit(
       file, path,
       "Distribution Scale Size Iterations AverageDuration Duration\n");
   file_printf_or_exit(file, path, "Uniform 6 2 3 5 15\n");
   file_printf_or_exit(file, path, "Uniform 6 4 4 7 28\n");
   file_printf_or_exit(file, path, "Uniform 6 6 5 11 55\n");
   fclose_or_exit(file, path);
}

static void test_write_unterminated_balancer_benchmark_data(const char *path) {
   invariant(path != NULL);

   FILE *file = fopen_or_exit(path, "w");
   file_printf_or_exit(file, path, "%s\n",
                       BENCHMARK_SCHEMA_BALANCER_BENCHMARK_HEADER);
   file_printf_or_exit(file, path, "Uniform 6 2 3 5 15\n");
   file_printf_or_exit(file, path, "Uniform 6 4 4 7 28\n");
   file_printf_or_exit(file, path, "Uniform 6 6 5 11 55");
   fclose_or_exit(file, path);
}

static void
test_write_inconsistent_iteration_balancer_measurement_data(const char *path) {
   invariant(path != NULL);

   FILE *file = fopen_or_exit(path, "w");
   file_printf_or_exit(file, path, "%s\n",
                       BENCHMARK_SCHEMA_BALANCER_MEASUREMENT_HEADER);
   file_printf_or_exit(file, path, "Uniform 6 2 1 0 0 1 1 0 0\n");
   file_printf_or_exit(file, path, "Uniform 6 2 2 0 0 1 1 0 0\n");
   file_printf_or_exit(file, path, "Uniform 6 4 1 0 0 1 1 0 0\n");
   file_printf_or_exit(file, path, "Uniform 6 6 1 0 0 1 1 0 0\n");
   file_printf_or_exit(file, path, "Uniform 6 6 2 0 0 1 1 0 0\n");
   fclose_or_exit(file, path);
}

static void test_write_inconsistent_final_iteration_balancer_measurement_data(
    const char *path) {
   invariant(path != NULL);

   FILE *file = fopen_or_exit(path, "w");
   file_printf_or_exit(file, path, "%s\n",
                       BENCHMARK_SCHEMA_BALANCER_MEASUREMENT_HEADER);
   file_printf_or_exit(file, path, "Uniform 6 2 1 0 0 1 1 0 0\n");
   file_printf_or_exit(file, path, "Uniform 6 2 2 0 0 1 1 0 0\n");
   file_printf_or_exit(file, path, "Uniform 6 4 1 0 0 1 1 0 0\n");
   file_printf_or_exit(file, path, "Uniform 6 4 2 0 0 1 1 0 0\n");
   file_printf_or_exit(file, path, "Uniform 6 6 1 0 0 1 1 0 0\n");
   fclose_or_exit(file, path);
}

static void
test_write_wrong_header_balancer_measurement_data(const char *path) {
   invariant(path != NULL);

   FILE *file = fopen_or_exit(path, "w");
   file_printf_or_exit(
       file, path,
       "Distribution Scale Size Iteration PartitionCount PartitionDepth "
       "AveragePathLength MaximumPathLength Rotations AllocationCount\n");
   file_printf_or_exit(file, path, "Uniform 6 2 1 0 0 1 1 0 0\n");
   file_printf_or_exit(file, path, "Uniform 6 4 1 0 0 1 1 0 0\n");
   file_printf_or_exit(file, path, "Uniform 6 6 1 0 0 1 1 0 0\n");
   fclose_or_exit(file, path);
}

static void
test_write_impossible_path_balancer_measurement_data(const char *path) {
   invariant(path != NULL);

   FILE *file = fopen_or_exit(path, "w");
   file_printf_or_exit(file, path, "%s\n",
                       BENCHMARK_SCHEMA_BALANCER_MEASUREMENT_HEADER);
   file_printf_or_exit(file, path, "Uniform 2 2 1 0 0 1 2 0 0\n");
   fclose_or_exit(file, path);
}

typedef void (*test_write_balancer_data_fn)(const char *path);

static void
assert_balancer_data_validator_aborts(test_write_balancer_data_fn write_data,
                                      test_balancer_data_kind_t kind) {
   invariant(write_data != NULL);

   char output_dir[] = "/tmp/bst-balancer-data-invalid-XXXXXX";
   char *root = mkdtemp(output_dir);
   invariant(root != NULL);

   char path[512];
   path_format(path, sizeof(path), "%s/invalid.txt", root);
   write_data(path);

   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      assert_balancer_data_schedule(path, "Median", dist_uniform[0], kind);
      _exit(0);
   }
   assert_child_aborted(pid);

   invariant(remove(path) == 0);
   invariant(rmdir(root) == 0);
}

static void assert_balancer_data_schedule_validator_aborts(void) {
   assert_balancer_data_validator_aborts(
       test_write_off_schedule_balancer_benchmark_data,
       test_balancer_data_benchmark);
   assert_balancer_data_validator_aborts(
       test_write_unexpected_distribution_balancer_benchmark_data,
       test_balancer_data_benchmark);
   assert_balancer_data_validator_aborts(
       test_write_inconsistent_duration_balancer_benchmark_data,
       test_balancer_data_benchmark);
   assert_balancer_data_validator_aborts(
       test_write_impossible_duration_balancer_benchmark_data,
       test_balancer_data_benchmark);
   assert_balancer_data_validator_aborts(
       test_write_wrong_header_balancer_benchmark_data,
       test_balancer_data_benchmark);
   assert_balancer_data_validator_aborts(
       test_write_unterminated_balancer_benchmark_data,
       test_balancer_data_benchmark);
   assert_balancer_data_validator_aborts(
       test_write_inconsistent_iteration_balancer_measurement_data,
       test_balancer_data_measurement);
   assert_balancer_data_validator_aborts(
       test_write_inconsistent_final_iteration_balancer_measurement_data,
       test_balancer_data_measurement);
   assert_balancer_data_validator_aborts(
       test_write_wrong_header_balancer_measurement_data,
       test_balancer_data_measurement);
   assert_balancer_data_validator_aborts(
       test_write_impossible_path_balancer_measurement_data,
       test_balancer_data_measurement);
}

static void assert_balancer_data_schema(const char *root,
                                        const char *expected_header,
                                        test_balancer_data_kind_t kind) {
   invariant(root != NULL);
   invariant(expected_header != NULL);

   DIR *dir = test_opendir_or_abort(root);
   u64 files = 0;
   struct dirent *entry;
   for (;;) {
      errno = 0;
      entry = readdir(dir);
      if (entry == NULL) {
         break;
      }
      if (test_data_hidden_entry(entry->d_name)) {
         continue;
      }

      const balancer_t *balancer = balancer_registry_find(entry->d_name);
      if (balancer == NULL) {
         fprintf(stderr, "unexpected balancer data file: %s/%s\n", root,
                 entry->d_name);
         abort();
      }

      char path[512];
      path_format(path, sizeof(path), "%s/%s", root, balancer->name);
      if (path_is_directory(path)) {
         fprintf(stderr, "balancer data entry is a directory: %s\n", path);
         abort();
      }

      if (kind == test_balancer_data_measurement) {
         const char *headers[] = {
             expected_header,
             test_legacy_balancer_measurement_header,
         };
         assert_data_file_schema_one_of(
             path, headers, (int)(sizeof(headers) / sizeof(headers[0])));
      } else {
         assert_data_file_schema(path, expected_header);
      }
      invariant(dist_uniform_count == 1);
      assert_balancer_data_schedule(path, balancer->name, dist_uniform[0],
                                    kind);
      files++;
   }
   test_readdir_error_or_abort(root);
   test_closedir_or_abort(dir, root);
   invariant(files == (u64)balancer_registry_count);
}

static void assert_balanced_latin_square(int candidate_count) {
   invariant(candidate_count > 0);
   u64 period = benchmark_balanced_latin_square_period(candidate_count);
   u64 expected = period / (u64)candidate_count;
   u64 matrix_size = (u64)candidate_count * (u64)candidate_count;
   u64 *position_counts =
       (u64 *)bst_calloc_array(matrix_size, sizeof(position_counts[0]));
   u64 *carryover_counts =
       (u64 *)bst_calloc_array(matrix_size, sizeof(carryover_counts[0]));
   bool *seen = (bool *)bst_calloc_array((u64)candidate_count, sizeof(seen[0]));

   for (u64 trial = 0; trial < period; trial++) {
      memset(seen, 0, (size_t)candidate_count * sizeof(seen[0]));
      int previous = -1;
      for (int position = 0; position < candidate_count; position++) {
         int candidate = benchmark_balanced_latin_square_index(candidate_count,
                                                               trial, position);
         invariant(candidate >= 0);
         invariant(candidate < candidate_count);
         invariant(!seen[candidate]);
         seen[candidate] = true;

         u64 position_index =
             (u64)candidate * (u64)candidate_count + (u64)position;
         position_counts[position_index]++;
         if (previous >= 0) {
            u64 carryover_index =
                (u64)previous * (u64)candidate_count + (u64)candidate;
            carryover_counts[carryover_index]++;
         }
         previous = candidate;

         int repeated = benchmark_balanced_latin_square_index(
             candidate_count, trial + period, position);
         assert_int_equal(candidate, repeated);
      }
   }

   for (int candidate = 0; candidate < candidate_count; candidate++) {
      for (int position = 0; position < candidate_count; position++) {
         u64 index = (u64)candidate * (u64)candidate_count + (u64)position;
         assert_equal(expected, position_counts[index]);
      }
      for (int next = 0; next < candidate_count; next++) {
         u64 index = (u64)candidate * (u64)candidate_count + (u64)next;
         assert_equal(candidate == next ? 0 : expected,
                      carryover_counts[index]);
      }
   }

   free(seen);
   free(carryover_counts);
   free(position_counts);
}

static void TestBenchmarkOrder(void) {
   printf("Benchmark order:\n");
   for (int candidate_count = 1; candidate_count <= 25; candidate_count++) {
      assert_balanced_latin_square(candidate_count);
   }
   printf("  balanced Latin squares for 1 through 25 candidates OK\n");
}

static void TestBenchmarkData(void) {
   printf("Benchmark data:\n");

   assert_benchmark_data_parser_rejects_malformed_numbers();
   printf("  strict row parser rejects malformed numbers OK\n");

   assert_benchmark_schema_field_counts();
   printf("  benchmark schema field counts OK\n");

   assert_data_file_schema_rejects_empty_rows();
   printf("  data schema rejects empty row sets OK\n");

   assert_data_file_schema_rejects_long_header();
   printf("  data schema rejects overlong headers OK\n");

   assert_data_file_schema_rejects_bad_row_arity();
   printf("  data schema rejects malformed row widths OK\n");

   assert_data_file_schema_rejects_long_data_row();
   printf("  data schema rejects overlong data rows OK\n");

   assert_data_file_schema_rejects_unterminated_lines();
   printf("  data schema rejects unterminated lines OK\n");

   assert_data_file_schema_rejects_nul_bytes();
   printf("  data schema rejects NUL bytes OK\n");

   assert_operation_measurement_row_validator_accepts_root_partition();
   assert_balancer_measurement_row_validator_accepts_root_partition();
   printf("  measurement row validators accept root partitions OK\n");

   assert_operation_measurement_row_validator_aborts();
   assert_balancer_measurement_row_validator_aborts();
   printf("  measurement row validators reject invalid counters OK\n");

   assert_operation_data_accepts_independent_scale_and_steps();
   printf("  operation data independent scale/steps schema OK\n");

   assert_operation_data_accepts_current_insert_delete_schedule();
   printf("  operation data current insert-delete schedules OK\n");

   assert_operation_data_schedule_validator_aborts();
   printf("  operation data sample schedule validator OK\n");

   assert_operation_data_duration_validator_aborts();
   printf("  operation benchmark duration validator OK\n");

   assert_operation_data_rejects_distribution_outer_order();
   printf("  operation benchmark row order validator OK\n");

   assert_balancer_data_accepts_measurement_iterations();
   printf("  balancer measurement iteration schema OK\n");

   assert_balancer_data_accepts_legacy_measurement_header();
   printf("  legacy balancer measurement schema compatibility OK\n");

   assert_balancer_data_accepts_benchmark_distribution();
   printf("  balancer benchmark distribution schema OK\n");

   assert_balancer_data_schedule_validator_aborts();
   printf("  balancer data schedule validator rejects malformed data OK\n");

   assert_balancer_data_schema("docs/benchmarks/data/balancers/measurements",
                               BENCHMARK_SCHEMA_BALANCER_MEASUREMENT_HEADER,
                               test_balancer_data_measurement);
   assert_balancer_data_schema("docs/benchmarks/data/balancers/benchmarks",
                               BENCHMARK_SCHEMA_BALANCER_BENCHMARK_HEADER,
                               test_balancer_data_benchmark);
   assert_balancer_plot_groups_match_data();
   assert_plot_duplicate_validators_fail();
   assert_plot_column_quote_variants_pass();
   assert_plot_comments_ignored_pass();
   assert_plot_quoted_column_text_ignored_pass();
   assert_plot_column_validators_fail();
   assert_benchmark_plot_size_normalization_contract();

   printf("  balancer measurement and benchmark data OK\n");

   test_operation_data_signature_t operation_benchmarks =
       assert_operation_data_schema(
           "operation benchmark data",
           "docs/benchmarks/data/operations/benchmarks",
           BENCHMARK_SCHEMA_OPERATION_BENCHMARK_HEADER, true, false);
   assert_operation_data_registry_coverage("operation benchmark data",
                                           &operation_benchmarks);
   test_operation_data_signature_t operation_measurements =
       assert_operation_data_schema(
           "operation measurement data",
           "docs/benchmarks/data/operations/measurements",
           BENCHMARK_SCHEMA_OPERATION_MEASUREMENT_HEADER, false, true);
   assert_operation_data_registry_coverage("operation measurement data",
                                           &operation_measurements);
   assert_operation_data_signatures_equal(
       "operation benchmarks", &operation_benchmarks, "operation measurements",
       &operation_measurements);
   assert_operation_plot_groups_match_data("operation benchmark data",
                                           &operation_benchmarks);
   assert_operation_plot_groups_match_data("operation measurement data",
                                           &operation_measurements);
   test_operation_data_signature_dispose(&operation_benchmarks);
   test_operation_data_signature_dispose(&operation_measurements);
   printf("  operation measurement and benchmark data OK\n");
}

static void TestBenchmarkIndex(void) {
   printf("Benchmark index:\n");

   char output_dir[] = "/tmp/bst-benchmark-index-XXXXXX";
   char *root = mkdtemp(output_dir);
   invariant(root != NULL);

   char template_path[512];
   path_format(template_path, sizeof(template_path), "%s/template.html", root);
   test_write_text_file(template_path, "<title>{{ .directory }}</title>\n"
                                       "{{ range $_, $file := .files }}\n"
                                       "<figure>{{ inline $file }}</figure>\n"
                                       "{{ end }}\n");

   char root_svg[512];
   char root_hidden_svg[512];
   path_format(root_svg, sizeof(root_svg), "%s/root.svg", root);
   path_format(root_hidden_svg, sizeof(root_hidden_svg), "%s/.hidden-root.svg",
               root);
   test_write_text_file(root_svg, "<svg id=\"root\"></svg>\n");
   test_write_text_file(root_hidden_svg, "<svg id=\"hidden-root\"></svg>\n");

   char nested_dir[512];
   path_format(nested_dir, sizeof(nested_dir), "%s/group<>&\"'", root);
   mkdir_p(nested_dir);

   char a_svg[512];
   char b_svg[512];
   char hidden_svg[512];
   char text_file[512];
   char binary_text_file[512];
   char uppercase_svg[512];
   char symlink_target_svg[512];
   char symlink_svg[512];
   path_format(a_svg, sizeof(a_svg), "%s/a.svg", nested_dir);
   path_format(b_svg, sizeof(b_svg), "%s/b.svg", nested_dir);
   path_format(hidden_svg, sizeof(hidden_svg), "%s/.hidden.svg", nested_dir);
   path_format(text_file, sizeof(text_file), "%s/notes.txt", nested_dir);
   path_format(binary_text_file, sizeof(binary_text_file), "%s/binary.txt",
               nested_dir);
   path_format(uppercase_svg, sizeof(uppercase_svg), "%s/C.SVG", nested_dir);
   path_format(symlink_target_svg, sizeof(symlink_target_svg),
               "%s/.linked-target.svg", nested_dir);
   path_format(symlink_svg, sizeof(symlink_svg), "%s/linked.svg", nested_dir);
   test_write_text_file(b_svg, "<svg id=\"b\"></svg>\n");
   test_write_text_file(a_svg, "<svg id=\"a\"></svg>\n");
   test_write_text_file(hidden_svg, "<svg id=\"hidden\"></svg>\n");
   test_write_text_file(text_file, "<svg id=\"text-file\"></svg>\n");
   static const unsigned char binary_text[] = {
       '<', 's', 'v', 'g', ' ', 'i', 'd', '=', '"', 'b',
       'i', 'n', 'a', 'r', 'y', '-', 't', 'e', 'x', 't',
       '"', '>', 0,   '<', '/', 's', 'v', 'g', '>', '\n',
   };
   FILE *binary_file = fopen_or_exit(binary_text_file, "wb");
   file_write_or_exit(binary_text, sizeof(binary_text), binary_file,
                      binary_text_file);
   fclose_or_exit(binary_file, binary_text_file);
   test_write_text_file(uppercase_svg, "<svg id=\"uppercase\"></svg>\n");
   test_write_text_file(symlink_target_svg, "<svg id=\"linked\"></svg>\n");
   invariant(symlink(".linked-target.svg", symlink_svg) == 0);

   char extra_svgs[17][512];
   for (size_t i = 0; i < 17; i++) {
      path_format(extra_svgs[i], sizeof(extra_svgs[i]), "%s/extra-%02zu.svg",
                  nested_dir, i);
      char content[64];
      snprintf(content, sizeof(content), "<svg id=\"extra-%02zu\"></svg>\n", i);
      test_write_text_file(extra_svgs[i], content);
   }

   char hidden_dir[512];
   char hidden_dir_svg[512];
   char hidden_dir_index[512];
   path_format(hidden_dir, sizeof(hidden_dir), "%s/.hidden", root);
   path_format(hidden_dir_svg, sizeof(hidden_dir_svg), "%s/x.svg", hidden_dir);
   path_format(hidden_dir_index, sizeof(hidden_dir_index), "%s/index.html",
               hidden_dir);
   mkdir_p(hidden_dir);
   test_write_text_file(hidden_dir_svg, "<svg id=\"hidden-dir\"></svg>\n");

   char empty_dir[512];
   char empty_dir_index[512];
   path_format(empty_dir, sizeof(empty_dir), "%s/empty", root);
   path_format(empty_dir_index, sizeof(empty_dir_index), "%s/index.html",
               empty_dir);
   mkdir_p(empty_dir);
   test_write_text_file(empty_dir_index, "<p>stale</p>\n");

   char branch_dir[512];
   char branch_child_dir[512];
   char branch_index[512];
   char branch_child_svg[512];
   char branch_child_index[512];
   path_format(branch_dir, sizeof(branch_dir), "%s/branch", root);
   path_format(branch_child_dir, sizeof(branch_child_dir), "%s/child",
               branch_dir);
   path_format(branch_index, sizeof(branch_index), "%s/index.html", branch_dir);
   path_format(branch_child_svg, sizeof(branch_child_svg), "%s/child.svg",
               branch_child_dir);
   path_format(branch_child_index, sizeof(branch_child_index), "%s/index.html",
               branch_child_dir);
   mkdir_p(branch_child_dir);
   test_write_text_file(branch_index, "<p>stale branch</p>\n");
   test_write_text_file(branch_child_svg, "<svg id=\"branch-child\"></svg>\n");

   benchmark_index_run(template_path, root);

   char root_index[512];
   char nested_index[512];
   path_format(root_index, sizeof(root_index), "%s/index.html", root);
   path_format(nested_index, sizeof(nested_index), "%s/index.html", nested_dir);
   invariant(access(root_index, F_OK) == 0);
   invariant(access(nested_index, F_OK) == 0);

   char *root_html = test_read_text_file(root_index);
   test_require_substring(root_html, root);
   test_require_substring(root_html, "id=\"root\"");
   invariant(strstr(root_html, "id=\"hidden-root\"") == NULL);
   free(root_html);

   char *nested_html = test_read_text_file(nested_index);
   test_require_substring(nested_html, "group&lt;&gt;&amp;&quot;&#39;");
   const char *a = test_require_substring(nested_html, "id=\"a\"");
   const char *b = test_require_substring(nested_html, "id=\"b\"");
   test_require_substring(nested_html, "id=\"extra-16\"");
   test_require_substring(nested_html, "id=\"linked\"");
   invariant(a < b);
   invariant(strstr(nested_html, "id=\"hidden\"") == NULL);
   invariant(strstr(nested_html, "id=\"text-file\"") == NULL);
   invariant(strstr(nested_html, "id=\"binary-text\"") == NULL);
   invariant(strstr(nested_html, "id=\"uppercase\"") == NULL);
   assert_path_absent(hidden_dir_index);
   assert_path_absent(empty_dir_index);
   assert_path_absent(branch_index);
   free(nested_html);

   char *branch_child_html = test_read_text_file(branch_child_index);
   test_require_substring(branch_child_html, "id=\"branch-child\"");
   free(branch_child_html);

   invariant(remove(nested_index) == 0);
   invariant(remove(branch_child_index) == 0);
   invariant(remove(branch_child_svg) == 0);
   invariant(rmdir(branch_child_dir) == 0);
   invariant(rmdir(branch_dir) == 0);
   invariant(remove(a_svg) == 0);
   invariant(remove(b_svg) == 0);
   invariant(remove(hidden_svg) == 0);
   invariant(remove(text_file) == 0);
   invariant(remove(binary_text_file) == 0);
   invariant(remove(uppercase_svg) == 0);
   invariant(remove(symlink_svg) == 0);
   invariant(remove(symlink_target_svg) == 0);
   for (size_t i = 0; i < 17; i++) {
      invariant(remove(extra_svgs[i]) == 0);
   }
   invariant(rmdir(nested_dir) == 0);
   invariant(remove(hidden_dir_svg) == 0);
   invariant(rmdir(hidden_dir) == 0);
   invariant(rmdir(empty_dir) == 0);
   invariant(remove(root_index) == 0);
   invariant(remove(root_svg) == 0);
   invariant(remove(root_hidden_svg) == 0);
   invariant(remove(template_path) == 0);
   invariant(rmdir(root) == 0);
   printf("  generated root and nested indexes OK\n");

   char empty_output_dir[] = "/tmp/bst-benchmark-index-empty-XXXXXX";
   char *empty_root = mkdtemp(empty_output_dir);
   invariant(empty_root != NULL);
   char empty_template_path[512];
   char empty_index_path[512];
   path_format(empty_template_path, sizeof(empty_template_path),
               "%s/template.html", empty_root);
   path_format(empty_index_path, sizeof(empty_index_path), "%s/index.html",
               empty_root);
   test_write_text_file(empty_template_path, "<main>{{ .directory }}</main>\n");

   benchmark_index_run(empty_template_path, empty_root);
   char *empty_html = test_read_text_file(empty_index_path);
   test_require_substring(empty_html, empty_root);
   free(empty_html);
   invariant(remove(empty_index_path) == 0);
   invariant(remove(empty_template_path) == 0);
   invariant(rmdir(empty_root) == 0);
   printf("  empty root index generation OK\n");

   assert_benchmark_index_template_exits_2("{{ inline $file }}\n");
   assert_benchmark_index_template_exits_2("{{ range $_, $file := .files }}\n");
   assert_benchmark_index_template_exits_2("{{ range $_, $file := .files }}\n"
                                           "<div>{{ .directory }}</div>\n"
                                           "{{ end }}\n");
   assert_benchmark_index_template_exits_2("{{ range $_, $file := .files }}\n"
                                           "{{ inline $file }}\n"
                                           "{{ inline $file }}\n"
                                           "{{ end }}\n");
   assert_benchmark_index_template_exits_2("{{ range $_, $file := .files }}\n"
                                           "{{ range $_, $file := .files }}\n"
                                           "{{ end }}\n");
   assert_benchmark_index_template_exits_2("{{ range $_, $file := .files }}\n"
                                           "{{ inline $file }}\n"
                                           "{{ end }}\n"
                                           "{{ range $_, $file := .files }}\n"
                                           "{{ end }}\n");
   assert_benchmark_index_template_exits_2("{{ range $_, $file := .files }}\n"
                                           "{{ inline $file }}\n"
                                           "{{ end }}\n"
                                           "{{ end }}\n");
   assert_benchmark_index_template_exits_2("{{ range $_, $file := .files }}\n"
                                           "{{ inline $file }}\n"
                                           "{{ end }}\n"
                                           "{{ inline $file }}\n");
   static const unsigned char nul_template[] = {
       '<', 'h', '1', '>', 0, '<', '/', 'h', '1', '>', '\n',
   };
   assert_benchmark_index_template_bytes_exits_2(nul_template,
                                                 sizeof(nul_template));
   static const unsigned char nul_inline_svg[] = {
       '<', 's', 'v', 'g', '>', 0, '<', '/', 's', 'v', 'g', '>', '\n',
   };
   assert_benchmark_index_inline_bytes_exits_2(
       nul_inline_svg, sizeof(nul_inline_svg), "inline file contains NUL byte");
   assert_benchmark_index_preflights_nested_inline_failure();
   assert_benchmark_index_template_path_exits_2();
   printf("  invalid templates and inline files fail before publishing OK\n");

   assert_benchmark_index_run_exits_2(NULL, "root", "template path is null");
   assert_benchmark_index_run_exits_2("", "root", "template path is empty");
   assert_benchmark_index_run_exits_2("template", NULL, "root path is null");
   assert_benchmark_index_run_exits_2("template", "", "root path is empty");
   printf("  entrypoint argument guards OK\n");
}

typedef struct {
   const char *value;
   const char *name;
   u64 scale;
   const char *expected_stderr;
} cli_parse_case_t;

typedef void (*cli_parse_child_fn)(const cli_parse_case_t *tc);

static void cli_parse_u64_child(const cli_parse_case_t *tc) {
   invariant(tc != NULL);
   (void)cli_parse_u64(tc->value, tc->name);
}

static void cli_parse_positive_u64_child(const cli_parse_case_t *tc) {
   invariant(tc != NULL);
   (void)cli_parse_positive_u64(tc->value, tc->name);
}

static void cli_parse_u64_scaled_child(const cli_parse_case_t *tc) {
   invariant(tc != NULL);
   (void)cli_parse_u64_scaled(tc->value, tc->name, tc->scale);
}

static void cli_parse_positive_u64_scaled_child(const cli_parse_case_t *tc) {
   invariant(tc != NULL);
   (void)cli_parse_positive_u64_scaled(tc->value, tc->name, tc->scale);
}

static void cli_parse_arena_capacity_child(const cli_parse_case_t *tc) {
   invariant(tc != NULL);
   (void)cli_parse_arena_capacity(tc->value, tc->name);
}

static void
cli_parse_positive_arena_capacity_child(const cli_parse_case_t *tc) {
   invariant(tc != NULL);
   (void)cli_parse_positive_arena_capacity(tc->value, tc->name);
}

static void cli_parse_array_count_child(const cli_parse_case_t *tc) {
   invariant(tc != NULL);
   invariant(tc->scale <= (u64)SIZE_MAX);
   (void)cli_parse_array_count(tc->value, tc->name, (size_t)tc->scale);
}

static void cli_parse_counter_count_child(const cli_parse_case_t *tc) {
   invariant(tc != NULL);
   (void)cli_parse_counter_count(tc->value, tc->name);
}

static void cli_parse_positive_counter_count_child(const cli_parse_case_t *tc) {
   invariant(tc != NULL);
   (void)cli_parse_positive_counter_count(tc->value, tc->name);
}

static void cli_parse_step_count_child(const cli_parse_case_t *tc) {
   invariant(tc != NULL);
   (void)cli_parse_step_count(tc->value, tc->name);
}

static void cli_parse_positive_step_count_child(const cli_parse_case_t *tc) {
   invariant(tc != NULL);
   (void)cli_parse_positive_step_count(tc->value, tc->name);
}

static void cli_parse_sample_count_child(const cli_parse_case_t *tc) {
   invariant(tc != NULL);
   (void)cli_parse_sample_count(tc->value, tc->name);
}

static void cli_parse_positive_sample_count_child(const cli_parse_case_t *tc) {
   invariant(tc != NULL);
   (void)cli_parse_positive_sample_count(tc->value, tc->name);
}

static void cli_parse_iteration_count_child(const cli_parse_case_t *tc) {
   invariant(tc != NULL);
   (void)cli_parse_iteration_count(tc->value, tc->name);
}

static void
cli_parse_positive_iteration_count_child(const cli_parse_case_t *tc) {
   invariant(tc != NULL);
   (void)cli_parse_positive_iteration_count(tc->value, tc->name);
}

static void cli_parse_duration_ms_ns_child(const cli_parse_case_t *tc) {
   invariant(tc != NULL);
   (void)cli_parse_duration_ms_ns(tc->value, tc->name);
}

static void cli_parse_int_child(const cli_parse_case_t *tc) {
   invariant(tc != NULL);
   (void)cli_parse_int(tc->value, tc->name);
}

static void cli_parse_nonempty_string_child(const cli_parse_case_t *tc) {
   invariant(tc != NULL);
   (void)cli_parse_nonempty_string(tc->value, tc->name);
}

static void cli_arg_value_child(const cli_parse_case_t *tc) {
   invariant(tc != NULL);
   char *argv[] = {"test", (char *)tc->name, (char *)tc->value};
   int argc = tc->value == NULL ? 2 : 3;
   int index = 1;
   (void)cli_arg_value(argc, argv, &index, tc->name);
}

static void cli_unknown_arg_child(const cli_parse_case_t *tc) {
   invariant(tc != NULL);
   exit(cli_unknown_arg(tc->value));
}

static void assert_cli_parse_exits_2(cli_parse_child_fn fn,
                                     cli_parse_case_t tc) {
   invariant(fn != NULL);
   invariant(tc.name != NULL);

   char stderr_path[] = "/tmp/bst-cli-parse-stderr-XXXXXX";
   if (tc.expected_stderr != NULL) {
      int stderr_fd = mkstemp(stderr_path);
      invariant(stderr_fd >= 0);
      invariant(close(stderr_fd) == 0);
   }

   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *out = freopen("/dev/null", "w", stdout);
      invariant(out != NULL);
      FILE *err = tc.expected_stderr != NULL
                      ? freopen(stderr_path, "w", stderr)
                      : freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      fn(&tc);
      _exit(0);
   }

   assert_child_exited_2(pid);
   if (tc.expected_stderr != NULL) {
      char *stderr_text = test_read_text_file(stderr_path);
      test_require_substring(stderr_text, tc.expected_stderr);
      free(stderr_text);
      invariant(remove(stderr_path) == 0);
   }
}

static void
assert_test_program_exits_2_with_stderr(char *const argv[],
                                        const char *expected_stderr) {
   invariant(test_executable_path != NULL);
   invariant(argv != NULL);
   invariant(argv[0] == test_executable_path);
   invariant(expected_stderr != NULL);

   char stderr_path[] = "/tmp/bst-cli-main-stderr-XXXXXX";
   int stderr_fd = mkstemp(stderr_path);
   invariant(stderr_fd >= 0);
   invariant(close(stderr_fd) == 0);

   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *out = freopen("/dev/null", "w", stdout);
      invariant(out != NULL);
      FILE *err = freopen(stderr_path, "w", stderr);
      invariant(err != NULL);
      execv(test_executable_path, argv);
      perror("execv");
      _exit(127);
   }

   assert_child_exited_2(pid);
   char *stderr_text = test_read_text_file(stderr_path);
   test_require_substring(stderr_text, expected_stderr);
   free(stderr_text);
   invariant(remove(stderr_path) == 0);
}

static void assert_test_program_exits_0(char *const argv[]) {
   invariant(test_executable_path != NULL);
   invariant(argv != NULL);
   invariant(argv[0] == test_executable_path);

   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *out = freopen("/dev/null", "w", stdout);
      invariant(out != NULL);
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      execv(test_executable_path, argv);
      perror("execv");
      _exit(127);
   }

   int status = wait_child_or_abort(pid);
   if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      fprintf(stderr, "child did not exit 0: status=%d\n", status);
      abort();
   }
}

static void test_cli_parse_u64_scaled_zero_scale_child(void) {
   (void)cli_parse_u64_scaled("1", "-scaled", 0);
}

static void test_cli_parse_positive_u64_scaled_zero_scale_child(void) {
   (void)cli_parse_positive_u64_scaled("1", "-scaled", 0);
}

static void test_cli_parse_array_count_zero_elem_size_child(void) {
   (void)cli_parse_array_count("1", "-n", 0);
}

static void test_cli_parse_choice_line_null_line_child(void) {
   int choice = 0;
   (void)cli_parse_choice_line(NULL, 1, &choice);
}

static void test_cli_parse_choice_line_zero_count_child(void) {
   int choice = 0;
   (void)cli_parse_choice_line("1\n", 0, &choice);
}

static void test_cli_parse_choice_line_null_out_child(void) {
   (void)cli_parse_choice_line("1\n", 1, NULL);
}

static void assert_cli_parse_choice_line(const char *line, int count,
                                         int expected) {
   int choice = -1;
   invariant(cli_parse_choice_line(line, count, &choice));
   assert_int_equal(expected, choice);
}

static void assert_cli_rejects_choice_line(const char *line, int count) {
   int choice = 123;
   invariant(!cli_parse_choice_line(line, count, &choice));
   assert_int_equal(123, choice);
}

static void TestCliParsing(void) {
   printf("CLI parsing:\n");

   assert_equal(0, cli_parse_u64("0", "-zero"));
   assert_equal(7, cli_parse_u64("0007", "-leading-zero"));
   assert_equal(18446744073709551615ULL,
                cli_parse_u64("18446744073709551615", "-max"));
   assert_equal(1, cli_parse_positive_u64("1", "-positive"));
   assert_equal(123000, cli_parse_u64_scaled("123", "-duration-ms", 1000));
   assert_equal(1000, cli_parse_positive_u64_scaled("1", "-duration-ms", 1000));
   char arena_limit[32];
   int written = snprintf(arena_limit, sizeof(arena_limit), "%llu",
                          (unsigned long long)arena_max_capacity());
   invariant(written > 0);
   invariant((size_t)written < sizeof(arena_limit));
   assert_equal(arena_max_capacity(),
                cli_parse_arena_capacity(arena_limit, "-arena"));
   assert_equal(1, cli_parse_positive_arena_capacity("1", "-arena"));
   u64 max_array_count = bst_array_max_count(sizeof(u64));
   char array_limit[32];
   written = snprintf(array_limit, sizeof(array_limit), "%llu",
                      (unsigned long long)max_array_count);
   invariant(written > 0);
   invariant((size_t)written < sizeof(array_limit));
   assert_equal(max_array_count,
                cli_parse_array_count(array_limit, "-n", sizeof(u64)));
   assert_equal(UINT64_MAX - 1,
                cli_parse_counter_count("18446744073709551614", "-counter"));
   assert_equal(1, cli_parse_positive_counter_count("1", "-counter"));
   assert_equal(UINT64_MAX - 1,
                cli_parse_step_count("18446744073709551614", "-steps"));
   assert_equal(1, cli_parse_positive_step_count("1", "-steps"));
   assert_equal(UINT64_MAX - 1,
                cli_parse_sample_count("18446744073709551614", "-samples"));
   assert_equal(UINT64_MAX - 1, cli_parse_positive_sample_count(
                                    "18446744073709551614", "-samples"));
   assert_equal(UINT64_MAX - 1, cli_parse_iteration_count(
                                    "18446744073709551614", "-iterations"));
   assert_equal(1, cli_parse_positive_iteration_count("1", "-iterations"));
   assert_equal(123000000, cli_parse_duration_ms_ns("123", "-duration-ms"));
   assert_equal(18446744073709000000ULL,
                cli_parse_duration_ms_ns("18446744073709", "-duration-ms"));
   assert_int_equal(INT_MAX, cli_parse_int("2147483647", "-int"));
   assert_int_equal(INT_MIN, cli_parse_int("-2147483648", "-int"));
   assert_int_equal(-7, cli_parse_int("-7", "-int"));
   assert_int_equal(0, cli_parse_int("-0", "-int"));
   invariant(strcmp("Splay", cli_parse_nonempty_string("Splay", "-tree")) == 0);
   assert_cli_parse_choice_line("1\n", 3, 0);
   assert_cli_parse_choice_line("02 \t\r\n", 3, 1);
   assert_cli_parse_choice_line("3", 3, 2);
   assert_cli_parse_choice_line("2147483647", INT_MAX, INT_MAX - 1);

   printf("  valid argument boundaries OK\n");

   assert_cli_parse_exits_2(
       cli_parse_u64_child,
       (cli_parse_case_t){.value = NULL,
                          .name = "-null",
                          .expected_stderr = "invalid -null: (null)"});
   assert_cli_parse_exits_2(cli_parse_u64_child,
                            (cli_parse_case_t){.value = "", .name = "-empty"});
   assert_cli_parse_exits_2(
       cli_parse_u64_child,
       (cli_parse_case_t){.value = "12x",
                          .name = "-malformed",
                          .expected_stderr = "invalid -malformed: 12x"});
   assert_cli_parse_exits_2(
       cli_parse_u64_child,
       (cli_parse_case_t){.value = "12\nx",
                          .name = "-malformed",
                          .expected_stderr = "invalid -malformed: 12\\nx"});
   assert_cli_parse_exits_2(
       cli_parse_u64_child,
       (cli_parse_case_t){.value = "12\\nx",
                          .name = "-malformed",
                          .expected_stderr = "invalid -malformed: 12\\\\nx"});
   assert_cli_parse_exits_2(
       cli_parse_u64_child,
       (cli_parse_case_t){.value = "12\200x",
                          .name = "-malformed",
                          .expected_stderr = "invalid -malformed: 12\\x80x"});
   assert_cli_parse_exits_2(cli_parse_u64_child,
                            (cli_parse_case_t){.value = "+1", .name = "-plus"});
   assert_cli_parse_exits_2(
       cli_parse_u64_child,
       (cli_parse_case_t){.value = "-1", .name = "-minus"});
   assert_cli_parse_exits_2(
       cli_parse_u64_child,
       (cli_parse_case_t){.value = " 1", .name = "-space"});
   assert_cli_parse_exits_2(
       cli_parse_u64_child,
       (cli_parse_case_t){.value = "18446744073709551616",
                          .name = "-overflow",
                          .expected_stderr =
                              "-overflow is too large: 18446744073709551616"});
   assert_cli_parse_exits_2(
       cli_parse_u64_child,
       (cli_parse_case_t){
           .value = "18446744073709551616\n",
           .name = "-overflow",
           .expected_stderr =
               "-overflow is too large: 18446744073709551616\\n"});
   assert_cli_parse_exits_2(
       cli_parse_positive_u64_child,
       (cli_parse_case_t){.value = "0", .name = "-positive"});
   assert_cli_parse_exits_2(cli_parse_u64_scaled_child,
                            (cli_parse_case_t){.value = "2",
                                               .name = "-scaled",
                                               .scale = UINT64_MAX});
   assert_cli_parse_exits_2(
       cli_parse_positive_u64_scaled_child,
       (cli_parse_case_t){.value = "0", .name = "-duration-ms", .scale = 1000});
   assert_cli_parse_exits_2(
       cli_parse_arena_capacity_child,
       (cli_parse_case_t){.value = "18446744073709551615", .name = "-arena"});
   assert_cli_parse_exits_2(cli_parse_positive_arena_capacity_child,
                            (cli_parse_case_t){.value = "0", .name = "-arena"});
   if (max_array_count < UINT64_MAX) {
      char array_overflow[32];
      written = snprintf(array_overflow, sizeof(array_overflow), "%llu",
                         (unsigned long long)(max_array_count + 1));
      invariant(written > 0);
      invariant((size_t)written < sizeof(array_overflow));
      assert_cli_parse_exits_2(cli_parse_array_count_child,
                               (cli_parse_case_t){.value = array_overflow,
                                                  .name = "-n",
                                                  .scale = sizeof(u64)});
   }
   assert_cli_parse_exits_2(
       cli_parse_counter_count_child,
       (cli_parse_case_t){.value = "18446744073709551615", .name = "-counter"});
   assert_cli_parse_exits_2(
       cli_parse_positive_counter_count_child,
       (cli_parse_case_t){.value = "18446744073709551615", .name = "-counter"});
   assert_cli_parse_exits_2(
       cli_parse_positive_counter_count_child,
       (cli_parse_case_t){.value = "0", .name = "-counter"});
   assert_cli_parse_exits_2(
       cli_parse_step_count_child,
       (cli_parse_case_t){.value = "18446744073709551615", .name = "-steps"});
   assert_cli_parse_exits_2(
       cli_parse_positive_step_count_child,
       (cli_parse_case_t){.value = "18446744073709551615", .name = "-steps"});
   assert_cli_parse_exits_2(cli_parse_positive_step_count_child,
                            (cli_parse_case_t){.value = "0", .name = "-steps"});
   assert_cli_parse_exits_2(
       cli_parse_sample_count_child,
       (cli_parse_case_t){.value = "18446744073709551615", .name = "-samples"});
   assert_cli_parse_exits_2(
       cli_parse_positive_sample_count_child,
       (cli_parse_case_t){.value = "18446744073709551615", .name = "-samples"});
   assert_cli_parse_exits_2(
       cli_parse_positive_sample_count_child,
       (cli_parse_case_t){.value = "0", .name = "-samples"});
   assert_cli_parse_exits_2(cli_parse_iteration_count_child,
                            (cli_parse_case_t){.value = "18446744073709551615",
                                               .name = "-iterations"});
   assert_cli_parse_exits_2(cli_parse_positive_iteration_count_child,
                            (cli_parse_case_t){.value = "18446744073709551615",
                                               .name = "-iterations"});
   assert_cli_parse_exits_2(
       cli_parse_positive_iteration_count_child,
       (cli_parse_case_t){.value = "0", .name = "-iterations"});
   assert_cli_parse_exits_2(
       cli_parse_duration_ms_ns_child,
       (cli_parse_case_t){.value = "0", .name = "-duration-ms"});
   assert_cli_parse_exits_2(
       cli_parse_duration_ms_ns_child,
       (cli_parse_case_t){.value = "18446744073710",
                          .name = "-duration-ms",
                          .expected_stderr =
                              "-duration-ms is too large: 18446744073710"});
   assert_cli_parse_exits_2(
       cli_parse_int_child,
       (cli_parse_case_t){.value = "2147483648", .name = "-int"});
   assert_cli_parse_exits_2(
       cli_parse_int_child,
       (cli_parse_case_t){.value = "-2147483649", .name = "-int"});
   assert_cli_parse_exits_2(cli_parse_int_child,
                            (cli_parse_case_t){.value = "-", .name = "-int"});
   assert_cli_parse_exits_2(cli_parse_int_child,
                            (cli_parse_case_t){.value = "+1", .name = "-int"});
   assert_cli_parse_exits_2(cli_parse_nonempty_string_child,
                            (cli_parse_case_t){.value = NULL, .name = "-tree"});
   assert_cli_parse_exits_2(cli_parse_nonempty_string_child,
                            (cli_parse_case_t){.value = "", .name = "-tree"});
   assert_cli_parse_exits_2(
       cli_arg_value_child,
       (cli_parse_case_t){.value = NULL,
                          .name = "-scale",
                          .expected_stderr = "missing value for -scale"});
   assert_cli_parse_exits_2(
       cli_unknown_arg_child,
       (cli_parse_case_t){.value = "-bad\narg",
                          .name = "-arg",
                          .expected_stderr =
                              "unknown or incomplete argument: -bad\\narg"});
   assert_cli_parse_exits_2(
       cli_unknown_arg_child,
       (cli_parse_case_t){.value = "-bad\\narg",
                          .name = "-arg",
                          .expected_stderr =
                              "unknown or incomplete argument: -bad\\\\narg"});
   assert_cli_parse_exits_2(
       cli_unknown_arg_child,
       (cli_parse_case_t){.value = "-bad\200arg",
                          .name = "-arg",
                          .expected_stderr =
                              "unknown or incomplete argument: -bad\\x80arg"});
   char *high_byte_tree_args[] = {(char *)test_executable_path,
                                  "-tree",
                                  "Bad\200Name",
                                  "-test",
                                  "TestInsert",
                                  NULL};
   assert_test_program_exits_2_with_stderr(high_byte_tree_args,
                                           "tree name is not a path component");
   char *high_byte_dist_args[] = {(char *)test_executable_path,
                                  "-distribution",
                                  "Bad\200Name",
                                  "-test",
                                  "TestInsert",
                                  NULL};
   assert_test_program_exits_2_with_stderr(
       high_byte_dist_args, "distribution name is not a printable token");
   char *high_byte_balancer_args[] = {(char *)test_executable_path,
                                      "-balancer",
                                      "Bad\200Name",
                                      "-test",
                                      "TestBalancers",
                                      NULL};
   assert_test_program_exits_2_with_stderr(
       high_byte_balancer_args, "balancer name is not a path component");
   char *high_byte_test_args[] = {(char *)test_executable_path, "-test",
                                  "Bad\200Name", NULL};
   assert_test_program_exits_2_with_stderr(
       high_byte_test_args, "test name is not a printable token");
   char *unknown_tree_args[] = {(char *)test_executable_path,
                                "-tree",
                                "__missing_tree__",
                                "-test",
                                "TestInsert",
                                NULL};
   assert_test_program_exits_2_with_stderr(unknown_tree_args,
                                           "unknown tree: __missing_tree__");
   char *unknown_dist_args[] = {(char *)test_executable_path,
                                "-distribution",
                                "__missing_distribution__",
                                "-test",
                                "TestInsert",
                                NULL};
   assert_test_program_exits_2_with_stderr(
       unknown_dist_args, "unknown distribution: __missing_distribution__");
   char *unknown_balancer_args[] = {(char *)test_executable_path,
                                    "-balancer",
                                    "__missing_balancer__",
                                    "-test",
                                    "TestBalancers",
                                    NULL};
   assert_test_program_exits_2_with_stderr(
       unknown_balancer_args, "unknown balancer: __missing_balancer__");
   char *unknown_test_args[] = {(char *)test_executable_path, "-test",
                                "__missing_test__", NULL};
   assert_test_program_exits_2_with_stderr(unknown_test_args,
                                           "unknown test: __missing_test__");
   invariant(balancer_registry_count > 0);
   char *no_match_args[] = {(char *)test_executable_path, "-balancer",
                            (char *)balancer_registry_all[0]->name,
                            "-skip-balancers", NULL};
   assert_test_program_exits_2_with_stderr(no_match_args, "no tests matched");
   invariant(tree_registry_count > 0);
   invariant(dist_count > 0);
   char *focused_tree_args[] = {(char *)test_executable_path,
                                "-tree",
                                (char *)tree_registry_all[0]->name,
                                "-distribution",
                                (char *)dist_all[0]->name,
                                "-test",
                                "TestInsert",
                                "-scale",
                                "0",
                                "-skip-balancers",
                                NULL};
   assert_test_program_exits_0(focused_tree_args);
   char *focused_tree_alias_args[] = {(char *)test_executable_path,
                                      "-tree",
                                      (char *)tree_registry_all[0]->name,
                                      "-dist",
                                      (char *)dist_all[0]->name,
                                      "-test",
                                      "TestInsert",
                                      "-scale",
                                      "0",
                                      "-skip-balancers",
                                      NULL};
   assert_test_program_exits_0(focused_tree_alias_args);
   char *focused_standalone_args[] = {(char *)test_executable_path, "-test",
                                      "TestCheckedAllocation",
                                      "-skip-balancers", NULL};
   assert_test_program_exits_0(focused_standalone_args);
   assert_cli_rejects_choice_line("", 3);
   assert_cli_rejects_choice_line("0\n", 3);
   assert_cli_rejects_choice_line("4\n", 3);
   assert_cli_rejects_choice_line("+1\n", 3);
   assert_cli_rejects_choice_line("-1\n", 3);
   assert_cli_rejects_choice_line(" 1\n", 3);
   assert_cli_rejects_choice_line("1x\n", 3);
   assert_cli_rejects_choice_line("1\n2", 3);
   assert_cli_rejects_choice_line("1\200\n", 3);
   assert_cli_rejects_choice_line("2147483648", INT_MAX);
   assert_cli_rejects_choice_line("999999999999999999999999999999", INT_MAX);

   printf("  argument routing exits OK\n");

   assert_aborts(test_cli_parse_u64_scaled_zero_scale_child);
   assert_aborts(test_cli_parse_positive_u64_scaled_zero_scale_child);
   assert_aborts(test_cli_parse_array_count_zero_elem_size_child);
   assert_aborts(test_cli_parse_choice_line_null_line_child);
   assert_aborts(test_cli_parse_choice_line_zero_count_child);
   assert_aborts(test_cli_parse_choice_line_null_out_child);

   printf("  parser invariant guards OK\n");
}

static void list_contract_visit_noop(u64 value, void *ctx) {
   (void)value;
   (void)ctx;
}

static const list_vtable_t *list_contract_direct_vtable = NULL;

typedef struct {
   tree_t tree;
   weight_balance_t wb;
} tree_list_wb_contract_t;

WB_VT_BODY(
    tree_list_wb_contract_t, tree_list_wb_contract, wb_three_two,
    {
       tree_list_wb_contract_t *t = (tree_list_wb_contract_t *)self->impl;
       t->tree.root = tree_allocate((node_t){.s = 1});
       t->tree.size = size_after_inserting_one(t->tree.size);
    },
    { bst_unreachable(); }, { bst_unreachable(); }, { bst_unreachable(); },
    {
       tree_list_wb_contract_t *t = (tree_list_wb_contract_t *)self->impl;
       tree_list_verify_tree(t->tree);
    })

static const list_vtable_t tree_list_wb_contract_vtable =
    TREE_LIST_VTABLE("WBContract", tree_list_wb_contract);

static void test_list_direct_vtable_size_null_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   (void)list_contract_direct_vtable->size_fn(NULL);
}

static void test_list_direct_vtable_new_null_vtable_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   (void)list_contract_direct_vtable->new_fn(NULL);
}

static void test_list_direct_vtable_new_bad_vtable_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_vtable_t vt = *list_contract_direct_vtable;
   vt.size_fn = NULL;
   (void)list_contract_direct_vtable->new_fn(&vt);
}

static void test_list_direct_vtable_clone_null_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   (void)list_contract_direct_vtable->clone_fn(NULL);
}

static void test_list_direct_vtable_free_null_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_contract_direct_vtable->free_fn(NULL);
}

static void test_list_direct_vtable_tree_null_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   (void)list_contract_direct_vtable->tree_fn(NULL);
}

static void test_list_direct_vtable_verify_null_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_contract_direct_vtable->verify_fn(NULL);
}

static void test_list_direct_vtable_each_null_self_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_contract_direct_vtable->each_fn(NULL, list_contract_visit_noop, NULL);
}

static void test_list_direct_vtable_each_null_visit_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_t instance =
       list_contract_direct_vtable->new_fn(list_contract_direct_vtable);
   list_contract_direct_vtable->each_fn(&instance, NULL, NULL);
}

static void test_list_direct_vtable_select_null_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   (void)list_contract_direct_vtable->select_fn(NULL, 0);
}

static void test_list_direct_vtable_select_empty_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_t instance =
       list_contract_direct_vtable->new_fn(list_contract_direct_vtable);
   (void)list_contract_direct_vtable->select_fn(&instance, 0);
}

static void test_list_direct_vtable_update_null_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_contract_direct_vtable->update_fn(NULL, 0, 0);
}

static void test_list_direct_vtable_update_empty_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_t instance =
       list_contract_direct_vtable->new_fn(list_contract_direct_vtable);
   list_contract_direct_vtable->update_fn(&instance, 0, 0);
}

static void test_list_direct_vtable_insert_null_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_contract_direct_vtable->insert_fn(NULL, 0, 0);
}

static void test_list_direct_vtable_insert_after_end_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_t instance =
       list_contract_direct_vtable->new_fn(list_contract_direct_vtable);
   list_contract_direct_vtable->insert_fn(&instance, 1, 0);
}

static void test_list_direct_vtable_delete_null_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   (void)list_contract_direct_vtable->delete_fn(NULL, 0);
}

static void test_list_direct_vtable_delete_empty_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_t instance =
       list_contract_direct_vtable->new_fn(list_contract_direct_vtable);
   (void)list_contract_direct_vtable->delete_fn(&instance, 0);
}

static void test_list_direct_vtable_split_null_self_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_t left;
   list_t right;
   list_contract_direct_vtable->split_fn(NULL, 0, &left, &right);
}

static void test_list_direct_vtable_split_null_left_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_t instance =
       list_contract_direct_vtable->new_fn(list_contract_direct_vtable);
   list_t right;
   list_contract_direct_vtable->split_fn(&instance, 0, NULL, &right);
}

static void test_list_direct_vtable_split_null_right_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_t instance =
       list_contract_direct_vtable->new_fn(list_contract_direct_vtable);
   list_t left;
   list_contract_direct_vtable->split_fn(&instance, 0, &left, NULL);
}

static void test_list_direct_vtable_split_left_is_self_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_t instance =
       list_contract_direct_vtable->new_fn(list_contract_direct_vtable);
   list_t right;
   list_contract_direct_vtable->split_fn(&instance, 0, &instance, &right);
}

static void test_list_direct_vtable_split_right_is_self_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_t instance =
       list_contract_direct_vtable->new_fn(list_contract_direct_vtable);
   list_t left;
   list_contract_direct_vtable->split_fn(&instance, 0, &left, &instance);
}

static void test_list_direct_vtable_split_same_output_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_t instance =
       list_contract_direct_vtable->new_fn(list_contract_direct_vtable);
   list_t output;
   list_contract_direct_vtable->split_fn(&instance, 0, &output, &output);
}

static void test_list_direct_vtable_split_after_end_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_t instance =
       list_contract_direct_vtable->new_fn(list_contract_direct_vtable);
   list_t left;
   list_t right;
   list_contract_direct_vtable->split_fn(&instance, 1, &left, &right);
}

static void test_list_direct_vtable_join_null_self_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   (void)list_contract_direct_vtable->join_fn(NULL, NULL);
}

static void test_list_direct_vtable_join_null_other_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_t instance =
       list_contract_direct_vtable->new_fn(list_contract_direct_vtable);
   (void)list_contract_direct_vtable->join_fn(&instance, NULL);
}

static void test_list_direct_vtable_join_same_self_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_t instance =
       list_contract_direct_vtable->new_fn(list_contract_direct_vtable);
   (void)list_contract_direct_vtable->join_fn(&instance, &instance);
}

static void test_list_direct_vtable_join_aliasing_impl_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_t instance =
       list_contract_direct_vtable->new_fn(list_contract_direct_vtable);
   list_t alias = instance;
   (void)list_contract_direct_vtable->join_fn(&instance, &alias);
}

static void
assert_list_direct_vtable_contract_guards_abort(const list_vtable_t *vt) {
   list_vtable_require(vt);
   list_contract_direct_vtable = vt;
   assert_aborts(test_list_direct_vtable_new_null_vtable_child);
   assert_aborts(test_list_direct_vtable_new_bad_vtable_child);
   assert_aborts(test_list_direct_vtable_clone_null_child);
   assert_aborts(test_list_direct_vtable_free_null_child);
   assert_aborts(test_list_direct_vtable_tree_null_child);
   assert_aborts(test_list_direct_vtable_size_null_child);
   assert_aborts(test_list_direct_vtable_verify_null_child);
   assert_aborts(test_list_direct_vtable_each_null_self_child);
   assert_aborts(test_list_direct_vtable_each_null_visit_child);
   assert_aborts(test_list_direct_vtable_select_null_child);
   assert_aborts(test_list_direct_vtable_select_empty_child);
   assert_aborts(test_list_direct_vtable_update_null_child);
   assert_aborts(test_list_direct_vtable_update_empty_child);
   assert_aborts(test_list_direct_vtable_insert_null_child);
   assert_aborts(test_list_direct_vtable_insert_after_end_child);
   assert_aborts(test_list_direct_vtable_delete_null_child);
   assert_aborts(test_list_direct_vtable_delete_empty_child);
   assert_aborts(test_list_direct_vtable_split_null_self_child);
   assert_aborts(test_list_direct_vtable_split_null_left_child);
   assert_aborts(test_list_direct_vtable_split_null_right_child);
   assert_aborts(test_list_direct_vtable_split_left_is_self_child);
   assert_aborts(test_list_direct_vtable_split_right_is_self_child);
   assert_aborts(test_list_direct_vtable_split_same_output_child);
   assert_aborts(test_list_direct_vtable_split_after_end_child);
   assert_aborts(test_list_direct_vtable_join_null_self_child);
   assert_aborts(test_list_direct_vtable_join_null_other_child);
   assert_aborts(test_list_direct_vtable_join_same_self_child);
   assert_aborts(test_list_direct_vtable_join_aliasing_impl_child);
   list_contract_direct_vtable = NULL;
}

static void assert_tree_registry_direct_vtable_contract_guards_abort(void) {
   assert_list_direct_vtable_contract_guards_abort(&reference_vtable);
   for (int i = 0; i < tree_registry_count; i++) {
      assert_list_direct_vtable_contract_guards_abort(tree_registry_all[i]);
   }
}

#ifdef DEBUG
static void test_tree_direct_vtable_split_wrong_reachable_size_child(void) {
   list_vtable_require(list_contract_direct_vtable);
   list_t instance =
       list_contract_direct_vtable->new_fn(list_contract_direct_vtable);
   tree_t *tree = list_contract_direct_vtable->tree_fn(&instance);
   invariant(tree != NULL);

   node_t child = {0};
   node_t root = {
       .r = &child,
       .s = 0,
   };
   *tree = (tree_t){
       .root = &root,
       .size = 3,
   };

   list_t left;
   list_t right;
   list_contract_direct_vtable->split_fn(&instance, 1, &left, &right);
}

static void assert_tree_registry_direct_vtable_split_wrong_size_aborts(void) {
   for (int i = 0; i < tree_registry_count; i++) {
      list_contract_direct_vtable = tree_registry_all[i];
      assert_aborts(test_tree_direct_vtable_split_wrong_reachable_size_child);
   }
   list_contract_direct_vtable = NULL;
}

static void assert_list_debug_verifier_on_dispatch(void) {
   list_t instance = list_new(&tracking_vtable);
   tracking_list_t *tracked = tracking_list_impl(&instance);
   invariant(tracked->verify_count > 0);

   u64 before_clone = tracked->verify_count;
   list_t clone = list_clone(&instance);
   invariant(tracking_list_impl(&instance)->verify_count > before_clone);
   invariant(tracking_list_impl(&clone)->verify_count > before_clone);

   u64 before_each = tracking_list_impl(&clone)->verify_count;
   list_each(&clone, list_contract_visit_noop, NULL);
   invariant(tracking_list_impl(&clone)->verify_count > before_each);

   list_insert(&clone, 0, 0);
   u64 before_select = tracking_list_impl(&clone)->verify_count;
   assert_equal(0, list_select(&clone, 0));
   invariant(tracking_list_impl(&clone)->verify_count > before_select);

   tracking_list_t *clone_tracked = tracking_list_impl(&clone);
   clone_tracked->free_min_verify_count = clone_tracked->verify_count + 1;
   list_free(&clone);

   tracking_list_t *instance_tracked = tracking_list_impl(&instance);
   instance_tracked->free_min_verify_count = instance_tracked->verify_count + 1;
   list_free(&instance);
}

static void assert_list_each_verifies_before_each(void) {
   list_t instance = list_new(&tracking_vtable);
   tracking_list_t *tracked = tracking_list_impl(&instance);
   tracked->verify_count = 0;
   tracked->require_verify_before_each = true;

   list_each(&instance, list_contract_visit_noop, NULL);
   invariant(tracked->verify_count > 0);

   list_free(&instance);
}

static void assert_list_debug_capture_uses_raw_each(void) {
   list_t instance = list_new(&tracking_vtable);
   tracking_list_t *tracked = tracking_list_impl(&instance);
   tracked->size = 3;
   tracked->each_count = 0;

   u64 *values = list_debug_capture_sequence(&instance, tracked->size);
   assert_equal(1, tracked->each_count);
   for (u64 i = 0; i < tracked->size; i++) {
      assert_equal(0, values[i]);
   }

   free(values);
   list_free(&instance);
}
#endif

static list_t tree_list_common_bad_rank_root_instance(node_t *root) {
   list_vtable_require(list_contract_direct_vtable);
   invariant(root != NULL);

   list_t instance =
       list_contract_direct_vtable->new_fn(list_contract_direct_vtable);
   tree_t *tree = list_contract_direct_vtable->tree_fn(&instance);
   invariant(tree != NULL);

   *root = (node_t){.s = 1};
   *tree = (tree_t){.root = root, .size = 1};
   return instance;
}

static void test_tree_list_common_tree_bad_root_rank_child(void) {
   node_t root;
   list_t instance = tree_list_common_bad_rank_root_instance(&root);
   (void)list_contract_direct_vtable->tree_fn(&instance);
}

static void test_tree_list_common_size_bad_root_rank_child(void) {
   node_t root;
   list_t instance = tree_list_common_bad_rank_root_instance(&root);
   (void)list_contract_direct_vtable->size_fn(&instance);
}

static void test_tree_list_common_each_bad_root_rank_child(void) {
   node_t root;
   list_t instance = tree_list_common_bad_rank_root_instance(&root);
   list_contract_direct_vtable->each_fn(&instance, list_contract_visit_noop,
                                        NULL);
}

static void test_tree_list_common_free_bad_root_rank_child(void) {
   node_t root;
   list_t instance = tree_list_common_bad_rank_root_instance(&root);
   list_contract_direct_vtable->free_fn(&instance);
}

static void assert_tree_list_common_sized_handle_guards_abort(void) {
   list_contract_direct_vtable = &randomized_vtable;
   assert_aborts(test_tree_list_common_tree_bad_root_rank_child);
   assert_aborts(test_tree_list_common_free_bad_root_rank_child);
   assert_aborts(test_tree_list_common_size_bad_root_rank_child);
   assert_aborts(test_tree_list_common_each_bad_root_rank_child);
   list_contract_direct_vtable = NULL;
}

static void assert_splay_sized_handle_guards_abort(void) {
   list_contract_direct_vtable = &splay_vtable;
   assert_aborts(test_tree_list_common_tree_bad_root_rank_child);
   assert_aborts(test_tree_list_common_free_bad_root_rank_child);
   assert_aborts(test_tree_list_common_size_bad_root_rank_child);
   assert_aborts(test_tree_list_common_each_bad_root_rank_child);
   list_contract_direct_vtable = NULL;
}

static void assert_treap_fingertree_sized_handle_guards_abort(void) {
   list_contract_direct_vtable = &treap_fingertree_vtable;
   assert_aborts(test_tree_list_common_tree_bad_root_rank_child);
   assert_aborts(test_tree_list_common_free_bad_root_rank_child);
   assert_aborts(test_tree_list_common_size_bad_root_rank_child);
   assert_aborts(test_tree_list_common_each_bad_root_rank_child);
   list_contract_direct_vtable = NULL;
}

static void test_tree_list_wb_insert_bad_post_rank_child(void) {
   list_t instance =
       tree_list_wb_contract_vtable.new_fn(&tree_list_wb_contract_vtable);
   tree_list_wb_contract_vtable.insert_fn(&instance, 0, 0);
}

static void assert_list_verify_verifies_before_each(void) {
   list_t instance = list_new(&tracking_vtable);
   tracking_list_t *tracked = tracking_list_impl(&instance);
   tracked->size = 3;
   tracked->verify_count = 0;
   tracked->require_verify_before_each = true;

   list_verify(&instance);
   assert_equal(1, tracked->verify_count);

   list_free(&instance);
}

static void test_list_verify_mutating_size_child(void) {
   list_t instance = list_new(&tracking_vtable);
   tracking_list_impl(&instance)->mutate_size_on_verify = true;
   list_verify(&instance);
}

static void test_list_vtable_null_child(void) {
   list_vtable_require(NULL);
}

static void test_list_vtable_empty_name_child(void) {
   list_vtable_t vt = tracking_vtable;
   vt.name = "";
   list_vtable_require(&vt);
}

static void test_list_vtable_dot_name_child(void) {
   list_vtable_t vt = tracking_vtable;
   vt.name = ".";
   list_vtable_require(&vt);
}

static void test_list_vtable_dotdot_name_child(void) {
   list_vtable_t vt = tracking_vtable;
   vt.name = "..";
   list_vtable_require(&vt);
}

static void test_list_vtable_slash_name_child(void) {
   list_vtable_t vt = tracking_vtable;
   vt.name = "Bad/Name";
   list_vtable_require(&vt);
}

static void test_list_vtable_space_name_child(void) {
   list_vtable_t vt = tracking_vtable;
   vt.name = "Bad Name";
   list_vtable_require(&vt);
}

static void test_list_vtable_high_byte_name_child(void) {
   list_vtable_t vt = tracking_vtable;
   vt.name = "Bad\200Name";
   list_vtable_require(&vt);
}

static void test_list_vtable_missing_size_child(void) {
   list_vtable_t vt = tracking_vtable;
   vt.size_fn = NULL;
   list_vtable_require(&vt);
}

static void test_list_new_null_vtable_child(void) {
   (void)list_new(NULL);
}

static void test_list_size_null_child(void) {
   (void)list_size(NULL);
}

static void test_list_size_uninitialized_child(void) {
   list_t instance = {0};
   (void)list_size(&instance);
}

static void test_list_free_uninitialized_with_impl_child(void) {
   list_t instance = {0};
   instance.impl = &instance;
   list_free(&instance);
}

static void test_list_each_null_visit_child(void) {
   list_t instance = list_new(&tracking_vtable);
   list_each(&instance, NULL, NULL);
}

static void test_list_select_empty_child(void) {
   list_t instance = list_new(&tracking_vtable);
   (void)list_select(&instance, 0);
}

static void test_list_update_empty_child(void) {
   list_t instance = list_new(&tracking_vtable);
   list_update(&instance, 0, 0);
}

static void test_list_insert_after_end_child(void) {
   list_t instance = list_new(&tracking_vtable);
   list_insert(&instance, 1, 0);
}

static void test_list_delete_empty_child(void) {
   list_t instance = list_new(&tracking_vtable);
   (void)list_delete(&instance, 0);
}

static void test_list_split_null_left_child(void) {
   list_t instance = list_new(&tracking_vtable);
   list_t right;
   list_split(&instance, 0, NULL, &right);
}

static void test_list_split_null_right_child(void) {
   list_t instance = list_new(&tracking_vtable);
   list_t left;
   list_split(&instance, 0, &left, NULL);
}

static void test_list_split_left_is_self_child(void) {
   list_t instance = list_new(&tracking_vtable);
   list_t right;
   list_split(&instance, 0, &instance, &right);
}

static void test_list_split_right_is_self_child(void) {
   list_t instance = list_new(&tracking_vtable);
   list_t left;
   list_split(&instance, 0, &left, &instance);
}

static void test_list_split_same_output_child(void) {
   list_t instance = list_new(&tracking_vtable);
   list_t output;
   list_split(&instance, 0, &output, &output);
}

static void test_list_split_after_end_child(void) {
   list_t instance = list_new(&tracking_vtable);
   list_t left;
   list_t right;
   list_split(&instance, 1, &left, &right);
}

static void test_list_join_null_left_child(void) {
   list_t right = list_new(&tracking_vtable);
   (void)list_join(NULL, &right);
}

static void test_list_join_null_right_child(void) {
   list_t left = list_new(&tracking_vtable);
   (void)list_join(&left, NULL);
}

static void test_list_join_mismatched_vtable_child(void) {
   list_t left = list_new(&tracking_vtable);
   list_t right = list_new(&reference_vtable);
   (void)list_join(&left, &right);
}

static void test_list_join_aliasing_handle_child(void) {
   list_t left = list_new(&tracking_vtable);
   list_t alias = left;
   (void)list_join(&left, &alias);
}

static void test_list_tree_null_root_nonzero_size_child(void) {
   list_t instance = list_new(&tracking_vtable);
   tracking_list_t *tracked = tracking_list_impl(&instance);
   tracked->size = 1;
   tracked->expose_tree = true;
   tracked->tree = (tree_t){.root = NULL, .size = 1};
   (void)list_tree(&instance);
}

static void test_list_tree_root_zero_size_child(void) {
   list_t instance = list_new(&tracking_vtable);
   tracking_list_t *tracked = tracking_list_impl(&instance);
   node_t root = {0};
   tracked->expose_tree = true;
   tracked->tree = (tree_t){.root = &root, .size = 0};
   (void)list_tree(&instance);
}

static void test_list_tree_size_mismatch_child(void) {
   list_t instance = list_new(&tracking_vtable);
   tracking_list_t *tracked = tracking_list_impl(&instance);
   node_t root = {0};
   tracked->size = 1;
   tracked->expose_tree = true;
   tracked->tree = (tree_t){.root = &root, .size = 2};
   (void)list_tree(&instance);
}

static void test_list_each_under_visits_child(void) {
   list_t instance = list_new(&tracking_vtable);
   tracking_list_t *tracked = tracking_list_impl(&instance);
   tracked->size = 3;
   tracked->omit_last_each = true;
   list_each(&instance, list_contract_visit_noop, NULL);
}

static void test_list_each_over_visits_child(void) {
   list_t instance = list_new(&tracking_vtable);
   tracking_list_t *tracked = tracking_list_impl(&instance);
   tracked->size = 3;
   tracked->extra_each = true;
   list_each(&instance, list_contract_visit_noop, NULL);
}

static void test_list_each_drops_context_child(void) {
   list_t instance = list_new(&tracking_vtable);
   tracking_list_t *tracked = tracking_list_impl(&instance);
   tracked->size = 1;
   tracked->drop_each_ctx = true;
   list_each(&instance, list_contract_visit_noop, NULL);
}

static void test_list_verify_drops_context_child(void) {
   list_t instance = list_new(&tracking_vtable);
   tracking_list_t *tracked = tracking_list_impl(&instance);
   tracked->size = 1;
   tracked->drop_each_ctx = true;
   list_verify(&instance);
}

static void test_list_split_requires_initialized_outputs_child(void) {
   list_t instance = list_new(&tracking_vtable);
   tracking_list_t *tracked = tracking_list_impl(&instance);
   tracked->size = 3;
   tracked->omit_split_vtable = true;

   list_t left = {.vt = instance.vt};
   list_t right = {.vt = instance.vt};
   list_split(&instance, 1, &left, &right);
}

static void test_list_free_requires_cleared_impl_child(void) {
   list_t instance = list_new(&tracking_vtable);
   tracking_list_impl(&instance)->leave_impl_on_free = true;
   list_free(&instance);
}

#if defined(DEBUG) && !defined(PERSISTENT)
static void test_list_clone_rejects_overlapping_tree_child(void) {
   list_t instance = list_new(&tracking_vtable);
   tracking_list_t *tracked = tracking_list_impl(&instance);
   node_t root = {0};
   tracked->size = 1;
   tracked->expose_tree = true;
   tracked->tree = (tree_t){.root = &root, .size = 1};
   (void)list_clone(&instance);
}
#endif

#ifdef DEBUG
static void test_list_split_rejects_overlapping_tree_outputs_child(void) {
   list_t instance = list_new(&tracking_vtable);
   tracking_list_t *tracked = tracking_list_impl(&instance);
   tracked->size = 2;
   tracked->overlap_split_trees = true;

   list_t left;
   list_t right;
   list_split(&instance, 1, &left, &right);
}
#endif

static void test_list_verify_conc_duplicate_child_child(void) {
   node_t leaf = {.s = 1};
   node_t root = {
       .l = &leaf,
       .r = &leaf,
       .s = 2,
       .y = 1,
   };
   conc_t conc = {.tree = {.root = &root, .size = 2}};
   list_t list = {.impl = &conc, .vt = &conc_vtable};
   list_verify(&list);
}

static void test_list_verify_conc_root_size_mismatch_child(void) {
   node_t root = {0};
   conc_t conc = {.tree = {.root = &root, .size = 0}};
   list_t list = {.impl = &conc, .vt = &conc_vtable};
   list_verify(&list);
}

static void test_list_verify_conc_wrong_size_child(void) {
   node_t left = {.s = 1};
   node_t right = {.s = 1};
   node_t root = {
       .l = &left,
       .r = &right,
       .s = 3,
       .y = 1,
   };
   conc_t conc = {.tree = {.root = &root, .size = 2}};
   list_t list = {.impl = &conc, .vt = &conc_vtable};
   list_verify(&list);
}

static void test_list_verify_conc_wrong_height_child(void) {
   node_t left = {.s = 1};
   node_t right = {.s = 1};
   node_t root = {
       .l = &left,
       .r = &right,
       .s = 2,
       .y = 0,
   };
   conc_t conc = {.tree = {.root = &root, .size = 2}};
   list_t list = {.impl = &conc, .vt = &conc_vtable};
   list_verify(&list);
}

static void test_list_verify_conc_unbalanced_height_child(void) {
   node_t a = {.s = 1};
   node_t b = {.s = 1};
   node_t c = {.s = 1};
   node_t d = {.s = 1};
   node_t left_left = {
       .l = &a,
       .r = &b,
       .s = 2,
       .y = 1,
   };
   node_t left_right = {
       .l = &c,
       .r = &d,
       .s = 2,
       .y = 1,
   };
   node_t left = {
       .l = &left_left,
       .r = &left_right,
       .s = 4,
       .y = 2,
   };
   node_t right = {.s = 1};
   node_t root = {
       .l = &left,
       .r = &right,
       .s = 5,
       .y = 3,
   };
   conc_t conc = {.tree = {.root = &root, .size = 5}};
   list_t list = {.impl = &conc, .vt = &conc_vtable};
   list_verify(&list);
}

static void assert_conc_tree_accessor_accepts_non_rank_root(void) {
   node_t left = {.s = 1};
   node_t right = {.s = 1};
   node_t root = {
       .l = &left,
       .r = &right,
       .s = 2,
       .y = 1,
   };
   conc_t conc = {.tree = {.root = &root, .size = 2}};
   list_t list = {.impl = &conc, .vt = &conc_vtable};
   tree_t *tree = list_tree(&list);
   invariant(tree == &conc.tree);
   invariant(tree->root == &root);
   invariant(tree->size == 2);
}

static list_t conc_bad_root_count_list(conc_t *conc, node_t *root, node_t *left,
                                       node_t *right) {
   invariant(conc != NULL);
   invariant(root != NULL);
   invariant(left != NULL);
   invariant(right != NULL);
   *left = (node_t){.s = 1};
   *right = (node_t){.s = 1};
   *root = (node_t){
       .l = left,
       .r = right,
       .s = 3,
       .y = 1,
   };
   *conc = (conc_t){.tree = {.root = root, .size = 2}};
   return (list_t){.impl = conc, .vt = &conc_vtable};
}

static void test_conc_size_bad_root_count_child(void) {
   conc_t conc;
   node_t root;
   node_t left;
   node_t right;
   list_t list = conc_bad_root_count_list(&conc, &root, &left, &right);
   (void)conc_vtable.size_fn(&list);
}

static void test_conc_each_bad_root_count_child(void) {
   conc_t conc;
   node_t root;
   node_t left;
   node_t right;
   list_t list = conc_bad_root_count_list(&conc, &root, &left, &right);
   conc_vtable.each_fn(&list, list_contract_visit_noop, NULL);
}

static void test_conc_free_bad_root_count_child(void) {
   conc_t conc;
   node_t root;
   node_t left;
   node_t right;
   list_t list = conc_bad_root_count_list(&conc, &root, &left, &right);
   conc_vtable.free_fn(&list);
}

static void test_conc_select_bad_root_count_child(void) {
   conc_t conc;
   node_t root;
   node_t left;
   node_t right;
   list_t list = conc_bad_root_count_list(&conc, &root, &left, &right);
   (void)conc_vtable.select_fn(&list, 0);
}

static void assert_conc_cheap_handle_guards_abort(void) {
   assert_aborts(test_conc_size_bad_root_count_child);
   assert_aborts(test_conc_each_bad_root_count_child);
   assert_aborts(test_conc_free_bad_root_count_child);
   assert_aborts(test_conc_select_bad_root_count_child);
}

static list_t conc_one_child_root_list(conc_t *conc, node_t *root,
                                       node_t *left) {
   invariant(conc != NULL);
   invariant(root != NULL);
   invariant(left != NULL);
   *left = (node_t){.s = 1};
   *root = (node_t){
       .l = left,
       .s = 1,
       .y = 1,
   };
   *conc = (conc_t){.tree = {.root = root, .size = 1}};
   return (list_t){.impl = conc, .vt = &conc_vtable};
}

static void test_conc_size_one_child_root_child(void) {
   conc_t conc;
   node_t root;
   node_t left;
   list_t list = conc_one_child_root_list(&conc, &root, &left);
   (void)conc_vtable.size_fn(&list);
}

static void test_conc_each_one_child_root_child(void) {
   conc_t conc;
   node_t root;
   node_t left;
   list_t list = conc_one_child_root_list(&conc, &root, &left);
   conc_vtable.each_fn(&list, list_contract_visit_noop, NULL);
}

static void test_conc_free_one_child_root_child(void) {
   conc_t conc;
   node_t root;
   node_t left;
   list_t list = conc_one_child_root_list(&conc, &root, &left);
   conc_vtable.free_fn(&list);
}

static void test_conc_select_one_child_root_child(void) {
   conc_t conc;
   node_t root;
   node_t left;
   list_t list = conc_one_child_root_list(&conc, &root, &left);
   (void)conc_vtable.select_fn(&list, 0);
}

static void assert_conc_root_shape_guards_abort(void) {
   assert_aborts(test_conc_size_one_child_root_child);
   assert_aborts(test_conc_each_one_child_root_child);
   assert_aborts(test_conc_free_one_child_root_child);
   assert_aborts(test_conc_select_one_child_root_child);
}

#ifndef PERSISTENT
static void test_list_clone_conc_duplicate_child_child(void) {
   node_t leaf = {.s = 1};
   node_t root = {
       .l = &leaf,
       .r = &leaf,
       .s = 2,
       .y = 1,
   };
   conc_t conc = {.tree = {.root = &root, .size = 2}};
   list_t list = {.impl = &conc, .vt = &conc_vtable};
   (void)list_clone(&list);
}

static void test_list_clone_conc_root_size_mismatch_child(void) {
   node_t root = {0};
   conc_t conc = {.tree = {.root = &root, .size = 0}};
   list_t list = {.impl = &conc, .vt = &conc_vtable};
   (void)list_clone(&list);
}

static void test_list_clone_conc_wrong_size_child(void) {
   node_t left = {.s = 1};
   node_t right = {.s = 1};
   node_t root = {
       .l = &left,
       .r = &right,
       .s = 3,
       .y = 1,
   };
   conc_t conc = {.tree = {.root = &root, .size = 2}};
   list_t list = {.impl = &conc, .vt = &conc_vtable};
   (void)list_clone(&list);
}

static void test_list_clone_conc_wrong_height_child(void) {
   node_t left = {.s = 1};
   node_t right = {.s = 1};
   node_t root = {
       .l = &left,
       .r = &right,
       .s = 2,
       .y = 0,
   };
   conc_t conc = {.tree = {.root = &root, .size = 2}};
   list_t list = {.impl = &conc, .vt = &conc_vtable};
   (void)list_clone(&list);
}

#ifdef DEBUG
static void test_tree_prepare_join_roots_overlap_child(void) {
   node_t left_leaf = {.s = 1};
   node_t shared_leaf = {.s = 1};
   node_t right_leaf = {.s = 1};
   node_t left_root = {
       .l = &left_leaf,
       .r = &shared_leaf,
       .s = 1,
       .y = 1,
   };
   node_t right_root = {
       .l = &shared_leaf,
       .r = &right_leaf,
       .s = 1,
       .y = 1,
   };
   tree_t left = {.root = &left_root, .size = 2};
   tree_t right = {.root = &right_root, .size = 2};

   invariant(tree_roots_overlap(&left, &right));
   (void)tree_prepare_join_roots(&left, &right);
}
#endif
#endif

#ifdef PERSISTENT
static void assert_conc_persistent_join_reuses_disjoint_roots(void) {
   list_t left = list_new(&conc_vtable);
   list_t right = list_new(&conc_vtable);
   for (u64 i = 0; i < 2; i++) {
      list_insert(&left, list_size(&left), i);
      list_insert(&right, list_size(&right), 10 + i);
   }

   tree_t *left_tree = list_tree(&left);
   tree_t *right_tree = list_tree(&right);
   node_t *left_root = left_tree->root;
   node_t *right_root = right_tree->root;
   invariant(left_root != NULL);
   invariant(right_root != NULL);
   invariant(left_root != right_root);

   list_t joined = list_join(&left, &right);
   tree_t *joined_tree = list_tree(&joined);
   invariant(joined_tree->root != NULL);
   invariant(joined_tree->root->l == left_root);
   invariant(joined_tree->root->r == right_root);
   invariant(tree_shared(left_root));
   invariant(tree_shared(right_root));
   assert_equal(4, list_size(&joined));
   assert_equal(0, list_select(&joined, 0));
   assert_equal(1, list_select(&joined, 1));
   assert_equal(10, list_select(&joined, 2));
   assert_equal(11, list_select(&joined, 3));

   list_update(&joined, 0, 9000);
   assert_equal(0, list_select(&left, 0));
   assert_equal(9000, list_select(&joined, 0));
   list_verify(&left);
   list_verify(&right);
   list_verify(&joined);

   list_free(&joined);
   list_free(&right);
   list_free(&left);
}

static void assert_conc_persistent_join_isolates_overlapping_roots(void) {
   list_t source = list_new(&conc_vtable);
   list_insert(&source, 0, 0);
   list_insert(&source, 1, 1);

   list_t joined = list_join(&source, &source);
   tree_t *joined_tree = list_tree(&joined);
   invariant(joined_tree->root != NULL);
   invariant(joined_tree->root->l != NULL);
   invariant(joined_tree->root->r != NULL);
   invariant(joined_tree->root->l != joined_tree->root->r);
   tree_t joined_left = {
       .root = joined_tree->root->l,
       .size = joined_tree->root->l->s,
   };
   tree_t joined_right = {
       .root = joined_tree->root->r,
       .size = joined_tree->root->r->s,
   };
   invariant(!tree_roots_overlap(&joined_left, &joined_right));
   assert_equal(4, list_size(&joined));
   assert_equal(0, list_select(&joined, 0));
   assert_equal(1, list_select(&joined, 1));
   assert_equal(0, list_select(&joined, 2));
   assert_equal(1, list_select(&joined, 3));

   list_update(&joined, 2, 9001);
   assert_equal(0, list_select(&source, 0));
   assert_equal(1, list_select(&source, 1));
   assert_equal(9001, list_select(&joined, 2));
   list_verify(&source);
   list_verify(&joined);

   list_free(&joined);
   list_free(&source);
}
#endif

static void test_tree_list_join_size_overflow_child(void) {
   (void)tree_list_join_size(UINT64_MAX, 1);
}

static void test_tree_list_split_right_size_underflow_child(void) {
   (void)tree_list_split_right_size(7, 8);
}

static void assert_tree_list_split_result_boundaries(void) {
   list_t instance = list_new(&tracking_vtable);
   tracking_list_impl(&instance)->size = 3;
   list_t left;
   list_t right;

   tree_list_require_split_result(&instance, 0, 0, 3, &left, &right);
   tree_list_require_split_result(&instance, 3, 3, 0, &left, &right);

   list_free(&instance);
}

static void assert_tree_list_prepared_split_result_boundaries(void) {
   list_t instance = list_new(&tracking_vtable);
   list_t left;
   list_t right;

   tree_list_require_prepared_split_result(&instance, 0, 3, 0, 3, &left,
                                           &right);
   tree_list_require_prepared_split_result(&instance, 3, 3, 3, 0, &left,
                                           &right);

   list_free(&instance);
}

static void test_tree_list_split_result_left_mismatch_child(void) {
   list_t instance = list_new(&tracking_vtable);
   tracking_list_impl(&instance)->size = 3;
   list_t left;
   list_t right;
   tree_list_require_split_result(&instance, 1, 2, 2, &left, &right);
}

static void test_tree_list_prepared_split_result_left_mismatch_child(void) {
   list_t instance = list_new(&tracking_vtable);
   list_t left;
   list_t right;
   tree_list_require_prepared_split_result(&instance, 1, 3, 2, 2, &left,
                                           &right);
}

static void test_tree_list_split_result_right_mismatch_child(void) {
   list_t instance = list_new(&tracking_vtable);
   tracking_list_impl(&instance)->size = 3;
   list_t left;
   list_t right;
   tree_list_require_split_result(&instance, 1, 1, 3, &left, &right);
}

static void test_tree_list_prepared_split_result_right_mismatch_child(void) {
   list_t instance = list_new(&tracking_vtable);
   list_t left;
   list_t right;
   tree_list_require_prepared_split_result(&instance, 1, 3, 1, 3, &left,
                                           &right);
}

static void test_tree_list_prepared_split_result_after_end_child(void) {
   list_t instance = list_new(&tracking_vtable);
   list_t left;
   list_t right;
   tree_list_require_prepared_split_result(&instance, 4, 3, 4, 0, &left,
                                           &right);
}

static void test_tree_list_join_result_size_mismatch_child(void) {
   list_t left = list_new(&tracking_vtable);
   list_t right = list_new(&tracking_vtable);
   tracking_list_impl(&left)->size = 2;
   tracking_list_impl(&right)->size = 3;
   tree_list_require_join_result(&left, &right, 4);
}

static void assert_tree_list_make_handle(void) {
   tracking_list_t impl = {0};
   list_t list = tree_list_make(&impl, &tracking_vtable);
   invariant(list.impl == &impl);
   invariant(list.vt == &tracking_vtable);
}

static void test_tree_list_make_null_impl_child(void) {
   (void)tree_list_make(NULL, &tracking_vtable);
}

static void test_tree_list_make_null_vtable_child(void) {
   tracking_list_t impl = {0};
   (void)tree_list_make(&impl, NULL);
}

static void test_tree_list_make_bad_vtable_child(void) {
   tracking_list_t impl = {0};
   list_vtable_t vt = tracking_vtable;
   vt.name = "";
   (void)tree_list_make(&impl, &vt);
}

static void assert_tree_list_alloc_impl_checked_tree(void) {
   node_t root = {0};
   conc_t *empty = (conc_t *)tree_list_alloc_impl(
       sizeof(conc_t), offsetof(conc_t, tree), NULL, 0, true);
   invariant(empty->tree.root == NULL);
   assert_equal(0, empty->tree.size);
   free(empty);

   conc_t *singleton = (conc_t *)tree_list_alloc_impl(
       sizeof(conc_t), offsetof(conc_t, tree), &root, 1, true);
   invariant(singleton->tree.root == &root);
   assert_equal(1, singleton->tree.size);
   free(singleton);
}

static void test_tree_list_alloc_impl_zero_size_child(void) {
   (void)tree_list_alloc_impl(0, 0, NULL, 0, false);
}

static void test_tree_list_alloc_impl_tree_outside_child(void) {
   (void)tree_list_alloc_impl(sizeof(conc_t), sizeof(conc_t) + 1, NULL, 0,
                              false);
}

static void test_tree_list_alloc_impl_tree_overruns_child(void) {
   (void)tree_list_alloc_impl(sizeof(tree_t) - 1, 0, NULL, 0, false);
}

static void test_tree_list_alloc_impl_misaligned_tree_child(void) {
   invariant(_Alignof(tree_t) > 1);
   (void)tree_list_alloc_impl(sizeof(conc_t), 1, NULL, 0, false);
}

static void test_tree_list_alloc_impl_wrong_size_child(void) {
   node_t root = {0};
   (void)tree_list_alloc_impl(sizeof(conc_t), offsetof(conc_t, tree), &root, 2,
                              true);
   bst_unreachable();
}

static void test_tree_list_alloc_impl_bad_root_rank_child(void) {
   node_t root = {.s = 1};
   (void)tree_list_alloc_impl(sizeof(conc_t), offsetof(conc_t, tree), &root, 1,
                              true);
}

static void assert_tree_list_clone_impl_empty_conc(void) {
   conc_t original = {0};
   list_t list = {.impl = &original, .vt = &conc_vtable};
   list_t clone =
       tree_list_clone_impl(&list, &original, sizeof(original), &original.tree);
   invariant(clone.vt == &conc_vtable);
   invariant(clone.impl != NULL);
   invariant(clone.impl != &original);
   assert_equal(0, list_size(&clone));
   list_free(&clone);
}

static void test_tree_list_clone_impl_mismatched_impl_child(void) {
   conc_t original = {0};
   conc_t other = {0};
   list_t list = {.impl = &original, .vt = &conc_vtable};
   (void)tree_list_clone_impl(&list, &other, sizeof(other), &other.tree);
}

static void test_tree_list_clone_impl_outside_tree_child(void) {
   conc_t original = {0};
   list_t list = {.impl = &original, .vt = &conc_vtable};
   tree_t *outside = (tree_t *)((char *)&original + sizeof(original));
   (void)tree_list_clone_impl(&list, &original, sizeof(original), outside);
}

static void test_tree_list_clone_impl_misaligned_tree_child(void) {
   invariant(_Alignof(tree_t) > 1);
   conc_t original = {0};
   list_t list = {.impl = &original, .vt = &conc_vtable};
   tree_t *misaligned = (tree_t *)((char *)&original + 1);
   (void)tree_list_clone_impl(&list, &original, sizeof(original), misaligned);
}

static void test_tree_list_clone_impl_bad_root_rank_child(void) {
   node_t root = {.s = 1};
   splay_t original = {.tree = {.root = &root, .size = 1}};
   list_t list = {.impl = &original, .vt = &splay_vtable};
   (void)tree_list_clone_impl(&list, &original, sizeof(original),
                              &original.tree);
}

static void test_tree_list_free_impl_mismatched_impl_child(void) {
   conc_t original = {0};
   conc_t other = {0};
   list_t list = {.impl = &original, .vt = &conc_vtable};
   tree_list_free_impl(&list, &other, sizeof(other), &other.tree);
}

static void test_tree_list_free_impl_outside_tree_child(void) {
   conc_t original = {0};
   list_t list = {.impl = &original, .vt = &conc_vtable};
   tree_t *outside = (tree_t *)((char *)&original + sizeof(original));
   tree_list_free_impl(&list, &original, sizeof(original), outside);
}

static void test_tree_list_free_impl_misaligned_tree_child(void) {
   invariant(_Alignof(tree_t) > 1);
   conc_t original = {0};
   list_t list = {.impl = &original, .vt = &conc_vtable};
   tree_t *misaligned = (tree_t *)((char *)&original + 1);
   tree_list_free_impl(&list, &original, sizeof(original), misaligned);
}

static void test_tree_list_inserted_overflow_child(void) {
   node_t root = {0};
   tree_t tree = {.root = &root, .size = UINT64_MAX};
   tree_list_inserted(&tree);
}

static void test_tree_list_inserted_null_child(void) {
   tree_list_inserted(NULL);
}

static void test_tree_list_inserted_missing_root_child(void) {
   tree_t tree = {0};
   tree_list_inserted(&tree);
}

static void test_tree_list_sized_inserted_bad_root_rank_child(void) {
   node_t root = {.s = 2};
   tree_t tree = {.root = &root, .size = 1};
   tree_list_sized_inserted(&tree);
}

static void test_tree_list_deleted_empty_child(void) {
   tree_t tree = {0};
   tree_list_deleted(&tree);
}

static void test_tree_list_deleted_stale_root_child(void) {
   node_t root = {0};
   tree_t tree = {.root = &root, .size = 1};
   tree_list_deleted(&tree);
}

static void test_tree_list_deleted_null_child(void) {
   tree_list_deleted(NULL);
}

static void test_tree_list_sized_deleted_bad_root_rank_child(void) {
   node_t root = {.s = 2};
   tree_t tree = {.root = &root, .size = 3};
   tree_list_sized_deleted(&tree);
}

static void test_tree_list_make_tree_null_root_nonzero_size_child(void) {
   (void)tree_list_make_tree(NULL, 1);
}

static void test_tree_list_make_tree_root_zero_size_child(void) {
   node_t root = {0};
   (void)tree_list_make_tree(&root, 0);
}

static void test_tree_list_make_sized_tree_bad_root_rank_child(void) {
   node_t root = {.s = 1};
   (void)tree_list_make_sized_tree(&root, 1);
}

static void TestListContracts(void) {
   printf("List contracts:\n");

   assert_list_verify_verifies_before_each();
   printf("  verifier runs before traversal OK\n");

   assert_aborts(test_list_verify_mutating_size_child);
   printf("  verifier size preservation guard OK\n");

   assert_aborts(test_list_vtable_null_child);
   assert_aborts(test_list_vtable_empty_name_child);
   assert_aborts(test_list_vtable_dot_name_child);
   assert_aborts(test_list_vtable_dotdot_name_child);
   assert_aborts(test_list_vtable_slash_name_child);
   assert_aborts(test_list_vtable_space_name_child);
   assert_aborts(test_list_vtable_high_byte_name_child);
   assert_aborts(test_list_vtable_missing_size_child);
   assert_aborts(test_list_new_null_vtable_child);
   assert_aborts(test_list_size_null_child);
   assert_aborts(test_list_size_uninitialized_child);
   assert_aborts(test_list_free_uninitialized_with_impl_child);
   assert_aborts(test_list_each_null_visit_child);
   assert_aborts(test_list_select_empty_child);
   assert_aborts(test_list_update_empty_child);
   assert_aborts(test_list_insert_after_end_child);
   assert_aborts(test_list_delete_empty_child);
   assert_aborts(test_list_split_null_left_child);
   assert_aborts(test_list_split_null_right_child);
   assert_aborts(test_list_split_left_is_self_child);
   assert_aborts(test_list_split_right_is_self_child);
   assert_aborts(test_list_split_same_output_child);
   assert_aborts(test_list_split_after_end_child);
   assert_aborts(test_list_join_null_left_child);
   assert_aborts(test_list_join_null_right_child);
   assert_aborts(test_list_join_mismatched_vtable_child);
   assert_aborts(test_list_join_aliasing_handle_child);
   assert_aborts(test_list_tree_null_root_nonzero_size_child);
   assert_aborts(test_list_tree_root_zero_size_child);
   assert_aborts(test_list_tree_size_mismatch_child);
   printf("  public dispatcher guard rails OK\n");

   assert_tree_registry_direct_vtable_contract_guards_abort();
   printf("  direct vtable handle guards OK\n");

   assert_tree_list_common_sized_handle_guards_abort();
   printf("  generated tree vtable sized-handle guards OK\n");

   assert_splay_sized_handle_guards_abort();
   printf("  Splay vtable sized-handle guards OK\n");

   assert_treap_fingertree_sized_handle_guards_abort();
   printf("  finger-tree vtable sized-handle guards OK\n");

   assert_aborts(test_tree_list_wb_insert_bad_post_rank_child);
   printf("  generated WB insert postcondition guard OK\n");

#ifdef DEBUG
   assert_tree_registry_direct_vtable_split_wrong_size_aborts();
   printf("  direct vtable split stale-size guards OK\n");
#endif

   assert_aborts(test_list_each_under_visits_child);
   assert_aborts(test_list_each_over_visits_child);
   assert_aborts(test_list_each_drops_context_child);
   assert_aborts(test_list_verify_drops_context_child);
   printf("  traversal callback guard OK\n");

   assert_aborts(test_list_split_requires_initialized_outputs_child);
   printf("  split output initialization guard OK\n");

   assert_aborts(test_list_free_requires_cleared_impl_child);
   printf("  free implementation lifecycle guard OK\n");

#ifdef DEBUG
   assert_aborts(test_list_split_rejects_overlapping_tree_outputs_child);
   printf("  split tree output disjointness guard OK\n");
#endif

#if defined(DEBUG) && !defined(PERSISTENT)
   assert_aborts(test_list_clone_rejects_overlapping_tree_child);
   printf("  nonpersistent clone tree disjointness guard OK\n");
#endif

   assert_aborts(test_list_verify_conc_duplicate_child_child);
   assert_aborts(test_list_verify_conc_root_size_mismatch_child);
   assert_aborts(test_list_verify_conc_wrong_size_child);
   assert_aborts(test_list_verify_conc_wrong_height_child);
   assert_aborts(test_list_verify_conc_unbalanced_height_child);
   assert_conc_tree_accessor_accepts_non_rank_root();
   assert_conc_cheap_handle_guards_abort();
   assert_conc_root_shape_guards_abort();
   printf("  tree-backed verifier rejects malformed nodes OK\n");

   assert_equal(UINT64_MAX, tree_list_join_size(UINT64_MAX - 1, 1));
   assert_equal(UINT64_MAX, tree_list_join_size(0, UINT64_MAX));
   assert_equal(7, tree_list_split_right_size(7, 0));
   assert_equal(4, tree_list_split_right_size(7, 3));
   assert_equal(0, tree_list_split_right_size(7, 7));
   assert_tree_list_split_result_boundaries();
   assert_tree_list_prepared_split_result_boundaries();
   tree_t empty_tree = tree_list_make_tree(NULL, 0);
   invariant(empty_tree.root == NULL);
   assert_equal(0, empty_tree.size);
   node_t size_root = {0};
   tree_t size_tree = {.root = &size_root, .size = 2};
   tree_t singleton_tree = tree_list_make_tree(&size_root, 1);
   invariant(singleton_tree.root == &size_root);
   assert_equal(1, singleton_tree.size);
   tree_t sized_singleton_tree = tree_list_make_sized_tree(&size_root, 1);
   invariant(sized_singleton_tree.root == &size_root);
   assert_equal(1, sized_singleton_tree.size);
   tree_list_inserted(&size_tree);
   assert_equal(3, size_tree.size);
   tree_list_deleted(&size_tree);
   assert_equal(2, size_tree.size);
   node_t sized_right = {0};
   node_t sized_root = {.r = &sized_right};
   tree_t sized_size_tree = {.root = &sized_root, .size = 2};
   tree_list_sized_inserted(&sized_size_tree);
   assert_equal(3, sized_size_tree.size);
   tree_list_sized_deleted(&sized_size_tree);
   assert_equal(2, sized_size_tree.size);
   assert_tree_list_make_handle();
   assert_tree_list_alloc_impl_checked_tree();
   assert_tree_list_clone_impl_empty_conc();
   assert_aborts(test_tree_list_make_tree_null_root_nonzero_size_child);
   assert_aborts(test_tree_list_make_tree_root_zero_size_child);
   assert_aborts(test_tree_list_make_sized_tree_bad_root_rank_child);
   assert_aborts(test_tree_list_join_size_overflow_child);
   assert_aborts(test_tree_list_split_right_size_underflow_child);
   assert_aborts(test_tree_list_split_result_left_mismatch_child);
   assert_aborts(test_tree_list_split_result_right_mismatch_child);
   assert_aborts(test_tree_list_prepared_split_result_left_mismatch_child);
   assert_aborts(test_tree_list_prepared_split_result_right_mismatch_child);
   assert_aborts(test_tree_list_prepared_split_result_after_end_child);
   assert_aborts(test_tree_list_join_result_size_mismatch_child);
   assert_aborts(test_tree_list_make_null_impl_child);
   assert_aborts(test_tree_list_make_null_vtable_child);
   assert_aborts(test_tree_list_make_bad_vtable_child);
   assert_aborts(test_tree_list_alloc_impl_zero_size_child);
   assert_aborts(test_tree_list_alloc_impl_tree_outside_child);
   assert_aborts(test_tree_list_alloc_impl_tree_overruns_child);
   assert_aborts(test_tree_list_alloc_impl_misaligned_tree_child);
   assert_aborts(test_tree_list_alloc_impl_bad_root_rank_child);
#ifdef DEBUG
   assert_aborts(test_tree_list_alloc_impl_wrong_size_child);
#endif
   assert_aborts(test_tree_list_clone_impl_mismatched_impl_child);
   assert_aborts(test_tree_list_clone_impl_outside_tree_child);
   assert_aborts(test_tree_list_clone_impl_misaligned_tree_child);
   assert_aborts(test_tree_list_clone_impl_bad_root_rank_child);
   assert_aborts(test_tree_list_free_impl_mismatched_impl_child);
   assert_aborts(test_tree_list_free_impl_outside_tree_child);
   assert_aborts(test_tree_list_free_impl_misaligned_tree_child);
   assert_aborts(test_tree_list_inserted_null_child);
   assert_aborts(test_tree_list_inserted_overflow_child);
   assert_aborts(test_tree_list_inserted_missing_root_child);
   assert_aborts(test_tree_list_sized_inserted_bad_root_rank_child);
   assert_aborts(test_tree_list_deleted_null_child);
   assert_aborts(test_tree_list_deleted_empty_child);
   assert_aborts(test_tree_list_deleted_stale_root_child);
   assert_aborts(test_tree_list_sized_deleted_bad_root_rank_child);
   printf("  tree-backed size helper boundary guards OK\n");

#ifndef PERSISTENT
   assert_aborts(test_list_clone_conc_duplicate_child_child);
   assert_aborts(test_list_clone_conc_root_size_mismatch_child);
   assert_aborts(test_list_clone_conc_wrong_size_child);
   assert_aborts(test_list_clone_conc_wrong_height_child);
   printf("  tree-backed physical clone rejects malformed nodes OK\n");
#ifdef DEBUG
   assert_aborts(test_tree_prepare_join_roots_overlap_child);
   printf("  nonpersistent join preflight rejects overlapping roots OK\n");
#endif
#endif

#ifdef PERSISTENT
   assert_conc_persistent_join_reuses_disjoint_roots();
   assert_conc_persistent_join_isolates_overlapping_roots();
   printf("  persistent Conc join sharing and overlap isolation OK\n");
#endif

#ifdef DEBUG
   assert_list_each_verifies_before_each();
   printf("  debug traversal preflight verifier OK\n");

   assert_list_debug_capture_uses_raw_each();
   printf("  debug sequence capture uses raw traversal OK\n");

   assert_list_debug_verifier_on_dispatch();
   printf("  read and lifecycle verifier hooks OK\n");

   assert_aborts(test_list_contract_mutating_each_child);
   printf("  traversal input preservation guard OK\n");

   assert_aborts(test_list_contract_mutating_clone_child);
   printf("  clone input preservation guard OK\n");

   assert_aborts(test_list_contract_mutating_select_child);
   printf("  select input preservation guard OK\n");

   assert_aborts(test_list_contract_mutating_update_child);
   printf("  update sequence preservation guard OK\n");

   assert_aborts(test_list_contract_mutating_insert_child);
   printf("  insert sequence preservation guard OK\n");

   assert_aborts(test_list_contract_mutating_delete_child);
   printf("  delete sequence preservation guard OK\n");
#endif

#if defined(DEBUG) && defined(PERSISTENT)
   assert_aborts(test_list_contract_mutating_split_child);
   printf("  split input preservation guard OK\n");

   assert_aborts(test_list_contract_mutating_join_child);
   printf("  join input preservation guard OK\n");
#else
   printf("  dispatcher contract checks compiled OK\n");
#endif
}

static void TestReferenceList(void) {
   printf("Reference list:\n");

   list_t empty = list_new(&reference_vtable);
   list_t empty_left, empty_right;
   list_split(&empty, 0, &empty_left, &empty_right);
   assert_equal(0, list_size(&empty_left));
   assert_equal(0, list_size(&empty_right));

   list_t empty_joined = list_join(&empty_left, &empty_right);
   assert_equal(0, list_size(&empty_joined));

   list_t ref = list_new(&reference_vtable);
   for (u64 i = 0; i < NODE_STACK_INLINE_CAP + 3; i++) {
      list_insert(&ref, list_size(&ref), i);
   }
   list_verify(&ref);

   list_update(&ref, 1, 42);
   assert_equal(42, list_select(&ref, 1));

   list_t clone = list_clone(&ref);
   assert_list(&ref, &clone);
   list_update(&clone, 0, 9000);
   assert_equal(0, list_select(&ref, 0));
   assert_equal(9000, list_select(&clone, 0));

   list_t doubled = list_join(&ref, &ref);
   u64 ref_size = list_size(&ref);
   assert_equal(ref_size * 2, list_size(&doubled));
   for (u64 i = 0; i < ref_size; i++) {
      assert_equal(list_select(&ref, i), list_select(&doubled, i));
      assert_equal(list_select(&ref, i), list_select(&doubled, ref_size + i));
   }

   list_t empty_prefix = list_join(&empty, &ref);
   list_t empty_suffix = list_join(&ref, &empty);
   assert_list(&ref, &empty_prefix);
   assert_list(&ref, &empty_suffix);

   list_t all_left, all_right;
   list_split(&ref, list_size(&ref), &all_left, &all_right);
   assert_list(&ref, &all_left);
   assert_equal(0, list_size(&all_right));

   list_t none_left, none_right;
   list_split(&ref, 0, &none_left, &none_right);
   assert_equal(0, list_size(&none_left));
   assert_list(&ref, &none_right);

   list_t left, right;
   list_split(&ref, NODE_STACK_INLINE_CAP, &left, &right);
   assert_equal(NODE_STACK_INLINE_CAP, list_size(&left));
   assert_equal(3, list_size(&right));

   list_t joined = list_join(&left, &right);
   assert_list(&ref, &joined);
   u64 left_first = list_select(&left, 0);
   u64 right_first = list_select(&right, 0);
   list_update(&left, 0, 9001);
   list_update(&right, 0, 9002);
   assert_equal(left_first, list_select(&joined, 0));
   assert_equal(right_first, list_select(&joined, NODE_STACK_INLINE_CAP));
   assert_equal(left_first, list_select(&ref, 0));
   assert_equal(right_first, list_select(&ref, NODE_STACK_INLINE_CAP));
   list_update(&joined, 0, 9003);
   assert_equal(9001, list_select(&left, 0));
   assert_equal(left_first, list_select(&ref, 0));

   list_t copied = reference_for(&ref);
   assert_list(&ref, &copied);
   u64 copied_size = list_size(&copied);
   u64 copied_first = list_select(&copied, 0);
   u64 copied_last = list_select(&copied, copied_size - 1);
   list_update(&ref, 0, 7001);
   list_insert(&ref, 1, 7002);
   assert_equal(copied_size, list_size(&copied));
   assert_equal(copied_first, list_select(&copied, 0));
   assert_equal(copied_last, list_select(&copied, copied_size - 1));
   list_update(&copied, 0, 7003);
   assert_equal(7001, list_select(&ref, 0));

   list_t copied_empty = reference_for(&empty);
   assert_equal(0, list_size(&copied_empty));
   list_verify(&copied_empty);

   list_t deleted = list_new(&reference_vtable);
   for (u64 i = 0; i < 5; i++) {
      list_insert(&deleted, list_size(&deleted), 10 + i);
   }
   assert_equal(12, list_delete(&deleted, 2));
   assert_equal(10, list_delete(&deleted, 0));
   assert_equal(14, list_delete(&deleted, list_size(&deleted) - 1));
   assert_equal(2, list_size(&deleted));
   assert_equal(11, list_select(&deleted, 0));
   assert_equal(13, list_select(&deleted, 1));
   list_verify(&deleted);

   list_t sized = reference_of_size(7);
   assert_equal(7, list_size(&sized));
   list_verify(&sized);

   list_t sized_empty = reference_of_size(0);
   assert_equal(0, list_size(&sized_empty));
   list_verify(&sized_empty);

   while (list_size(&joined) > 0) {
      list_delete(&joined, list_size(&joined) - 1);
      list_verify(&joined);
   }

   list_free(&sized_empty);
   list_free(&sized);
   list_free(&deleted);
   list_free(&copied_empty);
   list_free(&copied);
   list_free(&joined);
   list_free(&left);
   list_free(&right);
   list_free(&none_left);
   list_free(&none_right);
   list_free(&all_left);
   list_free(&all_right);
   list_free(&empty_suffix);
   list_free(&empty_prefix);
   list_free(&doubled);
   list_free(&clone);
   list_free(&ref);
   list_free(&empty_joined);
   list_free(&empty_left);
   list_free(&empty_right);
   list_free(&empty);
   printf("  clone/split/join/delete OK\n");
}

static void test_bst_malloc_zero_child(void) {
   (void)bst_malloc(0);
}

static void test_bst_calloc_zero_count_child(void) {
   (void)bst_calloc(0, 1);
}

static void test_bst_calloc_zero_size_child(void) {
   (void)bst_calloc(1, 0);
   bst_unreachable();
}

static void test_bst_calloc_overflow_child(void) {
   (void)bst_calloc(SIZE_MAX, 2);
   bst_unreachable();
}

static void test_bst_calloc_array_zero_count_child(void) {
   (void)bst_calloc_array(0, 1);
}

static void test_bst_calloc_array_zero_size_child(void) {
   (void)bst_calloc_array(1, 0);
   bst_unreachable();
}

static void test_bst_calloc_array_overflow_child(void) {
   (void)bst_calloc_array(UINT64_MAX, 2);
   bst_unreachable();
}

static void test_bst_array_max_count_zero_size_child(void) {
   (void)bst_array_max_count(0);
}

static void test_bst_array_bytes_zero_count_child(void) {
   (void)bst_array_bytes(0, sizeof(u64));
}

static void test_bst_array_bytes_zero_size_child(void) {
   (void)bst_array_bytes(1, 0);
}

static void test_bst_array_bytes_overflow_child(void) {
   (void)bst_array_bytes(UINT64_MAX, 2);
}

static void test_bst_array_copy_null_dst_child(void) {
   u64 src = 1;
   bst_array_copy(NULL, &src, 1, sizeof(src));
}

static void test_bst_array_copy_null_src_child(void) {
   u64 dst = 0;
   bst_array_copy(&dst, NULL, 1, sizeof(dst));
}

static void test_bst_array_copy_zero_size_child(void) {
   u64 dst = 0;
   u64 src = 1;
   bst_array_copy(&dst, &src, 1, 0);
}

static void test_bst_array_copy_same_range_child(void) {
   u64 values[2] = {1, 2};
   bst_array_copy(values, values, 2, sizeof(values[0]));
}

static void test_bst_array_copy_overlap_forward_child(void) {
   u64 values[4] = {1, 2, 3, 4};
   bst_array_copy(&values[1], values, 3, sizeof(values[0]));
}

static void test_bst_array_copy_overlap_backward_child(void) {
   u64 values[4] = {1, 2, 3, 4};
   bst_array_copy(values, &values[1], 3, sizeof(values[0]));
}

static void test_bst_array_copy_dst_range_overflow_child(void) {
   u64 src = 1;
   void *dst = (void *)(uintptr_t)(UINTPTR_MAX - sizeof(src) + 1);
   bst_array_copy(dst, &src, 1, sizeof(src));
}

static void test_bst_array_copy_src_range_overflow_child(void) {
   u64 dst = 0;
   const void *src = (const void *)(uintptr_t)(UINTPTR_MAX - sizeof(dst) + 1);
   bst_array_copy(&dst, src, 1, sizeof(dst));
}

static void test_bst_array_move_null_dst_child(void) {
   u64 src = 1;
   bst_array_move(NULL, &src, 1, sizeof(src));
}

static void test_bst_array_move_null_src_child(void) {
   u64 dst = 0;
   bst_array_move(&dst, NULL, 1, sizeof(dst));
}

static void test_bst_array_move_zero_size_child(void) {
   u64 values[2] = {1, 2};
   bst_array_move(&values[1], values, 1, 0);
}

static void test_bst_array_move_dst_range_overflow_child(void) {
   u64 src = 1;
   void *dst = (void *)(uintptr_t)(UINTPTR_MAX - sizeof(src) + 1);
   bst_array_move(dst, &src, 1, sizeof(src));
}

static void test_bst_array_move_src_range_overflow_child(void) {
   u64 dst = 0;
   const void *src = (const void *)(uintptr_t)(UINTPTR_MAX - sizeof(dst) + 1);
   bst_array_move(&dst, src, 1, sizeof(dst));
}

static void test_bst_array_zero_null_child(void) {
   bst_array_zero(NULL, 1, sizeof(u64));
}

static void test_bst_array_zero_zero_size_child(void) {
   u64 value = 1;
   bst_array_zero(&value, 1, 0);
}

static void test_bst_array_zero_range_overflow_child(void) {
   void *ptr = (void *)(uintptr_t)(UINTPTR_MAX - sizeof(u64) + 1);
   bst_array_zero(ptr, 1, sizeof(u64));
}

static void test_bst_array_next_cap_filled_child(void) {
   (void)bst_array_next_cap(8, 8, 8, sizeof(u64));
}

static void test_bst_array_next_cap_zero_initial_child(void) {
   (void)bst_array_next_cap(0, 1, 0, sizeof(u64));
}

static void test_bst_array_next_cap_zero_size_child(void) {
   (void)bst_array_next_cap(0, 1, 8, 0);
}

static void test_bst_array_next_cap_overflow_child(void) {
   (void)bst_array_next_cap(0, UINT64_MAX, 8, sizeof(u64));
}

static void test_bst_full_binary_tree_max_leaves_zero_nodes_child(void) {
   (void)bst_full_binary_tree_max_leaves_for_nodes(0);
}

static void test_bst_full_binary_tree_nodes_zero_leaves_child(void) {
   (void)bst_full_binary_tree_nodes_from_leaves(0);
}

static void test_bst_full_binary_tree_nodes_overflow_child(void) {
   (void)bst_full_binary_tree_nodes_from_leaves(UINT64_MAX / 2 + 2);
}

static void test_bst_malloc_array_zero_count_child(void) {
   (void)bst_malloc_array(0, 1);
}

static void test_bst_malloc_array_zero_size_child(void) {
   (void)bst_malloc_array(1, 0);
}

static void test_bst_malloc_array_overflow_child(void) {
   (void)bst_malloc_array(UINT64_MAX, 2);
}

static void test_bst_realloc_null_child(void) {
   (void)bst_realloc(NULL, 1);
}

static void test_bst_realloc_zero_size_child(void) {
   void *ptr = bst_malloc(1);
   (void)bst_realloc(ptr, 0);
}

static void test_bst_realloc_array_zero_count_child(void) {
   void *ptr = bst_malloc(1);
   (void)bst_realloc_array(ptr, 0, 1);
}

static void test_bst_realloc_array_zero_size_child(void) {
   void *ptr = bst_malloc(1);
   (void)bst_realloc_array(ptr, 1, 0);
}

static void test_bst_realloc_array_overflow_child(void) {
   void *ptr = bst_malloc(1);
   (void)bst_realloc_array(ptr, UINT64_MAX, 2);
   bst_unreachable();
}

static void test_stack_array_grow_null_data_child(void) {
   u64 cap = 1;
   u64 inline_data[1] = {0};
   (void)stack_array_grow(NULL, inline_data, cap, &cap, sizeof(inline_data[0]));
}

static void test_stack_array_grow_null_inline_child(void) {
   u64 cap = 1;
   u64 data[1] = {0};
   (void)stack_array_grow(data, NULL, cap, &cap, sizeof(data[0]));
}

static void test_stack_array_grow_null_cap_child(void) {
   u64 data[1] = {0};
   (void)stack_array_grow(data, data, 1, NULL, sizeof(data[0]));
}

static void test_stack_array_grow_not_full_child(void) {
   u64 cap = 2;
   u64 data[2] = {0};
   (void)stack_array_grow(data, data, 1, &cap, sizeof(data[0]));
}

static void test_stack_array_grow_zero_cap_child(void) {
   u64 cap = 0;
   u64 data[1] = {0};
   (void)stack_array_grow(data, data, 0, &cap, sizeof(data[0]));
}

static void test_stack_array_grow_zero_size_child(void) {
   u64 cap = 1;
   u64 data[1] = {0};
   (void)stack_array_grow(data, data, cap, &cap, 0);
}

static void test_node_seen_insert_null_child(void) {
   node_seen_t seen = {0};
   node_seen_insert(&seen, NULL);
}

static void test_node_seen_contains_null_child(void) {
   node_seen_t seen = {0};
   (void)node_seen_contains(&seen, NULL);
}

static void test_node_seen_contains_partial_zero_state_child(void) {
   node_seen_t seen = {.len = 1};
   node_t node = {0};
   (void)node_seen_contains(&seen, &node);
}

static void test_node_seen_contains_tiny_table_child(void) {
   node_t *slots[1] = {0};
   node_seen_t seen = {.slots = slots, .cap = 1};
   node_t node = {0};
   (void)node_seen_contains(&seen, &node);
}

static void test_node_seen_contains_full_table_child(void) {
   node_t nodes[NODE_SEEN_INLINE_CAP * 2] = {0};
   node_t *slots[NODE_SEEN_INLINE_CAP * 2];
   for (u64 i = 0; i < NODE_SEEN_INLINE_CAP * 2; i++) {
      slots[i] = &nodes[i];
   }
   node_t missing = {0};
   node_seen_t seen = {
       .slots = slots,
       .len = 0,
       .cap = NODE_SEEN_INLINE_CAP * 2,
   };
   (void)node_seen_contains(&seen, &missing);
}

static void test_node_seen_contains_external_inline_capacity_child(void) {
   node_t *slots[NODE_SEEN_INLINE_CAP] = {0};
   node_seen_t seen = {.slots = slots, .cap = NODE_SEEN_INLINE_CAP};
   node_t node = {0};
   (void)node_seen_contains(&seen, &node);
}

static void test_node_seen_contains_inline_wrong_capacity_child(void) {
   node_seen_t seen = {0};
   seen.slots = seen.inline_slots;
   seen.cap = NODE_SEEN_INLINE_CAP * 2;
   node_t node = {0};
   (void)node_seen_contains(&seen, &node);
}

static void test_node_seen_dispose_partial_zero_state_child(void) {
   node_seen_t seen = {.cap = NODE_SEEN_INLINE_CAP};
   node_seen_dispose(&seen);
}

static void test_node_seen_dispose_tiny_table_child(void) {
   node_t *slots[1] = {0};
   node_seen_t seen = {.slots = slots, .cap = 1};
   node_seen_dispose(&seen);
}

static void test_node_seen_dispose_external_inline_capacity_child(void) {
   node_t *slots[NODE_SEEN_INLINE_CAP] = {0};
   node_seen_t seen = {.slots = slots, .cap = NODE_SEEN_INLINE_CAP};
   node_seen_dispose(&seen);
}

static void test_node_seen_dispose_inline_wrong_capacity_child(void) {
   node_seen_t seen = {0};
   seen.slots = seen.inline_slots;
   seen.cap = NODE_SEEN_INLINE_CAP * 2;
   node_seen_dispose(&seen);
}

static void test_node_seen_duplicate_insert_child(void) {
   node_seen_t seen = {0};
   node_t node = {0};
   node_seen_insert(&seen, &node);
   node_seen_insert(&seen, &node);
}

static void test_node_seen_insert_tiny_table_child(void) {
   node_t *slots[1] = {0};
   node_seen_t seen = {.slots = slots, .cap = 1};
   node_t node = {0};
   node_seen_insert(&seen, &node);
}

static void test_node_seen_insert_raw_full_table_child(void) {
   node_t nodes[NODE_SEEN_INLINE_CAP * 2] = {0};
   node_t *slots[NODE_SEEN_INLINE_CAP * 2];
   for (u64 i = 0; i < NODE_SEEN_INLINE_CAP * 2; i++) {
      slots[i] = &nodes[i];
   }
   node_t missing = {0};
   node_seen_t seen = {
       .slots = slots,
       .len = 0,
       .cap = NODE_SEEN_INLINE_CAP * 2,
   };
   (void)node_seen_insert_raw(&seen, &missing);
}

static void assert_node_seen_raw_duplicate_contract(void) {
   node_seen_t seen = {0};
   node_t first = {0};
   node_t second = {0};

   node_seen_init(&seen);
   invariant(node_seen_insert_raw(&seen, &first));
   assert_equal(1, seen.len);
   invariant(node_seen_contains(&seen, &first));
   invariant(!node_seen_insert_raw(&seen, &first));
   assert_equal(1, seen.len);
   invariant(node_seen_insert_raw(&seen, &second));
   assert_equal(2, seen.len);
   invariant(node_seen_contains(&seen, &first));
   invariant(node_seen_contains(&seen, &second));
   node_seen_dispose(&seen);

   node_seen_dispose(&seen);
   invariant(seen.slots == NULL);
   assert_equal(0, seen.len);
   assert_equal(0, seen.cap);
}

static void TestCheckedAllocation(void) {
   printf("Checked allocation:\n");

   u64 max_array_count = bst_array_max_count(sizeof(u64));
   assert_equal((u64)(SIZE_MAX / sizeof(u64)), max_array_count);
   assert_equal((size_t)(4 * sizeof(u64)), bst_array_bytes(4, sizeof(u64)));
   invariant(bst_name_is_printable_token("Zipf/Experimental"));
   invariant(!bst_name_is_printable_token(NULL));
   invariant(!bst_name_is_printable_token(""));
   invariant(!bst_name_is_printable_token("two words"));
   invariant(!bst_name_is_printable_token("line\nbreak"));
   invariant(!bst_name_is_printable_token("bell\abreak"));
   invariant(!bst_name_is_printable_token("delete\177break"));
   invariant(!bst_name_is_printable_token("high\200byte"));
   invariant(bst_name_is_path_component("AVLTopDown"));
   invariant(!bst_name_is_path_component(NULL));
   invariant(!bst_name_is_path_component(""));
   invariant(!bst_name_is_path_component("."));
   invariant(!bst_name_is_path_component(".."));
   invariant(!bst_name_is_path_component("nested/name"));
   invariant(!bst_name_is_path_component("nested\\name"));
   invariant(!bst_name_is_path_component("two words"));
   invariant(!bst_name_is_path_component("line\nbreak"));
   invariant(!bst_name_is_path_component("bell\abreak"));
   invariant(!bst_name_is_path_component("delete\177break"));
   invariant(!bst_name_is_path_component("high\200byte"));

   unsigned char *bytes = (unsigned char *)bst_malloc(1);
   bytes[0] = 0xa5;
   bytes = (unsigned char *)bst_realloc(bytes, 32);
   assert_int_equal(0xa5, bytes[0]);
   free(bytes);

   u64 *zeros = (u64 *)bst_calloc_array(4, sizeof(zeros[0]));
   for (int i = 0; i < 4; i++) {
      assert_equal(0, zeros[i]);
   }
   free(zeros);

   u64 *values = (u64 *)bst_malloc_array(4, sizeof(values[0]));
   for (int i = 0; i < 4; i++) {
      values[i] = (u64)i + 1;
   }
   values = (u64 *)bst_realloc_array(values, 8, sizeof(values[0]));
   for (int i = 0; i < 4; i++) {
      assert_equal((u64)i + 1, values[i]);
   }
   free(values);

   u64 source[4] = {1, 2, 3, 4};
   u64 copied[4] = {0};
   bst_array_copy(copied, source, 4, sizeof(copied[0]));
   for (int i = 0; i < 4; i++) {
      assert_equal(source[i], copied[i]);
   }
   bst_array_move(&copied[1], copied, 3, sizeof(copied[0]));
   assert_equal(1, copied[0]);
   assert_equal(1, copied[1]);
   assert_equal(2, copied[2]);
   assert_equal(3, copied[3]);
   bst_array_zero(copied, 4, sizeof(copied[0]));
   for (int i = 0; i < 4; i++) {
      assert_equal(0, copied[i]);
   }
   bst_array_copy(NULL, NULL, 0, sizeof(u64));
   bst_array_move(NULL, NULL, 0, sizeof(u64));
   bst_array_zero(NULL, 0, sizeof(u64));
   bst_array_copy(NULL, NULL, 0, 0);
   bst_array_move(NULL, NULL, 0, 0);
   bst_array_zero(NULL, 0, 0);

   assert_equal(8, bst_array_next_cap(0, 1, 8, sizeof(u64)));
   assert_equal(16, bst_array_next_cap(8, 9, 8, sizeof(u64)));
   assert_equal(max_array_count,
                bst_array_next_cap(max_array_count / 2 + 1, max_array_count, 8,
                                   sizeof(u64)));
   assert_equal(1, bst_full_binary_tree_max_leaves_for_nodes(1));
   assert_equal(1, bst_full_binary_tree_max_leaves_for_nodes(2));
   assert_equal(2, bst_full_binary_tree_max_leaves_for_nodes(3));
   assert_equal(2, bst_full_binary_tree_max_leaves_for_nodes(4));
   assert_equal(4, bst_full_binary_tree_max_leaves_for_nodes(7));
   assert_equal(4, bst_full_binary_tree_max_leaves_for_nodes(8));
   assert_equal(UINT64_MAX / 2 + 1,
                bst_full_binary_tree_max_leaves_for_nodes(UINT64_MAX));
   assert_equal(1, bst_full_binary_tree_nodes_from_leaves(1));
   assert_equal(3, bst_full_binary_tree_nodes_from_leaves(2));
   assert_equal(9, bst_full_binary_tree_nodes_from_leaves(5));
   assert_equal(UINT64_MAX,
                bst_full_binary_tree_nodes_from_leaves(UINT64_MAX / 2 + 1));

   u64 inline_stack[2] = {11, 22};
   u64 stack_cap = 2;
   u64 *grown_stack =
       (u64 *)stack_array_grow(inline_stack, inline_stack, stack_cap,
                               &stack_cap, sizeof(inline_stack[0]));
   invariant(grown_stack != inline_stack);
   assert_equal(4, stack_cap);
   assert_equal(11, grown_stack[0]);
   assert_equal(22, grown_stack[1]);
   grown_stack[2] = 33;
   grown_stack[3] = 44;
   grown_stack = (u64 *)stack_array_grow(grown_stack, inline_stack, stack_cap,
                                         &stack_cap, sizeof(grown_stack[0]));
   invariant(grown_stack != inline_stack);
   assert_equal(8, stack_cap);
   assert_equal(11, grown_stack[0]);
   assert_equal(22, grown_stack[1]);
   assert_equal(33, grown_stack[2]);
   assert_equal(44, grown_stack[3]);
   free(grown_stack);

   node_seen_t seen = {0};
   node_t seen_nodes[NODE_SEEN_INLINE_CAP + 9] = {0};
   u64 seen_node_count = sizeof(seen_nodes) / sizeof(seen_nodes[0]);
   invariant(!node_seen_contains(&seen, &seen_nodes[0]));
   for (u64 i = 0; i < seen_node_count; i++) {
      node_seen_insert(&seen, &seen_nodes[i]);
      assert_equal(i + 1, seen.len);
      for (u64 j = 0; j <= i; j++) {
         invariant(node_seen_contains(&seen, &seen_nodes[j]));
      }
   }
   invariant(seen.cap > NODE_SEEN_INLINE_CAP);
   invariant(seen.slots != seen.inline_slots);
   for (u64 i = 0; i < NODE_SEEN_INLINE_CAP; i++) {
      invariant(seen.inline_slots[i] == NULL);
   }
   node_seen_dispose(&seen);
   invariant(seen.slots == NULL);
   assert_equal(0, seen.len);
   assert_equal(0, seen.cap);
   invariant(!node_seen_contains(&seen, &seen_nodes[0]));
   node_seen_insert(&seen, &seen_nodes[0]);
   invariant(seen.slots == seen.inline_slots);
   invariant(node_seen_contains(&seen, &seen_nodes[0]));
   node_seen_dispose(&seen);
   printf("  inline node set helper contracts OK\n");
   assert_node_seen_raw_duplicate_contract();

   assert_aborts(test_bst_malloc_zero_child);
   assert_aborts(test_bst_calloc_zero_count_child);
   assert_aborts(test_bst_calloc_zero_size_child);
   assert_aborts(test_bst_calloc_overflow_child);
   assert_aborts(test_bst_calloc_array_zero_count_child);
   assert_aborts(test_bst_calloc_array_zero_size_child);
   assert_aborts(test_bst_calloc_array_overflow_child);
   assert_aborts(test_bst_array_max_count_zero_size_child);
   assert_aborts(test_bst_array_bytes_zero_count_child);
   assert_aborts(test_bst_array_bytes_zero_size_child);
   assert_aborts(test_bst_array_bytes_overflow_child);
   assert_aborts(test_bst_array_copy_null_dst_child);
   assert_aborts(test_bst_array_copy_null_src_child);
   assert_aborts(test_bst_array_copy_zero_size_child);
   assert_aborts(test_bst_array_copy_same_range_child);
   assert_aborts(test_bst_array_copy_overlap_forward_child);
   assert_aborts(test_bst_array_copy_overlap_backward_child);
   assert_aborts(test_bst_array_copy_dst_range_overflow_child);
   assert_aborts(test_bst_array_copy_src_range_overflow_child);
   assert_aborts(test_bst_array_move_null_dst_child);
   assert_aborts(test_bst_array_move_null_src_child);
   assert_aborts(test_bst_array_move_zero_size_child);
   assert_aborts(test_bst_array_move_dst_range_overflow_child);
   assert_aborts(test_bst_array_move_src_range_overflow_child);
   assert_aborts(test_bst_array_zero_null_child);
   assert_aborts(test_bst_array_zero_zero_size_child);
   assert_aborts(test_bst_array_zero_range_overflow_child);
   assert_aborts(test_bst_array_next_cap_filled_child);
   assert_aborts(test_bst_array_next_cap_zero_initial_child);
   assert_aborts(test_bst_array_next_cap_zero_size_child);
   assert_aborts(test_bst_array_next_cap_overflow_child);
   assert_aborts(test_bst_full_binary_tree_max_leaves_zero_nodes_child);
   assert_aborts(test_bst_full_binary_tree_nodes_zero_leaves_child);
   assert_aborts(test_bst_full_binary_tree_nodes_overflow_child);
   assert_aborts(test_bst_malloc_array_zero_count_child);
   assert_aborts(test_bst_malloc_array_zero_size_child);
   assert_aborts(test_bst_malloc_array_overflow_child);
   assert_aborts(test_bst_realloc_null_child);
   assert_aborts(test_bst_realloc_zero_size_child);
   assert_aborts(test_bst_realloc_array_zero_count_child);
   assert_aborts(test_bst_realloc_array_zero_size_child);
   assert_aborts(test_bst_realloc_array_overflow_child);
   assert_aborts(test_stack_array_grow_null_data_child);
   assert_aborts(test_stack_array_grow_null_inline_child);
   assert_aborts(test_stack_array_grow_null_cap_child);
   assert_aborts(test_stack_array_grow_not_full_child);
   assert_aborts(test_stack_array_grow_zero_cap_child);
   assert_aborts(test_stack_array_grow_zero_size_child);
   assert_aborts(test_node_seen_insert_null_child);
   assert_aborts(test_node_seen_contains_null_child);
   assert_aborts(test_node_seen_contains_partial_zero_state_child);
   assert_aborts(test_node_seen_contains_tiny_table_child);
   assert_aborts(test_node_seen_contains_full_table_child);
   assert_aborts(test_node_seen_contains_external_inline_capacity_child);
   assert_aborts(test_node_seen_contains_inline_wrong_capacity_child);
   assert_aborts(test_node_seen_dispose_partial_zero_state_child);
   assert_aborts(test_node_seen_dispose_tiny_table_child);
   assert_aborts(test_node_seen_dispose_external_inline_capacity_child);
   assert_aborts(test_node_seen_dispose_inline_wrong_capacity_child);
   assert_aborts(test_node_seen_duplicate_insert_child);
   assert_aborts(test_node_seen_insert_tiny_table_child);
   assert_aborts(test_node_seen_insert_raw_full_table_child);

   printf("  allocation helper contracts OK\n");
}

static void assert_distribution_registry_slice(const dist_spec_t *const *specs,
                                               int count) {
   invariant(specs != NULL);
   invariant(count > 0);
   for (int i = 0; i < count; i++) {
      const dist_spec_t *spec = specs[i];
      dist_spec_require(spec);
      invariant(dist_find(spec->name) == spec);
      for (int j = i + 1; j < count; j++) {
         invariant(specs[j] != NULL);
         invariant(specs[j]->name != NULL);
         invariant(spec != specs[j]);
         invariant(strcmp(spec->name, specs[j]->name) != 0);
      }
   }
}

static void test_tree_registry_find_or_exit_missing_child(void) {
   (void)tree_registry_find_or_exit("__missing_tree__");
}

static void test_tree_registry_find_or_exit_null_child(void) {
   (void)tree_registry_find_or_exit(NULL);
}

static void test_tree_registry_find_or_exit_empty_child(void) {
   (void)tree_registry_find_or_exit("");
}

static void test_tree_registry_find_or_exit_bad_component_child(void) {
   (void)tree_registry_find_or_exit("Bad/Name");
}

static void test_tree_registry_find_or_exit_high_byte_child(void) {
   (void)tree_registry_find_or_exit("Bad\200Name");
}

static void test_tree_registry_selection_null_child(void) {
   tree_registry_selection_init(NULL, NULL);
}

static void test_tree_registry_fprint_names_null_file_child(void) {
   tree_registry_fprint_names(NULL, "trees");
}

static void test_tree_registry_fprint_names_null_label_child(void) {
   tree_registry_fprint_names(stdout, NULL);
}

static void test_distribution_find_or_exit_missing_child(void) {
   (void)dist_find_or_exit("__missing_distribution__");
}

static void test_distribution_find_or_exit_null_child(void) {
   (void)dist_find_or_exit(NULL);
}

static void test_distribution_find_or_exit_empty_child(void) {
   (void)dist_find_or_exit("");
}

static void test_distribution_find_or_exit_bad_token_child(void) {
   (void)dist_find_or_exit("Bad Name");
}

static void test_distribution_find_or_exit_high_byte_child(void) {
   (void)dist_find_or_exit("Bad\200Name");
}

static void test_dist_benchmark_selection_null_child(void) {
   dist_benchmark_selection_init(NULL, NULL);
}

static void test_dist_fprint_names_null_file_child(void) {
   dist_fprint_names(NULL, "distributions", NULL, 0);
}

static void test_dist_fprint_names_null_label_child(void) {
   dist_fprint_names(stdout, NULL, NULL, 0);
}

static void test_dist_fprint_names_negative_count_child(void) {
   dist_fprint_names(stdout, "distributions", NULL, -1);
}

static void test_balancer_registry_find_or_exit_missing_child(void) {
   (void)balancer_registry_find_or_exit("__missing_balancer__");
}

static void test_balancer_registry_find_or_exit_null_child(void) {
   (void)balancer_registry_find_or_exit(NULL);
}

static void test_balancer_registry_find_or_exit_empty_child(void) {
   (void)balancer_registry_find_or_exit("");
}

static void test_balancer_registry_find_or_exit_bad_component_child(void) {
   (void)balancer_registry_find_or_exit("Bad/Name");
}

static void test_balancer_registry_find_or_exit_high_byte_child(void) {
   (void)balancer_registry_find_or_exit("Bad\200Name");
}

static void test_balancer_registry_fprint_names_null_file_child(void) {
   balancer_registry_fprint_names(NULL, "balancers");
}

static void test_balancer_registry_fprint_names_null_label_child(void) {
   balancer_registry_fprint_names(stdout, NULL);
}

static balancer_t test_balancer_with_name(const char *name) {
   return (balancer_t){
       .name = name,
       .restore = balancer_median.restore,
       .verify = balancer_median.verify,
   };
}

static void test_balancer_require_empty_name_child(void) {
   balancer_t balancer = test_balancer_with_name("");
   balancer_require(&balancer);
}

static void test_balancer_require_dot_name_child(void) {
   balancer_t balancer = test_balancer_with_name(".");
   balancer_require(&balancer);
}

static void test_balancer_require_dotdot_name_child(void) {
   balancer_t balancer = test_balancer_with_name("..");
   balancer_require(&balancer);
}

static void test_balancer_require_slash_name_child(void) {
   balancer_t balancer = test_balancer_with_name("Bad/Name");
   balancer_require(&balancer);
}

static void test_balancer_require_space_name_child(void) {
   balancer_t balancer = test_balancer_with_name("Bad Name");
   balancer_require(&balancer);
}

static void
assert_registry_lookup_exits_2_with_stderr(void (*fn)(void),
                                           const char *expected_stderr) {
   assert_void_child_exits_2_with_stderr(fn, expected_stderr);
}

static void test_balancer_run_seed_empty_domain_child(void) {
   (void)balancer_run_seed("", dist_all[0], 1, 1);
}

static void test_balancer_run_seed_null_distribution_child(void) {
   (void)balancer_run_seed("test", NULL, 1, 1);
}

static void test_balancer_run_seed_unregistered_distribution_child(void) {
   const dist_spec_t dist_spec = {"Uniform", uniform_new};
   (void)balancer_run_seed("test", &dist_spec, 1, 1);
}

static void test_balancer_run_seed_zero_position_child(void) {
   (void)balancer_run_seed("test", dist_all[0], 0, 1);
}

static void test_balancer_run_seed_zero_iteration_child(void) {
   (void)balancer_run_seed("test", dist_all[0], 1, 0);
}

static void test_balancer_run_sample_schedule_too_large_child(void) {
   invariant(arena_max_capacity() < UINT64_MAX);
   (void)balancer_run_sample_schedule(arena_max_capacity() + 1, 1);
}

static void test_balancer_run_distribution_selection_null_child(void) {
   balancer_run_distribution_selection_init(NULL, NULL);
}

static void test_balancer_run_distribution_selection_unregistered_child(void) {
   const dist_spec_t dist_spec = {"Uniform", uniform_new};
   balancer_run_distribution_selection_t selection;
   balancer_run_distribution_selection_init(&selection, &dist_spec);
}

static void test_balancer_run_distribution_selection_by_name_null_child(void) {
   balancer_run_distribution_selection_init_by_name(NULL, NULL);
}

static void test_balancer_run_grow_splay_to_null_child(void) {
   balancer_run_grow_splay_to(NULL, 1);
}

static void test_balancer_run_grow_splay_to_shrink_child(void) {
   splay_t instance;
   splay_init(&instance);
   balancer_run_grow_splay_to(&instance, 2);
   balancer_run_grow_splay_to(&instance, 1);
}

static void test_balancer_run_grow_splay_to_root_size_mismatch_child(void) {
   node_t root = {0};
   splay_t instance = {
       .tree =
           {
               .root = &root,
               .size = 0,
           },
   };
   balancer_run_grow_splay_to(&instance, 1);
}

static void test_balancer_run_randomize_empty_tree_child(void) {
   tree_t tree = {0};
   (void)balancer_run_randomize_tree(tree, dist_all[0], "test", 1);
}

static void test_balancer_run_randomize_root_size_mismatch_child(void) {
   tree_t tree = {.size = 1};
   (void)balancer_run_randomize_tree(tree, dist_all[0], "test", 1);
}

static void test_balancer_run_randomize_unregistered_distribution_child(void) {
   tree_t tree = right_chain_of_size(1);
   const dist_spec_t dist_spec = {"Uniform", uniform_new};
   tree = balancer_run_randomize_tree(tree, &dist_spec, "test", 1);
   free_nodes(tree.root);
   bst_unreachable();
}

static void test_balancer_run_next_iteration_overflow_child(void) {
   (void)balancer_run_next_iteration(UINT64_MAX);
}

static void test_balancer_run_restore_null_balancer_child(void) {
   tree_t tree = {0};
   (void)balancer_run_restore_tree(NULL, tree);
   bst_unreachable();
}

static void test_balancer_run_restore_input_root_size_mismatch_child(void) {
   node_t root = {0};
   tree_t tree = {.root = &root, .size = 0};
   (void)balancer_run_restore_tree(&balancer_median, tree);
}

static tree_t test_balancer_restore_bad_empty_tree(tree_t tree) {
   (void)tree;
   static node_t root = {0};
   return (tree_t){.root = &root, .size = 0};
}

static void test_balancer_run_restore_output_root_size_mismatch_child(void) {
   const balancer_t balancer = {
       .name = "BadEmptyRestore",
       .restore = test_balancer_restore_bad_empty_tree,
       .verify = balancer_median.verify,
   };
   tree_t tree = {0};
   (void)balancer_run_restore_tree(&balancer, tree);
}

static void test_balancer_noop_verify(tree_t tree) {
   (void)tree;
}

static void test_balancer_verify_tree_root_size_mismatch_child(void) {
   const balancer_t balancer = {
       .name = "NoopVerifier",
       .restore = balancer_median.restore,
       .verify = test_balancer_noop_verify,
   };
   node_t root = {0};
   balancer_verify_tree(&balancer, (tree_t){.root = &root, .size = 0});
}

static void test_balancer_mutating_verify(tree_t tree) {
   tree_sized_handle_require(tree);
   invariant(tree.root != NULL);
   tree.root->s = tree.size;
}

static void test_balancer_verify_tree_mutating_child(void) {
   const balancer_t balancer = {
       .name = "MutatingVerifier",
       .restore = balancer_median.restore,
       .verify = test_balancer_mutating_verify,
   };
   node_t root = {0};
   balancer_verify_tree(&balancer, (tree_t){.root = &root, .size = 1});
}

static void test_balancer_direct_restore_root_size_mismatch_child(void) {
   node_t root = {0};
   (void)balancer_median.restore((tree_t){.root = &root, .size = 0});
}

static void test_balancer_direct_verify_root_size_mismatch_child(void) {
   tree_t tree = {.size = 1};
   balancer_median.verify(tree);
}

static void test_balancer_dsw_restore_root_size_mismatch_child(void) {
   tree_t tree = {.size = 1};
   (void)balancer_dsw.restore(tree);
}

static void test_balancer_dsw_verify_root_size_mismatch_child(void) {
   node_t root = {0};
   balancer_dsw.verify((tree_t){.root = &root, .size = 0});
}

#ifdef DEBUG
static tree_t test_balancer_restore_identity(tree_t tree) {
   return tree;
}

static void test_balancer_verify_rejects_empty(tree_t tree) {
   invariant(tree.size > 0);
}

static void test_balancer_run_restore_verifies_child(void) {
   const balancer_t balancer = {
       .name = "Verifier",
       .restore = test_balancer_restore_identity,
       .verify = test_balancer_verify_rejects_empty,
   };
   tree_t tree = {0};
   (void)balancer_run_restore_tree(&balancer, tree);
}
#endif

static void assert_balancer_run_outputs_make_exits_2(
    const char *output_dir, const char *kind, const char *expected_stderr) {
   invariant(expected_stderr != NULL);

   char stderr_path[] = "/tmp/bst-balancer-run-stderr-XXXXXX";
   int stderr_fd = mkstemp(stderr_path);
   invariant(stderr_fd >= 0);
   invariant(close(stderr_fd) == 0);

   fflush(NULL);
   pid_t pid = fork();
   invariant(pid >= 0);
   if (pid == 0) {
      FILE *err = freopen(stderr_path, "w", stderr);
      invariant(err != NULL);
      balancer_run_output_t *outputs = balancer_run_outputs_alloc();
      (void)balancer_run_outputs_make(outputs, balancer_registry_count, NULL,
                                      output_dir, kind);
      _exit(0);
   }
   assert_child_exited_2(pid);

   char *stderr_text = test_read_text_file(stderr_path);
   test_require_substring(stderr_text, expected_stderr);
   free(stderr_text);
   invariant(remove(stderr_path) == 0);
}

typedef void (*balancer_run_outputs_child_fn)(void);

static void
assert_balancer_run_outputs_child_exits_2(balancer_run_outputs_child_fn fn) {
   assert_void_child_exits_2(fn);
}

static void assert_balancer_run_outputs_child_exits_2_with_stderr(
    balancer_run_outputs_child_fn fn, const char *expected_stderr) {
   assert_void_child_exits_2_with_stderr(fn, expected_stderr);
}

static const balancer_t *test_registered_balancer(int index) {
   invariant(index >= 0);
   invariant(index < balancer_registry_count);
   const balancer_t *balancer = balancer_registry_all[index];
   balancer_require(balancer);
   return balancer;
}

static void balancer_run_prepare_null_outputs_child(void) {
   balancer_run_outputs_prepare_dirs(NULL, 1);
}

static void balancer_run_prepare_null_balancer_child(void) {
   balancer_run_output_t outputs[1] = {{
       .file = {.path = "/tmp/bst-balancer-null-balancer"},
       .dir = "/tmp",
   }};
   balancer_run_outputs_prepare_dirs(outputs, 1);
}

static void balancer_run_prepare_unregistered_balancer_child(void) {
   const balancer_t fake_balancer = {
       .name = "Unregistered",
       .restore = balancer_median.restore,
       .verify = balancer_median.verify,
   };
   balancer_run_output_t outputs[1] = {{
       .balancer = &fake_balancer,
       .file = {.path = "/tmp/bst-balancer-unregistered-balancer"},
       .dir = "/tmp",
   }};
   balancer_run_outputs_prepare_dirs(outputs, 1);
}

static void balancer_run_make_unregistered_selected_child(void) {
   const balancer_t fake_balancer = {
       .name = "Unregistered",
       .restore = balancer_median.restore,
       .verify = balancer_median.verify,
   };
   balancer_run_output_t *outputs = balancer_run_outputs_alloc();
   (void)balancer_run_outputs_make(outputs, balancer_registry_count,
                                   &fake_balancer, "/tmp/bst-registry",
                                   "benchmarks");
}

static void balancer_run_make_bad_selected_name_child(void) {
   const balancer_t fake_balancer = {
       .name = "Bad/Name",
       .restore = balancer_median.restore,
       .verify = balancer_median.verify,
   };
   balancer_run_output_t *outputs = balancer_run_outputs_alloc();
   (void)balancer_run_outputs_make(outputs, balancer_registry_count,
                                   &fake_balancer, "/tmp/bst-registry",
                                   "benchmarks");
}

static void balancer_run_make_high_byte_selected_name_child(void) {
   const balancer_t fake_balancer = {
       .name = "Bad\200Name",
       .restore = balancer_median.restore,
       .verify = balancer_median.verify,
   };
   balancer_run_output_t *outputs = balancer_run_outputs_alloc();
   (void)balancer_run_outputs_make(outputs, balancer_registry_count,
                                   &fake_balancer, "/tmp/bst-registry",
                                   "benchmarks");
}

static void balancer_run_make_null_outputs_child(void) {
   (void)balancer_run_outputs_make(NULL, balancer_registry_count, NULL,
                                   "/tmp/bst-registry", "benchmarks");
}

static void balancer_run_make_negative_capacity_child(void) {
   balancer_run_output_t *outputs = balancer_run_outputs_alloc();
   (void)balancer_run_outputs_make(outputs, -1, NULL, "/tmp/bst-registry",
                                   "benchmarks");
}

static void balancer_run_make_selected_zero_capacity_child(void) {
   balancer_run_output_t outputs[1] = {0};
   (void)balancer_run_outputs_make(outputs, 0, test_registered_balancer(0),
                                   "/tmp/bst-registry", "benchmarks");
}

static void balancer_run_make_small_capacity_child(void) {
   balancer_run_output_t outputs[1] = {0};
   (void)balancer_run_outputs_make(outputs, 1, NULL, "/tmp/bst-registry",
                                   "benchmarks");
}

static void balancer_run_prepare_empty_outputs_child(void) {
   balancer_run_output_t *outputs = balancer_run_outputs_alloc();
   balancer_run_outputs_prepare_dirs(outputs, 0);
}

static void balancer_run_prepare_too_many_outputs_child(void) {
   balancer_run_output_t *outputs = balancer_run_outputs_alloc();
   balancer_run_outputs_prepare_dirs(outputs, balancer_registry_count + 1);
}

static void balancer_run_prepare_null_dir_child(void) {
   balancer_run_output_t outputs[1] = {{
       .balancer = test_registered_balancer(0),
       .file = {.path = "/tmp/bst-balancer-null-dir"},
   }};
   balancer_run_outputs_prepare_dirs(outputs, 1);
}

static void balancer_run_prepare_empty_dir_child(void) {
   balancer_run_output_t outputs[1] = {{
       .balancer = test_registered_balancer(0),
       .file = {.path = "/tmp/bst-balancer-empty-dir"},
       .dir = "",
   }};
   balancer_run_outputs_prepare_dirs(outputs, 1);
}

static char test_balancer_run_prepare_valid_dir[512];
static char test_balancer_run_prepare_valid_path[512];
static char test_balancer_run_prepare_wrong_root[512];
static char test_balancer_run_prepare_wrong_missing_dir[512];
static char test_balancer_run_prepare_wrong_path[512];

static void balancer_run_prepare_late_empty_dir_child(void) {
   invariant(test_balancer_run_prepare_valid_dir[0] != '\0');
   invariant(test_balancer_run_prepare_valid_path[0] != '\0');
   balancer_run_output_t outputs[2] = {
       {
           .balancer = test_registered_balancer(0),
           .file = {.path = test_balancer_run_prepare_valid_path},
           .dir = test_balancer_run_prepare_valid_dir,
       },
       {
           .balancer = test_registered_balancer(1),
           .file = {.path = "/tmp/bst-balancer-late-empty-dir"},
           .dir = "",
       },
   };
   balancer_run_outputs_prepare_dirs(outputs, 2);
}

static void balancer_run_prepare_wrong_parent_dir_child(void) {
   invariant(test_balancer_run_prepare_wrong_root[0] != '\0');
   invariant(test_balancer_run_prepare_wrong_path[0] != '\0');
   balancer_run_output_t outputs[1] = {{
       .balancer = test_registered_balancer(0),
       .file = {.path = test_balancer_run_prepare_wrong_path},
       .dir = test_balancer_run_prepare_wrong_root,
   }};
   balancer_run_outputs_prepare_dirs(outputs, 1);
}

static void balancer_run_prepare_duplicate_balancer_child(void) {
   const balancer_t *balancer = test_registered_balancer(0);
   balancer_run_output_t outputs[2] = {
       {
           .balancer = balancer,
           .file = {.path = "/tmp/bst-balancer-duplicate-balancer-left"},
           .dir = "/tmp",
       },
       {
           .balancer = balancer,
           .file = {.path = "/tmp/bst-balancer-duplicate-balancer-right"},
           .dir = "/tmp",
       },
   };
   balancer_run_outputs_prepare_dirs(outputs, 2);
}

static void balancer_run_prepare_duplicate_path_child(void) {
   balancer_run_output_t outputs[2] = {
       {
           .balancer = test_registered_balancer(0),
           .file = {.path = "/tmp/bst-balancer-duplicate-output"},
           .dir = "/tmp",
       },
       {
           .balancer = test_registered_balancer(1),
           .file = {.path = "/tmp/bst-balancer-duplicate-output"},
           .dir = "/tmp",
       },
   };
   balancer_run_outputs_prepare_dirs(outputs, 2);
}

static void balancer_run_prepare_late_invalid_duplicate_path_child(void) {
   balancer_run_output_t outputs[2] = {
       {
           .balancer = test_registered_balancer(0),
           .file = {.path = "/tmp/bst-balancer-invalid-duplicate-output"},
           .dir = "/tmp",
       },
       {
           .balancer = test_registered_balancer(1),
           .file = {.path = "/tmp/bst-balancer-invalid-duplicate-output"},
           .dir = "",
       },
   };
   balancer_run_outputs_prepare_dirs(outputs, 2);
}

static void balancer_run_publish_null_outputs_child(void) {
   balancer_run_outputs_publish(NULL, 1);
}

static void balancer_run_publish_empty_outputs_child(void) {
   balancer_run_output_t *outputs = balancer_run_outputs_alloc();
   balancer_run_outputs_publish(outputs, 0);
}

static void balancer_run_publish_too_many_outputs_child(void) {
   balancer_run_output_t *outputs = balancer_run_outputs_alloc();
   balancer_run_outputs_publish(outputs, balancer_registry_count + 1);
}

static void balancer_run_dispose_negative_count_child(void) {
   balancer_run_output_t *outputs = balancer_run_outputs_alloc();
   balancer_run_outputs_dispose(outputs, -1);
}

static void balancer_run_dispose_null_negative_count_child(void) {
   balancer_run_outputs_dispose(NULL, -1);
}

static void balancer_run_dispose_null_positive_count_child(void) {
   balancer_run_outputs_dispose(NULL, 1);
}

static void balancer_run_dispose_too_many_outputs_child(void) {
   balancer_run_output_t *outputs = balancer_run_outputs_alloc();
   balancer_run_outputs_dispose(outputs, balancer_registry_count + 1);
}

static void assert_balancer_run_publish_preflights_all_outputs(void) {
   if (balancer_registry_count < 2) {
      return;
   }

   char output_dir[] = "/tmp/bst-balancer-publish-preflight-XXXXXX";
   char *root = mkdtemp(output_dir);
   invariant(root != NULL);

   balancer_run_output_t *outputs = balancer_run_outputs_alloc();
   int output_count = balancer_run_outputs_make(
       outputs, balancer_registry_count, NULL, root, "benchmarks");
   invariant(output_count >= 2);
   balancer_run_outputs_prepare_dirs(outputs, output_count);

   FILE *file = staged_file_open_or_exit(&outputs[0].file, "w");
   file_puts_or_exit("finished\n", file, outputs[0].file.tmp_path);
   staged_file_finish_or_exit(&outputs[0].file, file);

   file = staged_file_open_or_exit(&outputs[1].file, "w");
   file_puts_or_exit("unfinished\n", file, outputs[1].file.tmp_path);
   fclose_or_exit(file, outputs[1].file.tmp_path);

   fflush(NULL);
   pid_t publish_pid = fork();
   invariant(publish_pid >= 0);
   if (publish_pid == 0) {
      FILE *err = freopen("/dev/null", "w", stderr);
      invariant(err != NULL);
      balancer_run_outputs_publish(outputs, output_count);
      _exit(0);
   }
   assert_child_exited_2(publish_pid);

   for (int i = 0; i < output_count; i++) {
      assert_path_absent(outputs[i].file.path);
   }

   balancer_run_outputs_dispose(outputs, output_count);

   char dir[512];
   path_format(dir, sizeof(dir), "%s/balancers/benchmarks", root);
   invariant(rmdir(dir) == 0);
   path_format(dir, sizeof(dir), "%s/balancers", root);
   invariant(rmdir(dir) == 0);
   invariant(rmdir(root) == 0);
}

static void assert_tree_registry_vtable_contract(const list_vtable_t *vt) {
   list_vtable_require(vt);

   list_t instance = list_new(vt);
   list_t ref = list_new(&reference_vtable);

   list_insert(&instance, 0, 10);
   list_insert(&ref, 0, 10);
   list_insert(&instance, 1, 30);
   list_insert(&ref, 1, 30);
   list_insert(&instance, 1, 20);
   list_insert(&ref, 1, 20);
   assert_list(&ref, &instance);

   tree_t *tree = list_tree(&instance);
   invariant(tree != NULL);
   assert_equal(3, tree->size);

   list_update(&instance, 1, 22);
   list_update(&ref, 1, 22);
   assert_list(&ref, &instance);

   list_t clone = list_clone(&instance);
   assert_list(&ref, &clone);

   list_t left, right;
   list_split(&instance, 2, &left, &right);
   list_t ref_left, ref_right;
   list_split(&ref, 2, &ref_left, &ref_right);
   assert_list(&ref_left, &left);
   assert_list(&ref_right, &right);

   list_t joined = list_join(&left, &right);
   assert_list(&ref, &joined);

   assert_equal(list_delete(&ref, 1), list_delete(&joined, 1));
   assert_list(&ref, &joined);

   list_free(&joined);
   list_free(&ref_left);
   list_free(&ref_right);
   list_free(&left);
   list_free(&right);
   list_free(&clone);
   list_free(&ref);
   list_free(&instance);
}

static void TestRegistries(void) {
   printf("Registries:\n");

   invariant(tree_registry_count > 0);
   invariant(dist_count > 0);
   invariant(balancer_registry_count > 0);
   invariant(tree_registry_find(NULL) == NULL);
   invariant(tree_registry_find("") == NULL);
   invariant(tree_registry_find("Bad/Name") == NULL);
   invariant(tree_registry_find("Bad\\Name") == NULL);
   invariant(tree_registry_find("Bad\200Name") == NULL);
   invariant(tree_registry_find("__missing_tree__") == NULL);
   invariant(dist_find(NULL) == NULL);
   invariant(dist_find("") == NULL);
   invariant(dist_find("Bad Name") == NULL);
   invariant(dist_find("Bad\200Name") == NULL);
   invariant(dist_find("__missing_distribution__") == NULL);
   invariant(balancer_registry_find(NULL) == NULL);
   invariant(balancer_registry_find("") == NULL);
   invariant(balancer_registry_find("Bad/Name") == NULL);
   invariant(balancer_registry_find("Bad\\Name") == NULL);
   invariant(balancer_registry_find("Bad\200Name") == NULL);
   invariant(balancer_registry_find("__missing_balancer__") == NULL);
   assert_registry_lookup_exits_2_with_stderr(
       test_tree_registry_find_or_exit_missing_child,
       "unknown tree: __missing_tree__");
   assert_registry_lookup_exits_2_with_stderr(
       test_tree_registry_find_or_exit_null_child, "tree name is null");
   assert_registry_lookup_exits_2_with_stderr(
       test_tree_registry_find_or_exit_empty_child, "tree name is empty");
   assert_registry_lookup_exits_2_with_stderr(
       test_tree_registry_find_or_exit_bad_component_child,
       "tree name is not a path component");
   assert_registry_lookup_exits_2_with_stderr(
       test_tree_registry_find_or_exit_high_byte_child,
       "tree name is not a path component");
   assert_registry_lookup_exits_2_with_stderr(
       test_distribution_find_or_exit_missing_child,
       "unknown distribution: __missing_distribution__");
   assert_registry_lookup_exits_2_with_stderr(
       test_distribution_find_or_exit_null_child, "distribution name is null");
   assert_registry_lookup_exits_2_with_stderr(
       test_distribution_find_or_exit_empty_child,
       "distribution name is empty");
   assert_registry_lookup_exits_2_with_stderr(
       test_distribution_find_or_exit_bad_token_child,
       "distribution name is not a printable token");
   assert_registry_lookup_exits_2_with_stderr(
       test_distribution_find_or_exit_high_byte_child,
       "distribution name is not a printable token");
   assert_registry_lookup_exits_2_with_stderr(
       test_balancer_registry_find_or_exit_missing_child,
       "unknown balancer: __missing_balancer__");
   assert_registry_lookup_exits_2_with_stderr(
       test_balancer_registry_find_or_exit_null_child, "balancer name is null");
   assert_registry_lookup_exits_2_with_stderr(
       test_balancer_registry_find_or_exit_empty_child,
       "balancer name is empty");
   assert_registry_lookup_exits_2_with_stderr(
       test_balancer_registry_find_or_exit_bad_component_child,
       "balancer name is not a path component");
   assert_registry_lookup_exits_2_with_stderr(
       test_balancer_registry_find_or_exit_high_byte_child,
       "balancer name is not a path component");
   invariant(balancer_name_is_component("Median"));
   invariant(!balancer_name_is_component(NULL));
   invariant(!balancer_name_is_component(""));
   invariant(!balancer_name_is_component("."));
   invariant(!balancer_name_is_component(".."));
   invariant(!balancer_name_is_component("Bad/Name"));
   invariant(!balancer_name_is_component("Bad\\Name"));
   invariant(!balancer_name_is_component("Bad Name"));
   invariant(!balancer_name_is_component("Bad\200Name"));
   assert_aborts(test_balancer_require_empty_name_child);
   assert_aborts(test_balancer_require_dot_name_child);
   assert_aborts(test_balancer_require_dotdot_name_child);
   assert_aborts(test_balancer_require_slash_name_child);
   assert_aborts(test_balancer_require_space_name_child);
   assert_aborts(test_balancer_registry_fprint_names_null_file_child);
   assert_aborts(test_balancer_registry_fprint_names_null_label_child);

   for (int i = 0; i < tree_registry_count; i++) {
      const list_vtable_t *vt = tree_registry_all[i];
      list_vtable_require(vt);
      invariant(list_vtable_name_is_component(vt->name));
      invariant(tree_registry_find(vt->name) == vt);
      invariant(tree_registry_find_or_exit(vt->name) == vt);
      for (int j = i + 1; j < tree_registry_count; j++) {
         invariant(strcmp(vt->name, tree_registry_all[j]->name) != 0);
      }

      list_t instance = list_new(vt);
      list_verify(&instance);
      list_free(&instance);

      assert_tree_registry_vtable_contract(vt);
   }

   tree_registry_selection_t tree_selection;
   tree_registry_selection_init(&tree_selection, NULL);
   assert_int_equal(tree_registry_count, tree_selection.count);
   invariant(tree_selection.items == tree_registry_all);
   invariant(tree_selection.selected[0] == NULL);

   const list_vtable_t *splay_tree = tree_registry_find_or_exit("Splay");
   tree_registry_selection_init(&tree_selection, splay_tree->name);
   assert_int_equal(1, tree_selection.count);
   invariant(tree_selection.items[0] == splay_tree);
   tree_registry_selection_init(&tree_selection, NULL);
   assert_int_equal(tree_registry_count, tree_selection.count);
   invariant(tree_selection.items == tree_registry_all);
   invariant(tree_selection.selected[0] == NULL);
   assert_aborts(test_tree_registry_selection_null_child);
   assert_aborts(test_tree_registry_fprint_names_null_file_child);
   assert_aborts(test_tree_registry_fprint_names_null_label_child);
   printf("  tree registry selection OK\n");

   for (int i = 0; i < dist_count; i++) {
      const dist_spec_t *spec = dist_all[i];
      dist_spec_require(spec);
      invariant(dist_name_is_data_token(spec->name));
      invariant(dist_find(spec->name) == spec);
      invariant(dist_find_or_exit(spec->name) == spec);
      for (int j = i + 1; j < dist_count; j++) {
         invariant(strcmp(spec->name, dist_all[j]->name) != 0);
      }

      distribution_t *dist = dist_create(spec, 1);
      assert_equal(0, dist_less_than(dist, 1));
      dist_destroy(dist);
   }

   assert_distribution_registry_slice(dist_benchmark, dist_benchmark_count);
   assert_distribution_registry_slice(dist_uniform, dist_uniform_count);
   assert_distribution_registry_slice(dist_animation, dist_animation_count);

   const dist_spec_t *normal_dist = dist_find_or_exit("Normal");
   dist_selection_t benchmark_dist_selection;
   dist_benchmark_selection_init(&benchmark_dist_selection, NULL);
   assert_int_equal(dist_benchmark_count, benchmark_dist_selection.count);
   invariant(benchmark_dist_selection.items == dist_benchmark);
   invariant(benchmark_dist_selection.selected[0] == NULL);
   dist_benchmark_selection_init(&benchmark_dist_selection, normal_dist->name);
   assert_int_equal(1, benchmark_dist_selection.count);
   invariant(benchmark_dist_selection.items[0] == normal_dist);
   dist_benchmark_selection_init(&benchmark_dist_selection, NULL);
   assert_int_equal(dist_benchmark_count, benchmark_dist_selection.count);
   invariant(benchmark_dist_selection.items == dist_benchmark);
   invariant(benchmark_dist_selection.selected[0] == NULL);
   assert_aborts(test_dist_benchmark_selection_null_child);
   assert_aborts(test_dist_fprint_names_null_file_child);
   assert_aborts(test_dist_fprint_names_null_label_child);
   assert_aborts(test_dist_fprint_names_negative_count_child);
   printf("  benchmark distribution selection OK\n");

   char distribution_names_line[128];
   FILE *distribution_names = tmpfile();
   invariant(distribution_names != NULL);
   dist_fprint_names(distribution_names, "distributions", NULL, 0);
   rewind(distribution_names);
   test_expect_file_line(distribution_names, "distribution names",
                         distribution_names_line,
                         sizeof(distribution_names_line), "distributions:\n");
   test_expect_file_eof(distribution_names, "distribution names",
                        distribution_names_line,
                        sizeof(distribution_names_line));
   invariant(fclose(distribution_names) == 0);
   printf("  empty distribution name slice OK\n");

   balancer_run_distribution_selection_t balancer_dist_selection;
   balancer_run_distribution_selection_init(&balancer_dist_selection, NULL);
   assert_int_equal(dist_uniform_count, balancer_dist_selection.count);
   invariant(balancer_dist_selection.items == dist_uniform);

   balancer_run_distribution_selection_init(&balancer_dist_selection,
                                            normal_dist);
   assert_int_equal(1, balancer_dist_selection.count);
   invariant(balancer_dist_selection.items == balancer_dist_selection.selected);
   invariant(balancer_dist_selection.items[0] == normal_dist);
   balancer_run_distribution_selection_init(&balancer_dist_selection, NULL);
   assert_int_equal(dist_uniform_count, balancer_dist_selection.count);
   invariant(balancer_dist_selection.items == dist_uniform);
   invariant(balancer_dist_selection.selected[0] == NULL);
   assert_aborts(test_balancer_run_distribution_selection_null_child);
   assert_balancer_run_outputs_child_exits_2_with_stderr(
       test_balancer_run_distribution_selection_unregistered_child,
       "balancer distribution is not registered");

   balancer_run_distribution_selection_init_by_name(&balancer_dist_selection,
                                                    NULL);
   assert_int_equal(dist_uniform_count, balancer_dist_selection.count);
   invariant(balancer_dist_selection.items == dist_uniform);
   balancer_run_distribution_selection_init_by_name(&balancer_dist_selection,
                                                    normal_dist->name);
   assert_int_equal(1, balancer_dist_selection.count);
   invariant(balancer_dist_selection.items == balancer_dist_selection.selected);
   invariant(balancer_dist_selection.items[0] == normal_dist);
   assert_aborts(test_balancer_run_distribution_selection_by_name_null_child);
   printf("  balancer distribution selection OK\n");

   sample_schedule_t balancer_schedule = balancer_run_sample_schedule(0, 0);
   assert_equal(1, balancer_schedule.total);
   assert_equal(1, balancer_schedule.samples);
   balancer_schedule = balancer_run_sample_schedule(5, 100);
   assert_equal(5, balancer_schedule.total);
   assert_equal(5, balancer_schedule.samples);
   if (arena_max_capacity() < UINT64_MAX) {
      assert_balancer_run_outputs_child_exits_2_with_stderr(
          test_balancer_run_sample_schedule_too_large_child,
          "balancer scale is too large");
   }
   assert_equal(1, balancer_run_next_iteration(0));
   assert_equal(UINT64_MAX, balancer_run_next_iteration(UINT64_MAX - 1));
   assert_balancer_run_outputs_child_exits_2_with_stderr(
       test_balancer_run_next_iteration_overflow_child,
       "balancer iteration count is too large");
   printf("  balancer sample schedule OK\n");

   const dist_spec_t *seed_dist = dist_all[0];
   u64 expected_balancer_seed = random_derive_seed(0, "balancer-benchmarks");
   expected_balancer_seed =
       random_derive_seed(expected_balancer_seed, "position");
   expected_balancer_seed = random_derive_u64(expected_balancer_seed, 1000);
   expected_balancer_seed =
       random_derive_seed(expected_balancer_seed, seed_dist->name);
   expected_balancer_seed =
       random_derive_seed(expected_balancer_seed, "iteration");
   expected_balancer_seed = random_derive_u64(expected_balancer_seed, 17);
   u64 benchmark_seed =
       balancer_run_seed("balancer-benchmarks", seed_dist, 1000, 17);
   assert_equal(expected_balancer_seed, benchmark_seed);
   invariant(benchmark_seed !=
             balancer_run_seed("balancer-measurements", seed_dist, 1000, 17));
   const dist_spec_t *alternate_seed_dist = dist_all[dist_count - 1];
   dist_spec_require(alternate_seed_dist);
   if (alternate_seed_dist != seed_dist) {
      invariant(benchmark_seed != balancer_run_seed("balancer-benchmarks",
                                                    alternate_seed_dist, 1000,
                                                    17));
   }
   invariant(benchmark_seed !=
             balancer_run_seed("balancer-benchmarks", seed_dist, 1001, 17));
   invariant(benchmark_seed !=
             balancer_run_seed("balancer-benchmarks", seed_dist, 1000, 18));
   random_seed(benchmark_seed);
   u64 expected_next = random_uint64();
   random_seed(benchmark_seed);
   (void)balancer_run_seed("balancer-benchmarks", seed_dist, 1000, 17);
   assert_equal(expected_next, random_uint64());
   assert_aborts(test_balancer_run_seed_empty_domain_child);
   assert_aborts(test_balancer_run_seed_null_distribution_child);
   assert_balancer_run_outputs_child_exits_2_with_stderr(
       test_balancer_run_seed_unregistered_distribution_child,
       "balancer distribution is not registered");
   assert_aborts(test_balancer_run_seed_zero_position_child);
   assert_aborts(test_balancer_run_seed_zero_iteration_child);
   printf("  balancer run seeds OK\n");

   arena_t balancer_run_arena;
   arena_init(&balancer_run_arena, 8);
   arena_t *previous_balancer_run_arena = arena_install(&balancer_run_arena);
   splay_t balancer_run_splay;
   splay_init(&balancer_run_splay);
   balancer_run_grow_splay_to(&balancer_run_splay, 0);
   assert_equal(0, balancer_run_splay.tree.size);
   balancer_run_grow_splay_to(&balancer_run_splay, 8);
   assert_equal(8, balancer_run_splay.tree.size);
   assert_equal(8, tree_verify_size(balancer_run_splay.tree.root,
                                    balancer_run_splay.tree.size));
   balancer_run_grow_splay_to(&balancer_run_splay, 8);
   assert_equal(8, balancer_run_splay.tree.size);
   balancer_run_splay.tree = balancer_run_randomize_tree(
       balancer_run_splay.tree, seed_dist, "balancer-test", 3);
   assert_equal(8, balancer_run_splay.tree.size);
   assert_equal(8, tree_verify_size(balancer_run_splay.tree.root,
                                    balancer_run_splay.tree.size));
   balancer_run_splay.tree =
       balancer_run_restore_tree(&balancer_median, balancer_run_splay.tree);
   assert_equal(8, balancer_run_splay.tree.size);
   balancer_verify_tree(&balancer_median, balancer_run_splay.tree);
   splay_free(&balancer_run_splay);
   arena_restore(previous_balancer_run_arena);
   arena_destroy(&balancer_run_arena);
   assert_aborts(test_balancer_run_grow_splay_to_null_child);
   assert_aborts(test_balancer_run_grow_splay_to_shrink_child);
   assert_aborts(test_balancer_run_grow_splay_to_root_size_mismatch_child);
   assert_aborts(test_balancer_run_randomize_empty_tree_child);
   assert_aborts(test_balancer_run_randomize_root_size_mismatch_child);
   assert_balancer_run_outputs_child_exits_2_with_stderr(
       test_balancer_run_randomize_unregistered_distribution_child,
       "balancer distribution is not registered");
   assert_aborts(test_balancer_run_restore_null_balancer_child);
   assert_aborts(test_balancer_run_restore_input_root_size_mismatch_child);
   assert_aborts(test_balancer_run_restore_output_root_size_mismatch_child);
   assert_aborts(test_balancer_verify_tree_root_size_mismatch_child);
   assert_aborts(test_balancer_verify_tree_mutating_child);
   assert_aborts(test_balancer_direct_restore_root_size_mismatch_child);
   assert_aborts(test_balancer_direct_verify_root_size_mismatch_child);
   assert_aborts(test_balancer_dsw_restore_root_size_mismatch_child);
   assert_aborts(test_balancer_dsw_verify_root_size_mismatch_child);
#ifdef DEBUG
   assert_aborts(test_balancer_run_restore_verifies_child);
#endif
   printf("  balancer run tree lifecycle OK\n");

   assert_balancer_run_outputs_make_exits_2(
       NULL, "benchmarks", "balancer output directory is null");
   assert_balancer_run_outputs_make_exits_2(
       "", "benchmarks", "balancer output directory is empty");
   assert_balancer_run_outputs_make_exits_2("/tmp/bst-registry", NULL,
                                            "balancer output kind is null");
   assert_balancer_run_outputs_make_exits_2("/tmp/bst-registry", "",
                                            "balancer output kind is empty");
   char balancer_kind_output_dir[] = "/tmp/bst-balancer-kind-XXXXXX";
   char *balancer_kind_root = mkdtemp(balancer_kind_output_dir);
   invariant(balancer_kind_root != NULL);
   assert_balancer_run_outputs_make_exits_2(
       balancer_kind_root, "../escaped",
       "balancer output kind is not a path component");
   assert_balancer_run_outputs_make_exits_2(
       balancer_kind_root, "Bad\200Kind",
       "balancer output kind is not a path component");
   char balancer_kind_escaped_dir[512];
   path_format(balancer_kind_escaped_dir, sizeof(balancer_kind_escaped_dir),
               "%s/escaped", balancer_kind_root);
   assert_path_absent(balancer_kind_escaped_dir);
   invariant(rmdir(balancer_kind_root) == 0);

   assert_balancer_run_outputs_child_exits_2_with_stderr(
       balancer_run_make_unregistered_selected_child,
       "selected balancer is not registered");
   assert_balancer_run_outputs_child_exits_2_with_stderr(
       balancer_run_make_bad_selected_name_child,
       "balancer output balancer name is not a path component");
   assert_balancer_run_outputs_child_exits_2_with_stderr(
       balancer_run_make_high_byte_selected_name_child,
       "balancer output balancer name is not a path component");
   assert_balancer_run_outputs_child_exits_2_with_stderr(
       balancer_run_make_null_outputs_child, "balancer output list is null");
   assert_balancer_run_outputs_child_exits_2_with_stderr(
       balancer_run_make_negative_capacity_child,
       "balancer output capacity is negative");
   assert_balancer_run_outputs_child_exits_2_with_stderr(
       balancer_run_make_selected_zero_capacity_child,
       "balancer output capacity is too small");
   if (balancer_registry_count > 1) {
      assert_balancer_run_outputs_child_exits_2_with_stderr(
          balancer_run_make_small_capacity_child,
          "balancer output capacity is too small");
   }
   assert_balancer_run_outputs_child_exits_2(
       balancer_run_prepare_null_outputs_child);
   assert_balancer_run_outputs_child_exits_2(
       balancer_run_prepare_empty_outputs_child);
   assert_balancer_run_outputs_child_exits_2(
       balancer_run_prepare_too_many_outputs_child);
   assert_balancer_run_outputs_child_exits_2_with_stderr(
       balancer_run_prepare_null_balancer_child,
       "balancer output balancer is null");
   assert_balancer_run_outputs_child_exits_2_with_stderr(
       balancer_run_prepare_unregistered_balancer_child,
       "balancer output balancer is not registered");
   assert_balancer_run_outputs_child_exits_2(
       balancer_run_prepare_null_dir_child);
   assert_balancer_run_outputs_child_exits_2(
       balancer_run_prepare_empty_dir_child);
   if (balancer_registry_count >= 2) {
      char prepare_output_dir[] = "/tmp/bst-balancer-prepare-preflight-XXXXXX";
      char *prepare_root = mkdtemp(prepare_output_dir);
      invariant(prepare_root != NULL);
      path_format(test_balancer_run_prepare_valid_dir,
                  sizeof(test_balancer_run_prepare_valid_dir), "%s/created",
                  prepare_root);
      path_format(test_balancer_run_prepare_valid_path,
                  sizeof(test_balancer_run_prepare_valid_path), "%s/output",
                  test_balancer_run_prepare_valid_dir);
      assert_balancer_run_outputs_child_exits_2_with_stderr(
          balancer_run_prepare_late_empty_dir_child,
          "balancer output directory is empty");
      assert_path_absent(test_balancer_run_prepare_valid_dir);
      test_balancer_run_prepare_valid_dir[0] = '\0';
      test_balancer_run_prepare_valid_path[0] = '\0';
      invariant(rmdir(prepare_root) == 0);

      char wrong_output_dir[] = "/tmp/bst-balancer-wrong-parent-XXXXXX";
      char *wrong_root = mkdtemp(wrong_output_dir);
      invariant(wrong_root != NULL);
      path_format(test_balancer_run_prepare_wrong_root,
                  sizeof(test_balancer_run_prepare_wrong_root), "%s",
                  wrong_root);
      path_format(test_balancer_run_prepare_wrong_missing_dir,
                  sizeof(test_balancer_run_prepare_wrong_missing_dir),
                  "%s/missing", wrong_root);
      path_format(test_balancer_run_prepare_wrong_path,
                  sizeof(test_balancer_run_prepare_wrong_path), "%s/run.txt",
                  test_balancer_run_prepare_wrong_missing_dir);
      assert_balancer_run_outputs_child_exits_2_with_stderr(
          balancer_run_prepare_wrong_parent_dir_child,
          "balancer output directory is not path parent");
      assert_path_absent(test_balancer_run_prepare_wrong_missing_dir);
      assert_path_absent(test_balancer_run_prepare_wrong_path);
      test_balancer_run_prepare_wrong_root[0] = '\0';
      test_balancer_run_prepare_wrong_missing_dir[0] = '\0';
      test_balancer_run_prepare_wrong_path[0] = '\0';
      invariant(rmdir(wrong_root) == 0);

      assert_balancer_run_outputs_child_exits_2_with_stderr(
          balancer_run_prepare_duplicate_balancer_child,
          "duplicate balancer output");
      assert_balancer_run_outputs_child_exits_2_with_stderr(
          balancer_run_prepare_duplicate_path_child,
          "duplicate balancer output path");
      assert_balancer_run_outputs_child_exits_2_with_stderr(
          balancer_run_prepare_late_invalid_duplicate_path_child,
          "balancer output directory is empty");
   }
   assert_balancer_run_outputs_child_exits_2(
       balancer_run_publish_null_outputs_child);
   assert_balancer_run_outputs_child_exits_2(
       balancer_run_publish_empty_outputs_child);
   assert_balancer_run_outputs_child_exits_2(
       balancer_run_publish_too_many_outputs_child);
   assert_balancer_run_outputs_child_exits_2(
       balancer_run_dispose_negative_count_child);
   assert_balancer_run_outputs_child_exits_2(
       balancer_run_dispose_null_negative_count_child);
   assert_balancer_run_outputs_child_exits_2_with_stderr(
       balancer_run_dispose_null_positive_count_child,
       "balancer output list is null");
   assert_balancer_run_outputs_child_exits_2(
       balancer_run_dispose_too_many_outputs_child);
   balancer_run_outputs_dispose(NULL, 0);

   for (int i = 0; i < balancer_registry_count; i++) {
      const balancer_t *balancer = balancer_registry_all[i];
      balancer_require(balancer);
      invariant(balancer_name_is_component(balancer->name));
      invariant(balancer_registry_find(balancer->name) == balancer);
      invariant(balancer_registry_find_or_exit(balancer->name) == balancer);
      invariant(balancer_registry_find_optional_or_exit(balancer->name) ==
                balancer);
      for (int j = i + 1; j < balancer_registry_count; j++) {
         invariant(strcmp(balancer->name, balancer_registry_all[j]->name) != 0);
      }
   }
   invariant(balancer_registry_find_optional_or_exit(NULL) == NULL);

   balancer_run_output_t *balancer_outputs = balancer_run_outputs_alloc();
   int balancer_output_count =
       balancer_run_outputs_make(balancer_outputs, balancer_registry_count,
                                 NULL, "/tmp/bst-registry", "benchmarks");
   assert_int_equal(balancer_registry_count, balancer_output_count);
   for (int i = 0; i < balancer_output_count; i++) {
      const balancer_t *balancer = balancer_registry_all[i];
      invariant(balancer_outputs[i].balancer == balancer);
      invariant(balancer_outputs[i].file.tmp_path == NULL);

      char expected_path[512];
      path_format(expected_path, sizeof(expected_path),
                  "/tmp/bst-registry/balancers/benchmarks/%s", balancer->name);
      invariant(strcmp(balancer_outputs[i].file.path, expected_path) == 0);
      invariant(strcmp(balancer_outputs[i].dir,
                       "/tmp/bst-registry/balancers/benchmarks") == 0);
      for (int j = i + 1; j < balancer_output_count; j++) {
         invariant(strcmp(balancer_outputs[i].file.path,
                          balancer_outputs[j].file.path) != 0);
      }
   }
   balancer_run_outputs_dispose(balancer_outputs, balancer_output_count);

   const balancer_t *selected_balancer = balancer_registry_all[0];
   balancer_outputs = balancer_run_outputs_alloc();
   balancer_output_count = balancer_run_outputs_make(
       balancer_outputs, balancer_registry_count, selected_balancer,
       "/tmp/bst-registry", "measurements");
   assert_int_equal(1, balancer_output_count);
   invariant(balancer_outputs[0].balancer == selected_balancer);
   invariant(balancer_outputs[0].file.tmp_path == NULL);
   char expected_path[512];
   path_format(expected_path, sizeof(expected_path),
               "/tmp/bst-registry/balancers/measurements/%s",
               selected_balancer->name);
   invariant(strcmp(balancer_outputs[0].file.path, expected_path) == 0);
   invariant(strcmp(balancer_outputs[0].dir,
                    "/tmp/bst-registry/balancers/measurements") == 0);
   balancer_run_outputs_dispose(balancer_outputs, balancer_output_count);

   assert_balancer_run_publish_preflights_all_outputs();
   printf("  balancer output publish preflight OK\n");

   printf("  registry lookup and vtable contracts OK\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// Test runner
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   const char *name;
   test_fn fn;
} named_test_t;

typedef struct {
   const char *dist_filter;
} standalone_test_ctx_t;

typedef void (*standalone_test_fn)(standalone_test_ctx_t *ctx);

typedef struct {
   const char *name;
   standalone_test_fn fn;
} standalone_test_t;

static const named_test_t all_tests[] = {
    {"TestInsert", TestInsert},
    {"TestInsertPersistent", TestInsertPersistent},
    {"TestSelect", TestSelect},
    {"TestClone", TestClone},
    {"TestSelectPersistent", TestSelectPersistent},
    {"TestSelectAfterInsert", TestSelectAfterInsert},
    {"TestSelectAfterInsertPersistent", TestSelectAfterInsertPersistent},
    {"TestUpdate", TestUpdate},
    {"TestUpdatePersistent", TestUpdatePersistent},
    {"TestDelete", TestDelete},
    {"TestDeletePersistent", TestDeletePersistent},
    {"TestInsertDelete", TestInsertDelete},
    {"TestInsertDeletePersistent", TestInsertDeletePersistent},
    {"TestSplit", TestSplit},
    {"TestJoin", TestJoin},
    {"TestJoinFromSplit", TestJoinFromSplit},
    {"TestJoinAfterInsertDelete", TestJoinAfterInsertDelete},
};

static const int num_tests = sizeof(all_tests) / sizeof(all_tests[0]);

static void RunTestRandom(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestRandom();
}

static void RunTestRandomizedJoinPurity(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestRandomizedJoinPurity();
}

static void RunTestSharedJoinContracts(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestSharedJoinContracts();
}

static void RunTestTreapFingerTreeContracts(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestTreapFingerTreeContracts();
}

static void RunTestDistributions(standalone_test_ctx_t *ctx) {
   invariant(ctx != NULL);
   TestDistributions(ctx->dist_filter);
}

static void RunTestTreePathMetrics(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestTreePathMetrics();
}

static void RunTestRankBoundaries(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestRankBoundaries();
}

static void RunTestTreeRandomize(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestTreeRandomize();
}

static void RunTestRankVerifiers(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestRankVerifiers();
}

static void RunTestWeightVerifiers(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestWeightVerifiers();
}

static void RunTestTreeArenaOwnership(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestTreeArenaOwnership();
}

static void RunTestTreePersistenceOwnership(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestTreePersistenceOwnership();
}

static void RunTestOperations(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestOperations();
}

static void RunTestMeasurements(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestMeasurements();
}

static void RunTestFilesystem(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestFilesystem();
}

static void RunTestBenchmarkIndex(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestBenchmarkIndex();
}

static void RunTestBenchmarkOrder(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestBenchmarkOrder();
}

static void RunTestBenchmarkData(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestBenchmarkData();
}

static void RunTestCliParsing(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestCliParsing();
}

static void RunTestListContracts(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestListContracts();
}

static void RunTestReferenceList(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestReferenceList();
}

static void RunTestCheckedAllocation(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestCheckedAllocation();
}

static void RunTestRegistries(standalone_test_ctx_t *ctx) {
   (void)ctx;
   TestRegistries();
}

static const standalone_test_t standalone_tests[] = {
    {"TestRandom", RunTestRandom},
    {"TestRandomizedJoinPurity", RunTestRandomizedJoinPurity},
    {"TestSharedJoinContracts", RunTestSharedJoinContracts},
    {"TestTreapFingerTreeContracts", RunTestTreapFingerTreeContracts},
    {"TestDistributions", RunTestDistributions},
    {"TestTreePathMetrics", RunTestTreePathMetrics},
    {"TestRankBoundaries", RunTestRankBoundaries},
    {"TestTreeRandomize", RunTestTreeRandomize},
    {"TestRankVerifiers", RunTestRankVerifiers},
    {"TestWeightVerifiers", RunTestWeightVerifiers},
    {"TestTreeArenaOwnership", RunTestTreeArenaOwnership},
    {"TestTreePersistenceOwnership", RunTestTreePersistenceOwnership},
    {"TestOperations", RunTestOperations},
    {"TestMeasurements", RunTestMeasurements},
    {"TestFilesystem", RunTestFilesystem},
    {"TestBenchmarkIndex", RunTestBenchmarkIndex},
    {"TestBenchmarkOrder", RunTestBenchmarkOrder},
    {"TestBenchmarkData", RunTestBenchmarkData},
    {"TestCliParsing", RunTestCliParsing},
    {"TestListContracts", RunTestListContracts},
    {"TestReferenceList", RunTestReferenceList},
    {"TestCheckedAllocation", RunTestCheckedAllocation},
    {"TestRegistries", RunTestRegistries},
};

static const int standalone_test_count =
    sizeof(standalone_tests) / sizeof(standalone_tests[0]);

static bool name_matches(const char *filter, const char *name) {
   invariant(name != NULL);
   return filter == NULL || strcmp(filter, name) == 0;
}

static bool known_tree(const char *name) {
   return name == NULL || tree_registry_find(name) != NULL;
}

static bool known_distribution(const char *name) {
   return name == NULL || dist_find(name) != NULL;
}

static bool known_balancer(const char *name) {
   return name == NULL || balancer_registry_find(name) != NULL;
}

static bool standalone_test_name(const char *name) {
   invariant(name != NULL);
   for (int i = 0; i < standalone_test_count; i++) {
      if (strcmp(name, standalone_tests[i].name) == 0) {
         return true;
      }
   }
   return false;
}

static bool tree_test_name(const char *name) {
   invariant(name != NULL);
   for (int i = 0; i < num_tests; i++) {
      if (strcmp(name, all_tests[i].name) == 0) {
         return true;
      }
   }
   return false;
}

static bool known_test(const char *name) {
   if (name == NULL) {
      return true;
   }
   if (strcmp(name, "TestBalancers") == 0) {
      return true;
   }
   return standalone_test_name(name) || tree_test_name(name);
}

static void print_known_names(void) {
   tree_registry_fprint_names(stderr, "trees");
   dist_fprint_names(stderr, "distributions", dist_all, dist_count);
   balancer_registry_fprint_names(stderr, "balancers");

   fprintf(stderr, "tests: TestBalancers");
   for (int i = 0; i < standalone_test_count; i++) {
      fprintf(stderr, " %s", standalone_tests[i].name);
   }
   for (int i = 0; i < num_tests; i++) {
      fprintf(stderr, " %s", all_tests[i].name);
   }
   fprintf(stderr, "\n");
}

static const char *test_count_noun(u64 count) {
   return count == 1 ? "test" : "tests";
}

int main(int argc, char **argv) {
   invariant(argc > 0);
   invariant(argv != NULL);
   invariant(argv[0] != NULL);
   test_executable_path = argv[0];
   random_seed(1);

   const char *tree_filter = NULL;
   const char *dist_filter = NULL;
   const char *test_filter = NULL;
   const char *balancer_filter = NULL;
   bool balancers_explicit = false;
   bool run_balancers = false;
   u64 standalone_total = 0;
   u64 balancer_total = 0;
   u64 scale = 100;
   u64 balancer_scale = 1000;

   for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-tree") == 0) {
         tree_filter = cli_parse_nonempty_string(
             cli_arg_value(argc, argv, &i, "-tree"), "-tree");
      } else if ((strcmp(argv[i], "-distribution") == 0 ||
                  strcmp(argv[i], "-dist") == 0)) {
         const char *flag = argv[i];
         dist_filter = cli_parse_nonempty_string(
             cli_arg_value(argc, argv, &i, flag), flag);
      } else if (strcmp(argv[i], "-test") == 0) {
         test_filter = cli_parse_nonempty_string(
             cli_arg_value(argc, argv, &i, "-test"), "-test");
      } else if (strcmp(argv[i], "-scale") == 0) {
         scale = cli_parse_counter_count(
             cli_arg_value(argc, argv, &i, "-scale"), "-scale");
         balancer_scale = scale;
      } else if ((strcmp(argv[i], "-balancer") == 0 ||
                  strcmp(argv[i], "-strategy") == 0)) {
         const char *flag = argv[i];
         balancer_filter = cli_parse_nonempty_string(
             cli_arg_value(argc, argv, &i, flag), flag);
      } else if (strcmp(argv[i], "-balancers") == 0) {
         balancers_explicit = true;
         run_balancers = true;
      } else if (strcmp(argv[i], "-skip-balancers") == 0) {
         balancers_explicit = true;
         run_balancers = false;
      } else {
         return cli_unknown_arg(argv[i]);
      }
   }

   bool tree_filter_valid =
       tree_filter == NULL || list_vtable_name_is_component(tree_filter);
   bool dist_filter_valid =
       dist_filter == NULL || dist_name_is_data_token(dist_filter);
   bool balancer_filter_valid =
       balancer_filter == NULL || balancer_name_is_component(balancer_filter);
   bool test_filter_valid =
       test_filter == NULL || bst_name_is_printable_token(test_filter);

   if (!tree_filter_valid || !dist_filter_valid || !balancer_filter_valid ||
       !test_filter_valid || !known_tree(tree_filter) ||
       !known_distribution(dist_filter) || !known_balancer(balancer_filter) ||
       !known_test(test_filter)) {
      if (tree_filter && !tree_filter_valid) {
         fprintf(stderr, "tree name is not a path component\n");
      } else if (tree_filter && !known_tree(tree_filter)) {
         fprintf(stderr, "unknown tree: %s\n", tree_filter);
      }
      if (dist_filter && !dist_filter_valid) {
         fprintf(stderr, "distribution name is not a printable token\n");
      } else if (dist_filter && !known_distribution(dist_filter)) {
         fprintf(stderr, "unknown distribution: %s\n", dist_filter);
      }
      if (balancer_filter && !balancer_filter_valid) {
         fprintf(stderr, "balancer name is not a path component\n");
      } else if (balancer_filter && !known_balancer(balancer_filter)) {
         fprintf(stderr, "unknown balancer: %s\n", balancer_filter);
      }
      if (test_filter && !test_filter_valid) {
         fprintf(stderr, "test name is not a printable token\n");
      } else if (test_filter && !known_test(test_filter)) {
         fprintf(stderr, "unknown test: %s\n", test_filter);
      }
      print_known_names();
      return 2;
   }

   bool tree_scope = tree_filter != NULL || dist_filter != NULL;
   bool balancer_scope = balancer_filter != NULL;

   if (!balancers_explicit) {
      run_balancers = test_filter == NULL
                          ? (balancer_scope || !tree_scope)
                          : strcmp(test_filter, "TestBalancers") == 0;
   }

   bool run_standalone_tests = test_filter == NULL
                                   ? (!tree_scope && !balancer_scope)
                                   : standalone_test_name(test_filter);

   bool run_tree_tests =
       test_filter == NULL
           ? (tree_filter != NULL || (dist_filter != NULL && !balancer_scope) ||
              (!tree_scope && !balancer_scope))
           : tree_test_name(test_filter);

   if (run_balancers && name_matches(test_filter, "TestBalancers")) {
      balancer_total =
          TestBalancers(balancer_scale, balancer_filter, dist_filter);
   }

   if (run_standalone_tests) {
      standalone_test_ctx_t standalone_ctx = {
          .dist_filter = dist_filter,
      };
      for (int i = 0; i < standalone_test_count; i++) {
         if (!name_matches(test_filter, standalone_tests[i].name)) {
            continue;
         }
         standalone_tests[i].fn(&standalone_ctx);
         standalone_total++;
      }
   }

   u64 total = 0;

   if (run_tree_tests) {
      for (int t = 0; t < tree_registry_count; t++) {
         if (!name_matches(tree_filter, tree_registry_all[t]->name)) {
            continue;
         }

         printf("%s:\n", tree_registry_all[t]->name);
         for (int d = 0; d < dist_count; d++) {
            if (!name_matches(dist_filter, dist_all[d]->name)) {
               continue;
            }

            printf("  %s: ", dist_all[d]->name);
            fflush(stdout);
            for (int f = 0; f < num_tests; f++) {
               if (!name_matches(test_filter, all_tests[f].name)) {
                  continue;
               }

               for (u64 s = 0;; s++) {
                  distribution_t *dist =
                      dist_create(dist_all[d], random_uint64());
                  all_tests[f].fn(tree_registry_all[t], s, dist);
                  dist_destroy(dist);
                  total++;
                  if (s == scale) {
                     break;
                  }
               }
               printf(".");
               fflush(stdout);
            }
            printf(" OK\n");
         }
      }
   }

   u64 matched_total = total + standalone_total + balancer_total;
   if (matched_total == 0) {
      fprintf(stderr, "no tests matched\n");
      return 2;
   }

   printf("\nAll tests passed (%llu %s).\n", (unsigned long long)matched_total,
          test_count_noun(matched_total));
   return 0;
}
