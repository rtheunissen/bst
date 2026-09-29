#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "distributions.h"
#include "list.h"

#include <stdio.h>

typedef enum {
#ifdef PERSISTENT
   OP_INSERT_PERSISTENT,
   OP_INSERT_DELETE_PERSISTENT,
#else
   OP_INSERT,
   OP_INSERT_DELETE,
#endif
} operation_type_t;

typedef struct {
   operation_type_t type;
   const char *name;
   u64 scale;
   u64 steps;
} operation_t;

#define OPERATION_ACTION_INSERT LIST_EVENT_INSERT
#define OPERATION_ACTION_DELETE LIST_EVENT_DELETE
typedef list_event_action_t operation_action_t;

typedef struct {
   list_t instance;
   operation_action_t action;
   u64 position;
   u64 size_before;
   u64 size_after;
   u64 step_count;
} operation_step_t;

typedef list_event_t operation_event_t;

enum { operation_default_count = 2 };

static inline bool operation_name_is_component(const char *name) {
   return bst_name_is_path_component(name);
}

static inline u64 operation_effective_scale_value(u64 scale) {
   return scale == 0 ? 1 : scale;
}

// step_count is the 1-based operation number after advancing to this step.
static inline bool operation_insert_delete_should_insert(u64 scale, u64 size,
                                                         u64 step_count) {
   invariant(step_count > 0);
   scale = operation_effective_scale_value(scale);
   invariant(size <= scale);
   invariant(size <= step_count);
   if (size == 0 || size < scale) {
      return true;
   }
   if (step_count <= scale) {
      return false;
   }
   u64 steps_after_scale = suffix_size_after_prefix(step_count, scale);
   return steps_after_scale % 2 == 0;
}

int operation_defaults(operation_t operations[operation_default_count]);
int operation_defaults_for_name(operation_t operations[operation_default_count],
                                const char *name);
operation_t operation_default_mixed(void);
void operation_require(const operation_t *op);
const operation_t *operation_find(const operation_t *operations, int count,
                                  const char *name);
const operation_t *operation_find_or_exit(const operation_t *operations,
                                          int count, const char *name);
bool operation_inserts_only(const operation_t *op);
void operation_fprint_names(FILE *file, const char *label,
                            const operation_t *operations, int count);
operation_step_t operation_step(const operation_t *op, list_t instance,
                                distribution_t *dist, u64 *step_count);
list_t operation_update(const operation_t *op, list_t instance,
                        distribution_t *dist, u64 *step_count);

#ifdef BST_FAST_BENCHMARK
typedef struct {
   u64 insert_count;
   u64 insert_delete_count;
   bool insert_delete_next_is_insert;
} operation_position_chunk_t;

static inline bool
operation_insert_delete_next_is_insert_after_fill(const operation_t *op,
                                                  u64 completed) {
   operation_require(op);
   invariant(!operation_inserts_only(op));
   invariant(completed >= op->scale);
   u64 steps_after_scale = suffix_size_after_prefix(completed, op->scale);
   return steps_after_scale % 2 != 0;
}

static inline operation_position_chunk_t
operation_position_chunk_plan(const operation_t *op, u64 completed, u64 count) {
   operation_require(op);
   invariant(completed <= op->steps);
   invariant(count <= op->steps - completed);

   if (operation_inserts_only(op)) {
      return (operation_position_chunk_t){.insert_count = count};
   }

   u64 target = completed + count;
   u64 insert_count = 0;
   if (completed < op->scale) {
      u64 insert_target = MIN(target, op->scale);
      insert_count = sequence_split_inputs_require(completed, insert_target);
   }

   u64 alternating_start = completed + insert_count;
   u64 insert_delete_count =
       sequence_split_inputs_require(alternating_start, target);
   bool insert_delete_next_is_insert = false;
   if (insert_delete_count > 0) {
      insert_delete_next_is_insert =
          operation_insert_delete_next_is_insert_after_fill(op,
                                                            alternating_start);
   }

   return (operation_position_chunk_t){
       .insert_count = insert_count,
       .insert_delete_count = insert_delete_count,
       .insert_delete_next_is_insert = insert_delete_next_is_insert,
   };
}

