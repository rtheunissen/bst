// Comparison program: outputs deterministic operation results for each tree.
//
// For each tree, inserts N elements, then selects all, then deletes all.
// Outputs the sequence of values for comparison across implementations/runs.
//
#include "bst.h"
#include "cli.h"
#include "list.h"
#include "random.h"
#include "distributions.h"
#include "reference_list.h"
#include "tree_registry.h"

#include <stdio.h>
#include <string.h>

typedef struct {
   u64 *data;
   u64 len;
   u64 cap;
} collect_t;

static void compare_require_tree(const list_vtable_t *vt) {
   list_vtable_require(vt);
}

static void compare_require_distribution(const dist_spec_t *spec) {
   dist_spec_require(spec);
}

static void collect_visit(u64 x, void *ctx) {
   collect_t *c = (collect_t *)ctx;
   invariant(c != NULL);
   invariant(c->len < c->cap);
   invariant(c->data != NULL);
   u64 index = c->len;
   c->len = size_after_inserting_one(c->len);
   c->data[index] = x;
}

static u64 *compare_alloc_values(u64 count) {
   if (count == 0) {
      return NULL;
   }
   return (u64 *)bst_malloc_array(count, sizeof(u64));
}

static void compare_require_equal(u64 expected, u64 actual, const char *label) {
   invariant(label != NULL);
   if (expected != actual) {
      fprintf(stderr, "%s mismatch: expected %llu, actual %llu\n", label,
              (unsigned long long)expected, (unsigned long long)actual);
      exit(2);
   }
}

static void compare_require_equal_at(u64 expected, u64 actual,
                                     const char *label, u64 index) {
   invariant(label != NULL);
   if (expected != actual) {
      fprintf(stderr, "%s[%llu] mismatch: expected %llu, actual %llu\n", label,
              (unsigned long long)index, (unsigned long long)expected,
              (unsigned long long)actual);
      exit(2);
   }
}

typedef struct {
   list_t *expected;
   u64 index;
   u64 size;
} compare_assert_ctx_t;

static void compare_assert_visit(u64 actual, void *ctx) {
   compare_assert_ctx_t *c = (compare_assert_ctx_t *)ctx;
   invariant(c != NULL);
   invariant(c->expected != NULL);
   invariant(c->index < c->size);
   compare_require_equal_at(list_select(c->expected, c->index), actual, "value",
                            c->index);
   c->index = size_after_inserting_one(c->index);
}

static void compare_assert_list(list_t *expected, list_t *actual) {
   u64 size = list_size(expected);
   compare_require_equal(size, list_size(actual), "size");
   for (u64 i = 0; i < size; i++) {
      compare_require_equal_at(list_select(expected, i), list_select(actual, i),
                               "select", i);
   }
   compare_assert_ctx_t ctx = {.expected = expected, .size = size};
   list_each(actual, compare_assert_visit, &ctx);
   invariant(ctx.index == size);
}

static void compare_tree(const list_vtable_t *vt, const dist_spec_t *dist_spec,
                         u64 n, u64 seed) {
   compare_require_tree(vt);
   compare_require_distribution(dist_spec);

   printf("=== %s ===\n", vt->name);

   random_seed(seed);

   splitmix64_t seeds;
   splitmix64_seed(&seeds, seed);

   distribution_t *dist = dist_create(dist_spec, splitmix64_uint64(&seeds));

   xoshiro256_t values;
   xoshiro256_seed(&values, splitmix64_uint64(&seeds));

   list_t tree = list_new(vt);
   list_t ref = list_new(&reference_vtable);

   for (u64 i = 0; i < n; i++) {
      u64 pos = dist_less_than(dist, list_size(&tree) + 1);
      u64 val = xoshiro256_uint64(&values);
      list_insert(&tree, pos, val);
      list_insert(&ref, pos, val);
      list_verify(&tree);
      list_verify(&ref);
      compare_assert_list(&ref, &tree);
   }

   list_verify(&tree);
   list_verify(&ref);
   compare_assert_list(&ref, &tree);

   u64 size = list_size(&tree);
   collect_t col = {
       .data = compare_alloc_values(size),
       .len = 0,
       .cap = size,
   };
   list_each(&tree, collect_visit, &col);
   invariant(col.len == size);

   printf("size=%llu\n", (unsigned long long)size);
   printf("each:");
   for (u64 i = 0; i < col.len; i++) {
      printf(" %llu", (unsigned long long)col.data[i]);
   }
   printf("\n");

   printf("delete:");
   while (list_size(&tree) > 0) {
      u64 pos = dist_less_than(dist, list_size(&tree));
      u64 val = list_delete(&tree, pos);
      compare_require_equal_at(list_delete(&ref, pos), val, "delete", pos);
      list_verify(&tree);
      list_verify(&ref);
      compare_assert_list(&ref, &tree);
      printf(" %llu", (unsigned long long)val);
   }
   printf("\n\n");

   list_verify(&tree);
   list_verify(&ref);
   compare_assert_list(&ref, &tree);
   free(col.data);
   list_free(&tree);
   list_free(&ref);
   dist_destroy(dist);
}

int main(int argc, char **argv) {
   random_seed(1);
   u64 n = 200;
   u64 seed = 1;
   const char *tree_name = NULL;
   const char *dist_name = "Uniform";

   for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-tree") == 0) {
         tree_name = cli_parse_nonempty_string(
             cli_arg_value(argc, argv, &i, "-tree"), "-tree");
      } else if ((strcmp(argv[i], "-distribution") == 0 ||
                  strcmp(argv[i], "-dist") == 0)) {
         const char *flag = argv[i];
         dist_name = cli_parse_nonempty_string(
             cli_arg_value(argc, argv, &i, flag), flag);
      } else if ((strcmp(argv[i], "-n") == 0 ||
                  strcmp(argv[i], "-size") == 0)) {
         const char *flag = argv[i];
         n = cli_parse_array_count(cli_arg_value(argc, argv, &i, flag), flag,
                                   sizeof(u64));
      } else if (strcmp(argv[i], "-seed") == 0) {
         seed = cli_parse_u64(cli_arg_value(argc, argv, &i, "-seed"), "-seed");
      } else {
         return cli_unknown_arg(argv[i]);
      }
   }

   const dist_spec_t *dist_spec = dist_find_or_exit(dist_name);
   compare_require_distribution(dist_spec);

   if (tree_name) {
      const list_vtable_t *vt = tree_registry_find_or_exit(tree_name);
      compare_require_tree(vt);
      compare_tree(vt, dist_spec, n, seed);
      return 0;
   }

   for (int t = 0; t < tree_registry_count; t++) {
      compare_require_tree(tree_registry_all[t]);
      compare_tree(tree_registry_all[t], dist_spec, n, seed);
   }

   return 0;
}
