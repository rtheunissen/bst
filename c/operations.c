#include "operations.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void operation_slice_require(const operation_t *operations, int count);

static const char *operation_type_name(operation_type_t type) {
   switch (type) {
#ifdef PERSISTENT
   case OP_INSERT_PERSISTENT:
      return "InsertPersistent";
   case OP_INSERT_DELETE_PERSISTENT:
      return "InsertDeletePersistent";
#else
   case OP_INSERT:
      return "Insert";
   case OP_INSERT_DELETE:
      return "InsertDelete";
#endif
   default:
      bst_unreachable();
   }
}

static bool operation_type_inserts_only(operation_type_t type) {
   switch (type) {
#ifdef PERSISTENT
   case OP_INSERT_PERSISTENT:
      return true;
   case OP_INSERT_DELETE_PERSISTENT:
      return false;
#else
   case OP_INSERT:
      return true;
   case OP_INSERT_DELETE:
      return false;
#endif
   default:
      bst_unreachable();
   }
}

int operation_defaults(operation_t operations[operation_default_count]) {
   invariant(operations != NULL);
#ifdef PERSISTENT
   operations[0] = (operation_t){
       .type = OP_INSERT_PERSISTENT,
       .name = "InsertPersistent",
       .scale = 10000000,
       .steps = 10000000,
   };
   operations[1] = (operation_t){
       .type = OP_INSERT_DELETE_PERSISTENT,
       .name = "InsertDeletePersistent",
       .scale = 1000000,
       .steps = 10000000,
   };
#else
   operations[0] = (operation_t){
       .type = OP_INSERT,
       .name = "Insert",
       .scale = 10000000,
       .steps = 10000000,
   };
   operations[1] = (operation_t){
       .type = OP_INSERT_DELETE,
       .name = "InsertDelete",
       .scale = 1000000,
       .steps = 10000000,
   };
#endif
   operation_slice_require(operations, operation_default_count);
   return operation_default_count;
}

int operation_defaults_for_name(operation_t operations[operation_default_count],
                                const char *name) {
   invariant(operations != NULL);

   int count = operation_defaults(operations);
   if (name == NULL) {
      return count;
   }

   const operation_t *selected =
       operation_find_or_exit(operations, count, name);
   operations[0] = *selected;
   return 1;
}

operation_t operation_default_mixed(void) {
   operation_t operations[operation_default_count];
   int count = operation_defaults(operations);
   operation_t *selected = NULL;
   for (int i = 0; i < count; i++) {
      operation_t *op = &operations[i];
      operation_require(op);
      if (operation_inserts_only(op)) {
         continue;
      }
      invariant(selected == NULL);
      selected = op;
   }

   invariant(selected != NULL);
   return *selected;
}

void operation_require(const operation_t *op) {
   invariant(op != NULL);
   invariant(op->name != NULL);
   invariant(operation_name_is_component(op->name));
   invariant(strcmp(op->name, operation_type_name(op->type)) == 0);
}

static void operation_slice_require(const operation_t *operations, int count) {
   invariant(count >= 0);
   invariant(count == 0 || operations != NULL);

   for (int i = 0; i < count; i++) {
      operation_require(&operations[i]);
      for (int j = i + 1; j < count; j++) {
         operation_require(&operations[j]);
         invariant(strcmp(operations[i].name, operations[j].name) != 0);
      }
   }
}

static bool operation_name_is_lookupable(const char *name) {
   return name != NULL && name[0] != '\0' && operation_name_is_component(name);
}

static const operation_t *
operation_find_lookupable(const operation_t *operations, int count,
                          const char *name) {
   invariant(count >= 0);
   invariant(count == 0 || operations != NULL);
   invariant(operation_name_is_lookupable(name));
   for (int i = 0; i < count; i++) {
      if (strcmp(operations[i].name, name) == 0) {
         return &operations[i];
      }
   }
   return NULL;
}

const operation_t *operation_find(const operation_t *operations, int count,
                                  const char *name) {
   operation_slice_require(operations, count);
   if (!operation_name_is_lookupable(name)) {
      return NULL;
   }
   return operation_find_lookupable(operations, count, name);
}

const operation_t *operation_find_or_exit(const operation_t *operations,
                                          int count, const char *name) {
   operation_slice_require(operations, count);

   if (name == NULL) {
      fprintf(stderr, "operation name is null\n");
      operation_fprint_names(stderr, "operations", operations, count);
      exit(2);
   }
   if (name[0] == '\0') {
      fprintf(stderr, "operation name is empty\n");
      operation_fprint_names(stderr, "operations", operations, count);
      exit(2);
   }
   if (!operation_name_is_component(name)) {
      fprintf(stderr, "operation name is not a path component\n");
      operation_fprint_names(stderr, "operations", operations, count);
      exit(2);
   }

   const operation_t *op = operation_find_lookupable(operations, count, name);
   if (op == NULL) {
      fprintf(stderr, "unknown operation for this build: %s\n", name);
      operation_fprint_names(stderr, "operations", operations, count);
      exit(2);
   }

   return op;
}