u64 operation_prepare_insert_positions(const operation_t *op,
                                       distribution_t *dist, u64 size,
                                       u64 step_count, u64 *positions,
                                       u64 count);
u64 operation_prepare_insert_delete_positions(const operation_t *op,
                                              distribution_t *dist, u64 size,
                                              u64 step_count, u64 *positions,
                                              u64 count);
u64 operation_prepare_events(const operation_t *op, distribution_t *dist,
                             u64 size, u64 step_count,
                             operation_event_t *events, u64 count);

static inline tree_t operation_event_snapshot(list_tree_fn tree_fn,
                                              list_t *instance) {
   tree_t *tree = tree_fn(instance);
   tree_t snapshot = *tree;
   tree_share(snapshot.root);
   return snapshot;
}

static inline void operation_event_release_snapshot(tree_t snapshot) {
   tree_release(snapshot.root);
}

static inline void operation_apply_prepared_insert_event_fn(
    list_tree_fn tree_fn, list_insert_fn insert_fn, list_t *instance,
    const operation_event_t *event) {
   invariant(insert_fn != NULL);
   invariant(instance != NULL);
   invariant(event != NULL);
   invariant(event->action == OPERATION_ACTION_INSERT);

#ifdef PERSISTENT
   invariant(tree_fn != NULL);
   tree_t snapshot = operation_event_snapshot(tree_fn, instance);
#else
   (void)tree_fn;
#endif
   insert_fn(instance, event->position, 0);
#ifdef PERSISTENT
   operation_event_release_snapshot(snapshot);
#endif
}

static inline void operation_apply_prepared_insert_event_vt(
    const list_vtable_t *vt, list_t *instance, const operation_event_t *event) {
   invariant(vt != NULL);
   operation_apply_prepared_insert_event_fn(vt->tree_fn, vt->insert_fn,
                                            instance, event);
}

static inline void operation_apply_prepared_delete_event_fn(
    list_tree_fn tree_fn, list_delete_fn delete_fn, list_t *instance,
    const operation_event_t *event) {
   invariant(delete_fn != NULL);
   invariant(instance != NULL);
   invariant(event != NULL);
   invariant(event->action == OPERATION_ACTION_DELETE);

#ifdef PERSISTENT
   invariant(tree_fn != NULL);
   tree_t snapshot = operation_event_snapshot(tree_fn, instance);
   (void)delete_fn(instance, event->position);
   operation_event_release_snapshot(snapshot);
#else
   (void)tree_fn;
   (void)delete_fn(instance, event->position);
#endif
}

static inline void operation_apply_prepared_event_fn(
    list_tree_fn tree_fn, list_insert_fn insert_fn, list_delete_fn delete_fn,
    list_t *instance, const operation_event_t *event) {
   invariant(event != NULL);

   switch (event->action) {
   case OPERATION_ACTION_INSERT:
      operation_apply_prepared_insert_event_fn(tree_fn, insert_fn, instance,
                                               event);
      break;
   case OPERATION_ACTION_DELETE:
      operation_apply_prepared_delete_event_fn(tree_fn, delete_fn, instance,
                                               event);
      break;
   default:
      bst_unreachable();
   }
}

static inline void
operation_apply_prepared_event_vt(const list_vtable_t *vt, list_t *instance,
                                  const operation_event_t *event) {
   invariant(vt != NULL);
   operation_apply_prepared_event_fn(vt->tree_fn, vt->insert_fn, vt->delete_fn,
                                     instance, event);
}

static inline void
operation_apply_prepared_event(list_t *instance,
                               const operation_event_t *event) {
   operation_apply_prepared_event_vt(instance->vt, instance, event);
}

static inline list_t operation_apply_event(const operation_t *op,
                                           list_t instance,
                                           const operation_event_t *event,
                                           u64 *step_count) {
   invariant(op != NULL);
   invariant(step_count != NULL);
   (void)op;

   operation_apply_prepared_event(&instance, event);
   *step_count = size_after_inserting_one(*step_count);
   return instance;
}
#endif

#endif // OPERATIONS_H