bool operation_inserts_only(const operation_t *op) {
   operation_require(op);
   return operation_type_inserts_only(op->type);
}

void operation_fprint_names(FILE *file, const char *label,
                            const operation_t *operations, int count) {
   invariant(file != NULL);
   invariant(label != NULL);
   operation_slice_require(operations, count);

   fprintf(file, "%s:", label);
   for (int i = 0; i < count; i++) {
      fprintf(file, " %s", operations[i].name);
   }
   fprintf(file, "\n");
}

static void operation_require_inserted(u64 before, u64 index, list_t *after) {
   u64 inserted_size = sequence_insert_inputs_require(index, before);
   invariant(list_size(after) == inserted_size);
}

static void operation_require_deleted(u64 before, u64 index, list_t *after) {
   u64 deleted_size = sequence_delete_inputs_require(index, before);
   invariant(list_size(after) == deleted_size);
}

static u64 operation_insert_index(distribution_t *dist, list_t *instance) {
   u64 size = list_size(instance);
   return dist_less_than(dist, sequence_insert_position_count(size));
}

static u64 operation_insert_at_distribution(list_t *instance,
                                            distribution_t *dist) {
   u64 index = operation_insert_index(dist, instance);
   list_insert(instance, index, 0);
   return index;
}

static u64 operation_delete_at_distribution(list_t *instance,
                                            distribution_t *dist) {
   u64 size = list_size(instance);
   invariant(size > 0);
   u64 i = dist_less_than(dist, size);
   u64 position = rank_from_end(size, i);
   list_delete(instance, position);
   return position;
}

static void operation_action_require(operation_action_t action) {
   switch (action) {
   case OPERATION_ACTION_INSERT:
   case OPERATION_ACTION_DELETE:
      break;
   default:
      bst_unreachable();
   }
}

#ifdef BST_FAST_BENCHMARK
static u64 operation_prepare_insert_position(distribution_t *dist, u64 size) {
   return dist_less_than(dist, sequence_insert_position_count(size));
}

static operation_event_t operation_prepare_insert_event(distribution_t *dist,
                                                        u64 size) {
   return (operation_event_t){
       .action = OPERATION_ACTION_INSERT,
       .position = operation_prepare_insert_position(dist, size),
   };
}

static operation_event_t operation_prepare_delete_event(distribution_t *dist,
                                                        u64 size) {
   u64 i = dist_less_than(dist, size);
   return (operation_event_t){
       .action = OPERATION_ACTION_DELETE,
       .position = rank_from_end(size, i),
   };
}

u64 operation_prepare_insert_positions(const operation_t *op,
                                       distribution_t *dist, u64 size,
                                       u64 step_count, u64 *positions,
                                       u64 count) {
   operation_require(op);
   invariant(operation_inserts_only(op));
   invariant(dist != NULL);
   invariant(positions != NULL || count == 0);

   u64 current_size = size;
   u64 current_step = step_count;
   for (u64 i = 0; i < count; i++) {
      u64 next_step = size_after_inserting_one(current_step);
      invariant(next_step <= op->steps);
      positions[i] = operation_prepare_insert_position(dist, current_size);
      current_size = size_after_inserting_one(current_size);
      current_step = next_step;
   }

   return current_size;
}

u64 operation_prepare_insert_delete_positions(const operation_t *op,
                                              distribution_t *dist, u64 size,
                                              u64 step_count, u64 *positions,
                                              u64 count) {
   operation_require(op);
   invariant(!operation_inserts_only(op));
   invariant(dist != NULL);
   invariant(positions != NULL || count == 0);

   u64 current_size = size;
   u64 current_step = step_count;
   for (u64 i = 0; i < count; i++) {
      u64 next_step = size_after_inserting_one(current_step);
      invariant(next_step <= op->steps);
      if (operation_insert_delete_should_insert(op->scale, current_size,
                                                next_step)) {
         positions[i] = operation_prepare_insert_position(dist, current_size);
         current_size = size_after_inserting_one(current_size);
      } else {
         operation_event_t event =
             operation_prepare_delete_event(dist, current_size);
         positions[i] = event.position;
         current_size = size_after_removing_one(current_size);
      }
      current_step = next_step;
   }

   return current_size;
}

u64 operation_prepare_events(const operation_t *op, distribution_t *dist,
                             u64 size, u64 step_count,
                             operation_event_t *events, u64 count) {
   operation_require(op);
   invariant(dist != NULL);
   invariant(events != NULL || count == 0);

   u64 current_size = size;
   u64 current_step = step_count;
   switch (op->type) {
#ifdef PERSISTENT
   case OP_INSERT_PERSISTENT:
#else
   case OP_INSERT:
#endif
      for (u64 i = 0; i < count; i++) {
         u64 next_step = size_after_inserting_one(current_step);
         invariant(next_step <= op->steps);
         events[i] = operation_prepare_insert_event(dist, current_size);
         current_size = size_after_inserting_one(current_size);
         current_step = next_step;
      }
      break;

#ifdef PERSISTENT
   case OP_INSERT_DELETE_PERSISTENT:
#else
   case OP_INSERT_DELETE:
#endif
      for (u64 i = 0; i < count; i++) {
         u64 next_step = size_after_inserting_one(current_step);
         invariant(next_step <= op->steps);
         operation_event_t event;
         if (operation_insert_delete_should_insert(op->scale, current_size,
                                                   next_step)) {
            event = operation_prepare_insert_event(dist, current_size);
            current_size = size_after_inserting_one(current_size);
         } else {
            event = operation_prepare_delete_event(dist, current_size);
            current_size = size_after_removing_one(current_size);
         }
         operation_action_require(event.action);
         events[i] = event;
         current_step = next_step;
      }
      break;

   default:
      bst_unreachable();
   }

   return current_size;
}
#endif

static bool operation_should_insert(const operation_t *op, list_t *instance,
                                    u64 step_count) {
   operation_require(op);
   return operation_insert_delete_should_insert(op->scale, list_size(instance),
                                                step_count);
}

static void operation_step_require_result(const operation_step_t *step,
                                          u64 size_before,
                                          u64 step_count_before) {
   invariant(step != NULL);
   operation_action_require(step->action);
   invariant(step->size_before == size_before);
   invariant(step->step_count == size_after_inserting_one(step_count_before));
   list_t instance = step->instance;
   invariant(step->size_after == list_size(&instance));
   switch (step->action) {
   case OPERATION_ACTION_INSERT:
      invariant(step->size_after == sequence_insert_inputs_require(
                                        step->position, step->size_before));
      invariant(step->position < step->size_after);
      break;
   case OPERATION_ACTION_DELETE:
      invariant(step->size_after == sequence_delete_inputs_require(
                                        step->position, step->size_before));
      break;
   default:
      bst_unreachable();
   }
}

operation_step_t operation_step(const operation_t *op, list_t instance,
                                distribution_t *dist, u64 *step_count) {
   operation_require(op);
   invariant(dist != NULL);
   invariant(step_count != NULL);
   u64 before = list_size(&instance);
   u64 step_count_before = *step_count;
   invariant(step_count_before < op->steps);
   u64 next_step_count = size_after_inserting_one(step_count_before);
   operation_step_t step = {
       .instance = instance,
       .size_before = before,
       .step_count = next_step_count,
   };

   switch (op->type) {
#ifdef PERSISTENT
   case OP_INSERT_PERSISTENT: {
      list_t clone = list_clone(&instance);
      step.action = OPERATION_ACTION_INSERT;
      step.position = operation_insert_at_distribution(&clone, dist);
      operation_require_inserted(before, step.position, &clone);
      list_free(&instance);
      step.instance = clone;
      break;
   }
   case OP_INSERT_DELETE_PERSISTENT: {
      list_t clone = list_clone(&instance);
      bool insert = operation_should_insert(op, &clone, next_step_count);
      if (insert) {
         step.action = OPERATION_ACTION_INSERT;
         step.position = operation_insert_at_distribution(&clone, dist);
         operation_require_inserted(before, step.position, &clone);
      } else {
         step.action = OPERATION_ACTION_DELETE;
         step.position = operation_delete_at_distribution(&clone, dist);
         operation_require_deleted(before, step.position, &clone);
      }
      list_free(&instance);
      step.instance = clone;
      break;
   }
#else
   case OP_INSERT: {
      step.action = OPERATION_ACTION_INSERT;
      step.position = operation_insert_at_distribution(&instance, dist);
      operation_require_inserted(before, step.position, &instance);
      step.instance = instance;
      break;
   }
   case OP_INSERT_DELETE: {
      bool insert = operation_should_insert(op, &instance, next_step_count);
      if (insert) {
         step.action = OPERATION_ACTION_INSERT;
         step.position = operation_insert_at_distribution(&instance, dist);
         operation_require_inserted(before, step.position, &instance);
      } else {
         step.action = OPERATION_ACTION_DELETE;
         step.position = operation_delete_at_distribution(&instance, dist);
         operation_require_deleted(before, step.position, &instance);
      }
      step.instance = instance;
      break;
   }
#endif
   default:
      bst_unreachable();
   }
   step.size_after = list_size(&step.instance);
   operation_step_require_result(&step, before, step_count_before);
   *step_count = next_step_count;
   return step;
}

#ifdef BST_FAST_BENCHMARK
static list_t operation_update_fast(const operation_t *op, list_t instance,
                                    distribution_t *dist, u64 *step_count) {
   operation_event_t event;
   (void)operation_prepare_events(op, dist, list_size_fast(&instance),
                                  *step_count, &event, 1);
   return operation_apply_event(op, instance, &event, step_count);
}
#endif

list_t operation_update(const operation_t *op, list_t instance,
                        distribution_t *dist, u64 *step_count) {
#ifdef BST_FAST_BENCHMARK
   return operation_update_fast(op, instance, dist, step_count);
#else
   return operation_step(op, instance, dist, step_count).instance;
#endif
}
