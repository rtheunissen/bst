#ifndef LIST_H
#define LIST_H

#include "bst.h"
#include "time_utils.h"

//
// Abstract list interface (C vtable equivalent of Go's list.List interface)
//
// Each tree implementation provides a vtable with these function pointers.
// The test suite operates on list_t handles, dispatching through the vtable.
//

typedef struct list_vtable list_vtable_t;

typedef struct list {
   void *impl;              // Pointer to the concrete tree struct
   const list_vtable_t *vt; // Pointer to the vtable
} list_t;

typedef enum {
   LIST_EVENT_INSERT,
   LIST_EVENT_DELETE,
} list_event_action_t;

typedef struct {
   list_event_action_t action;
   u64 position;
} list_event_t;

typedef tree_t *(*list_tree_fn)(list_t *self);
typedef void (*list_insert_fn)(list_t *self, u64 i, u64 x);
typedef u64 (*list_delete_fn)(list_t *self, u64 i);
typedef void (*list_apply_inserts_fn)(list_t *self, const u64 *positions,
                                      u64 count);
typedef void (*list_apply_insert_delete_fn)(list_t *self, const u64 *positions,
                                            u64 count, bool next_is_insert);
typedef void (*list_apply_events_fn)(list_t *self, const list_event_t *events,
                                     u64 count);
typedef u64 (*list_time_inserts_fn)(list_t *self, const u64 *positions,
                                    u64 count);
typedef u64 (*list_time_insert_delete_fn)(list_t *self, const u64 *positions,
                                          u64 insert_count,
                                          u64 insert_delete_count,
                                          bool next_is_insert);

struct list_vtable {
   const char *name;

   list_t (*new_fn)(const list_vtable_t *vt);
   list_t (*clone_fn)(list_t *self);
   void (*free_fn)(list_t *self);
   list_tree_fn tree_fn;
   u64 (*size_fn)(list_t *self);
   void (*each_fn)(list_t *self, void (*visit)(u64, void *), void *ctx);
   void (*verify_fn)(list_t *self);
   u64 (*select_fn)(list_t *self, u64 i);
   void (*update_fn)(list_t *self, u64 i, u64 x);
   list_insert_fn insert_fn;
   list_delete_fn delete_fn;
   list_apply_inserts_fn apply_inserts_fn;
   list_apply_insert_delete_fn apply_insert_delete_fn;
   list_apply_events_fn apply_events_fn;
   list_time_inserts_fn time_inserts_fn;
   list_time_insert_delete_fn time_insert_delete_fn;
   void (*split_fn)(list_t *self, u64 i, list_t *out_l, list_t *out_r);
   list_t (*join_fn)(list_t *self, list_t *other);
};

static inline void list_apply_inserts_default(list_t *self,
                                              const u64 *positions, u64 count) {
   invariant(self != NULL);
   invariant(positions != NULL || count == 0);
   for (u64 i = 0; i < count; i++) {
      self->vt->insert_fn(self, positions[i], 0);
   }
}

static inline void
list_apply_events_default(list_t *self, const list_event_t *events, u64 count) {
   invariant(self != NULL);
   invariant(events != NULL || count == 0);
   for (u64 i = 0; i < count; i++) {
      switch (events[i].action) {
      case LIST_EVENT_INSERT:
         self->vt->insert_fn(self, events[i].position, 0);
         break;
      case LIST_EVENT_DELETE:
         (void)self->vt->delete_fn(self, events[i].position);
         break;
      default:
         bst_unreachable();
      }
   }
}

static inline void list_apply_insert_delete_default(list_t *self,
                                                    const u64 *positions,
                                                    u64 count,
                                                    bool next_is_insert) {
   invariant(self != NULL);
   invariant(positions != NULL || count == 0);
   if (next_is_insert) {
      u64 i = 0;
      for (; i + 1 < count; i += 2) {
         self->vt->insert_fn(self, positions[i], 0);
         (void)self->vt->delete_fn(self, positions[i + 1]);
      }
      if (i < count) {
         self->vt->insert_fn(self, positions[i], 0);
      }
   } else {
      u64 i = 0;
      for (; i + 1 < count; i += 2) {
         (void)self->vt->delete_fn(self, positions[i]);
         self->vt->insert_fn(self, positions[i + 1], 0);
      }
      if (i < count) {
         (void)self->vt->delete_fn(self, positions[i]);
      }
   }
}

static inline u64 list_time_inserts_default(list_t *self, const u64 *positions,
                                            u64 count) {
   u64 start = monotonic_time_ns();
   list_apply_inserts_default(self, positions, count);
   return elapsed_time_ns_since_nonzero(start);
}

static inline u64 list_time_insert_delete_default(list_t *self,
                                                  const u64 *positions,
                                                  u64 insert_count,
                                                  u64 insert_delete_count,
                                                  bool next_is_insert) {
   const u64 *insert_delete_positions = positions + insert_count;
   if (insert_count > 0 && insert_delete_count > 0) {
      u64 start = monotonic_time_ns();
      list_apply_inserts_default(self, positions, insert_count);
      list_apply_insert_delete_default(self, insert_delete_positions,
                                       insert_delete_count, next_is_insert);
      return elapsed_time_ns_since_nonzero(start);
   }
   if (insert_count > 0) {
      u64 start = monotonic_time_ns();
      list_apply_inserts_default(self, positions, insert_count);
      return elapsed_time_ns_since_nonzero(start);
   }

   invariant(insert_delete_count > 0);
   u64 start = monotonic_time_ns();
   list_apply_insert_delete_default(self, insert_delete_positions,
                                    insert_delete_count, next_is_insert);
   return elapsed_time_ns_since_nonzero(start);
}

static inline bool list_vtable_name_is_component(const char *name) {
   return bst_name_is_path_component(name);
}

static inline void list_vtable_require(const list_vtable_t *vt) {
   invariant(vt != NULL);
   invariant(list_vtable_name_is_component(vt->name));
   invariant(vt->new_fn != NULL);
   invariant(vt->clone_fn != NULL);
   invariant(vt->free_fn != NULL);
   invariant(vt->tree_fn != NULL);
   invariant(vt->size_fn != NULL);
   invariant(vt->each_fn != NULL);
   invariant(vt->verify_fn != NULL);
   invariant(vt->select_fn != NULL);
   invariant(vt->update_fn != NULL);
   invariant(vt->insert_fn != NULL);
   invariant(vt->delete_fn != NULL);
   invariant(vt->apply_inserts_fn != NULL);
   invariant(vt->apply_insert_delete_fn != NULL);
   invariant(vt->apply_events_fn != NULL);
   invariant(vt->time_inserts_fn != NULL);
   invariant(vt->time_insert_delete_fn != NULL);
   invariant(vt->split_fn != NULL);
   invariant(vt->join_fn != NULL);
}

// ─────────────────────────────────────────────────────────────────────────────
// Convenience dispatchers
// ─────────────────────────────────────────────────────────────────────────────

static inline void list_require(list_t *l) {
   invariant(l != NULL);
   list_vtable_require(l->vt);
   invariant(l->impl != NULL);
}

static inline u64 list_size(list_t *l);
static inline tree_t *list_tree(list_t *l);
static inline void list_each(list_t *l, void (*visit)(u64, void *), void *ctx);
static inline void list_debug_verify(list_t *l);

static inline void list_debug_tree(list_t *l) {
#ifdef DEBUG
   (void)list_tree(l);
#else
   (void)l;
#endif
}

static inline void list_debug_require_split_outputs_disjoint(list_t *left,
                                                             list_t *right) {
#ifdef DEBUG
   tree_t *left_tree = list_tree(left);
   tree_t *right_tree = list_tree(right);
   if (left_tree != NULL && right_tree != NULL) {
      assert_bst(!tree_roots_overlap(left_tree, right_tree));
   }
#else
   (void)left;
   (void)right;
#endif
}

#ifdef DEBUG
typedef struct {
   u64 *values;
   u64 count;
   u64 expected;
} list_debug_capture_t;

static inline void list_debug_sequence_collect(u64 x, void *ctx) {
   list_debug_capture_t *seq = (list_debug_capture_t *)ctx;
   invariant(seq != NULL);
   invariant(seq->values != NULL);
   invariant(seq->count < seq->expected);
   u64 index = seq->count;
   seq->count = size_after_inserting_one(seq->count);
   seq->values[index] = x;
}

static inline u64 *list_debug_capture_sequence(list_t *list, u64 size) {
   list_require(list);
   invariant(list_size(list) == size);
   if (size == 0) {
      return NULL;
   }

   u64 *values = (u64 *)bst_malloc_array(size, sizeof(values[0]));
   list_debug_capture_t seq = {.values = values, .expected = size};
   list->vt->each_fn(list, list_debug_sequence_collect, &seq);
   invariant(seq.count == size);
   invariant(list_size(list) == size);
   return values;
}

typedef struct {
   const u64 *values;
   u64 value_count;
   u64 value_offset;
   u64 actual_offset;
   u64 actual_index;
   u64 compare_count;
   u64 compared;
} list_debug_compare_t;

static inline void list_debug_sequence_compare(u64 x, void *ctx) {
   list_debug_compare_t *cmp = (list_debug_compare_t *)ctx;
   invariant(cmp != NULL);
   if (cmp->actual_index >= cmp->actual_offset &&
       cmp->compared < cmp->compare_count) {
      invariant(cmp->values != NULL);
      invariant(cmp->value_offset <= cmp->value_count);
      u64 remaining_values =
          suffix_size_after_prefix(cmp->value_count, cmp->value_offset);
      invariant(cmp->compared < remaining_values);
      u64 expected_index = size_sum(cmp->value_offset, cmp->compared);
      assert_bst(x == cmp->values[expected_index]);
      cmp->compared = size_after_inserting_one(cmp->compared);
   }
   cmp->actual_index = size_after_inserting_one(cmp->actual_index);
}

static inline void
list_debug_require_sequence_slice(list_t *actual, u64 actual_offset,
                                  const u64 *values, u64 value_count,
                                  u64 value_offset, u64 compare_count) {
   u64 actual_size = list_size(actual);
   invariant(actual_offset <= actual_size);
   u64 actual_remaining = suffix_size_after_prefix(actual_size, actual_offset);
   invariant(compare_count <= actual_remaining);
   invariant(value_offset <= value_count);
   u64 value_remaining = suffix_size_after_prefix(value_count, value_offset);
   invariant(compare_count <= value_remaining);
   invariant(compare_count == 0 || values != NULL);

   list_debug_compare_t cmp = {
       .values = values,
       .value_count = value_count,
       .value_offset = value_offset,
       .actual_offset = actual_offset,
       .compare_count = compare_count,
   };
   list_each(actual, list_debug_sequence_compare, &cmp);
   invariant(cmp.actual_index == actual_size);
   invariant(cmp.compared == compare_count);
}

static inline void list_debug_require_sequence(list_t *actual,
                                               const u64 *values, u64 size) {
   list_debug_require_sequence_slice(actual, 0, values, size, 0, size);
}

static inline void list_debug_require_same_sequence(list_t *expected,
                                                    list_t *actual) {
   u64 size = list_size(expected);
   invariant(list_size(actual) == size);

   u64 *values = list_debug_capture_sequence(expected, size);
   list_debug_require_sequence(actual, values, size);
   free(values);
}

typedef struct {
   u64 index;
   u64 size;
   u64 current;
   u64 value;
   bool found;
} list_debug_value_at_t;

static inline void list_debug_value_at_visit(u64 x, void *ctx) {
   list_debug_value_at_t *at = (list_debug_value_at_t *)ctx;
   invariant(at != NULL);
   invariant(at->current < at->size);
   if (at->current == at->index) {
      invariant(!at->found);
      at->value = x;
      at->found = true;
   }
   at->current = size_after_inserting_one(at->current);
}

static inline u64 list_debug_value_at(list_t *list, u64 index) {
   u64 size = list_size(list);
   sequence_rank_inputs_require(index, size);
   list_debug_value_at_t at = {.index = index, .size = size};
   list_each(list, list_debug_value_at_visit, &at);
   invariant(at.current == size);
   invariant(at.found);
   return at.value;
}

static inline void list_debug_require_updated_sequence(list_t *actual,
                                                       u64 index, u64 value,
                                                       const u64 *before,
                                                       u64 size) {
   sequence_rank_inputs_require(index, size);
   invariant(list_size(actual) == size);
   assert_bst(list_debug_value_at(actual, index) == value);
   u64 suffix_size = suffix_size_after_rank(size, index);
   u64 suffix_offset = size_after_inserting_one(index);
   list_debug_require_sequence_slice(actual, 0, before, size, 0, index);
   list_debug_require_sequence_slice(actual, suffix_offset, before, size,
                                     suffix_offset, suffix_size);
}

static inline void list_debug_require_inserted_sequence(list_t *actual,
                                                        u64 index, u64 value,
                                                        const u64 *before,
                                                        u64 size) {
   u64 inserted_size = sequence_insert_inputs_require(index, size);
   invariant(list_size(actual) == inserted_size);
   assert_bst(list_debug_value_at(actual, index) == value);
   u64 suffix_size = sequence_split_inputs_require(index, size);
   u64 inserted_suffix_offset = size_after_inserting_one(index);
   list_debug_require_sequence_slice(actual, 0, before, size, 0, index);
   list_debug_require_sequence_slice(actual, inserted_suffix_offset, before,
                                     size, index, suffix_size);
}

static inline void list_debug_require_deleted_sequence(list_t *actual,
                                                       u64 index, u64 deleted,
                                                       const u64 *before,
                                                       u64 size) {
   u64 deleted_size = sequence_delete_inputs_require(index, size);
   invariant(list_size(actual) == deleted_size);
   assert_bst(deleted == before[index]);
   u64 suffix_size = suffix_size_after_rank(size, index);
   u64 original_suffix_offset = size_after_inserting_one(index);
   list_debug_require_sequence_slice(actual, 0, before, size, 0, index);
   list_debug_require_sequence_slice(actual, index, before, size,
                                     original_suffix_offset, suffix_size);
}

static inline void
list_debug_require_split_sequence(list_t *source, list_t *left, list_t *right,
                                  u64 split_size, const u64 *before, u64 size) {
   u64 right_size = sequence_split_inputs_require(split_size, size);
   invariant(list_size(source) == size);
   invariant(list_size(left) == split_size);
   invariant(list_size(right) == right_size);
   list_debug_require_sequence(source, before, size);
   list_debug_require_sequence_slice(left, 0, before, size, 0, split_size);
   list_debug_require_sequence_slice(right, 0, before, size, split_size,
                                     right_size);
}

static inline void
list_debug_require_joined_sequence(list_t *left, list_t *right, list_t *joined,
                                   const u64 *left_before, u64 left_size,
                                   const u64 *right_before, u64 right_size) {
   invariant(list_size(left) == left_size);
   invariant(list_size(right) == right_size);
   invariant(list_size(joined) == size_sum(left_size, right_size));
   list_debug_require_sequence(left, left_before, left_size);
   list_debug_require_sequence(right, right_before, right_size);
   list_debug_require_sequence_slice(joined, 0, left_before, left_size, 0,
                                     left_size);
   list_debug_require_sequence_slice(joined, left_size, right_before,
                                     right_size, 0, right_size);
}
#endif

static inline list_t list_new(const list_vtable_t *vt) {
   list_vtable_require(vt);
   list_t l = vt->new_fn(vt);
   invariant(l.vt == vt);
   invariant(l.impl != NULL);
   invariant(list_size(&l) == 0);
   list_debug_tree(&l);
   list_debug_verify(&l);
   return l;
}

#if defined(DEBUG) && !defined(PERSISTENT)
static inline void
list_debug_require_physical_clone_trees_disjoint(list_t *original,
                                                 list_t *clone) {
   tree_t *original_tree = list_tree(original);
   tree_t *clone_tree = list_tree(clone);
   if (original_tree != NULL && clone_tree != NULL) {
      assert_bst(!tree_roots_overlap(original_tree, clone_tree));
   }
}
#endif

static inline list_t list_clone(list_t *l) {
   list_require(l);
   invariant(l->vt->clone_fn != NULL);
   u64 size = list_size(l);
   list_debug_tree(l);
   list_debug_verify(l);
#ifdef DEBUG
   u64 *debug_values = list_debug_capture_sequence(l, size);
#endif
   list_t clone = l->vt->clone_fn(l);
   invariant(list_size(l) == size);
   invariant(clone.vt == l->vt);
   invariant(clone.impl != NULL);
   invariant(clone.impl != l->impl);
   invariant(list_size(&clone) == size);
   list_debug_tree(&clone);
   list_debug_verify(l);
   list_debug_verify(&clone);
#ifdef DEBUG
   list_debug_require_sequence(l, debug_values, size);
   list_debug_require_sequence(&clone, debug_values, size);
#ifndef PERSISTENT
   list_debug_require_physical_clone_trees_disjoint(l, &clone);
#endif
   free(debug_values);
#endif
   return clone;
}

static inline void list_free(list_t *l) {
   if (l == NULL) {
      return;
   }
   if (l->vt == NULL) {
      invariant(l->impl == NULL);
      return;
   }
   invariant(l->impl != NULL);
   list_vtable_require(l->vt);
   list_debug_verify(l);
   const list_vtable_t *vt = l->vt;
   vt->free_fn(l);
   invariant(l->impl == NULL);
   invariant(l->vt == vt);
   l->impl = NULL;
   l->vt = NULL;
}

static inline tree_t *list_tree(list_t *l) {
   list_require(l);
   invariant(l->vt->tree_fn != NULL);
   tree_t *tree = l->vt->tree_fn(l);
   if (tree != NULL) {
      tree_handle_require(*tree);
      invariant(tree->size == list_size(l));
   }
   return tree;
}

static inline u64 list_size(list_t *l) {
   list_require(l);
   invariant(l->vt->size_fn != NULL);
   return l->vt->size_fn(l);
}

static inline tree_t *list_tree_fast(list_t *l) {
   return l->vt->tree_fn(l);
}

static inline u64 list_size_fast(list_t *l) {
   return l->vt->size_fn(l);
}

static inline void list_insert_fast(list_t *l, u64 i, u64 x) {
   l->vt->insert_fn(l, i, x);
}

static inline u64 list_delete_fast(list_t *l, u64 i) {
   return l->vt->delete_fn(l, i);
}

typedef struct {
   void (*visit)(u64, void *);
   void *ctx;
   u64 count;
   u64 expected;
} list_each_count_t;

static inline void list_each_count_visit(u64 x, void *ctx) {
   list_each_count_t *count = (list_each_count_t *)ctx;
   invariant(count != NULL);
   invariant(count->visit != NULL);
   invariant(count->count < count->expected);
   count->count = size_after_inserting_one(count->count);
   count->visit(x, count->ctx);
}

static inline void list_each(list_t *l, void (*visit)(u64, void *), void *ctx) {
   list_require(l);
   invariant(l->vt->each_fn != NULL);
   invariant(visit != NULL);
   u64 size = list_size(l);
   list_debug_tree(l);
   list_debug_verify(l);
#ifdef DEBUG
   u64 *debug_values = list_debug_capture_sequence(l, size);
#endif
   list_each_count_t count = {
       .visit = visit,
       .ctx = ctx,
       .expected = size,
   };
   l->vt->each_fn(l, list_each_count_visit, &count);
   invariant(list_size(l) == size);
   invariant(count.count == size);
   list_debug_tree(l);
   list_debug_verify(l);
#ifdef DEBUG
   list_debug_compare_t cmp = {
       .values = debug_values,
       .value_count = size,
       .value_offset = 0,
       .actual_offset = 0,
       .compare_count = size,
   };
   l->vt->each_fn(l, list_debug_sequence_compare, &cmp);
   invariant(cmp.actual_index == size);
   invariant(cmp.compared == size);
   free(debug_values);
#endif
}

typedef struct {
   u64 count;
   u64 expected;
} list_verify_count_t;

static inline void list_verify_count(u64 x, void *ctx) {
   (void)x;
   list_verify_count_t *c = (list_verify_count_t *)ctx;
   invariant(c != NULL);
   invariant(c->count < c->expected);
   c->count = size_after_inserting_one(c->count);
}

static inline void list_verify(list_t *l) {
   list_require(l);
   u64 expected = list_size(l);
   l->vt->verify_fn(l);
   invariant(list_size(l) == expected);

   list_verify_count_t count = {0, expected};
   l->vt->each_fn(l, list_verify_count, &count);
   invariant(list_size(l) == expected);
   invariant(count.count == expected);

   tree_t *tree = list_tree(l);
   if (tree != NULL) {
      invariant(expected == tree->size);
   }
}

static inline void list_debug_verify(list_t *l) {
#ifdef DEBUG
   list_require(l);
   u64 expected = list_size(l);
   tree_t *tree = list_tree(l);
   if (tree != NULL) {
      invariant(tree->size == expected);
   }
   l->vt->verify_fn(l);
   invariant(list_size(l) == expected);
#else
   (void)l;
#endif
}

static inline u64 list_select(list_t *l, u64 i) {
   list_require(l);
   invariant(l->vt->select_fn != NULL);
   u64 size = list_size(l);
   sequence_rank_inputs_require(i, size);
#ifdef DEBUG
   u64 *debug_values = list_debug_capture_sequence(l, size);
   u64 expected = debug_values[i];
#endif
   list_debug_tree(l);
   u64 x = l->vt->select_fn(l, i);
   invariant(list_size(l) == size);
   list_debug_tree(l);
   list_debug_verify(l);
#ifdef DEBUG
   assert_bst(x == expected);
   list_debug_require_sequence(l, debug_values, size);
   free(debug_values);
#endif
   return x;
}

static inline void list_update(list_t *l, u64 i, u64 x) {
   list_require(l);
   invariant(l->vt->update_fn != NULL);
   u64 size = list_size(l);
   sequence_rank_inputs_require(i, size);
#ifdef DEBUG
   u64 *debug_values = list_debug_capture_sequence(l, size);
#endif
   list_debug_tree(l);
   l->vt->update_fn(l, i, x);
   invariant(list_size(l) == size);
   list_debug_tree(l);
   list_debug_verify(l);
#ifdef DEBUG
   list_debug_require_updated_sequence(l, i, x, debug_values, size);
   free(debug_values);
#endif
}

static inline void list_insert(list_t *l, u64 i, u64 x) {
   list_require(l);
   invariant(l->vt->insert_fn != NULL);
   u64 size = list_size(l);
   u64 inserted_size = sequence_insert_inputs_require(i, size);
#ifdef DEBUG
   u64 *debug_values = list_debug_capture_sequence(l, size);
#endif
   list_debug_tree(l);
   l->vt->insert_fn(l, i, x);
   invariant(list_size(l) == inserted_size);
   list_debug_tree(l);
   list_debug_verify(l);
#ifdef DEBUG
   list_debug_require_inserted_sequence(l, i, x, debug_values, size);
   free(debug_values);
#endif
}

static inline u64 list_delete(list_t *l, u64 i) {
   list_require(l);
   invariant(l->vt->delete_fn != NULL);
   u64 size = list_size(l);
   u64 deleted_size = sequence_delete_inputs_require(i, size);
#ifdef DEBUG
   u64 *debug_values = list_debug_capture_sequence(l, size);
#endif
   list_debug_tree(l);
   u64 x = l->vt->delete_fn(l, i);
   invariant(list_size(l) == deleted_size);
   list_debug_tree(l);
   list_debug_verify(l);
#ifdef DEBUG
   list_debug_require_deleted_sequence(l, i, x, debug_values, size);
   free(debug_values);
#endif
   return x;
}

static inline void list_split(list_t *l, u64 i, list_t *out_l, list_t *out_r) {
   list_require(l);
   invariant(l->vt->split_fn != NULL);
   invariant(out_l != NULL);
   invariant(out_r != NULL);
   invariant(out_l != l);
   invariant(out_r != l);
   invariant(out_l != out_r);
   u64 size = list_size(l);
   u64 right_size = sequence_split_inputs_require(i, size);
   *out_l = (list_t){0};
   *out_r = (list_t){0};
#ifdef DEBUG
   u64 *debug_values = list_debug_capture_sequence(l, size);
#endif
#ifdef PERSISTENT
   l->vt->split_fn(l, i, out_l, out_r);
   invariant(out_l->impl != l->impl);
   invariant(out_r->impl != l->impl);
#else
   list_t copy = list_clone(l);
   copy.vt->split_fn(&copy, i, out_l, out_r);
   invariant(out_l->impl != copy.impl);
   invariant(out_r->impl != copy.impl);
   tree_t *tree = list_tree(&copy);
   if (tree != NULL) {
      *tree = (tree_t){0};
   }
   list_free(&copy);
#endif
   invariant(list_size(l) == size);
   invariant(out_l->vt == l->vt);
   invariant(out_r->vt == l->vt);
   invariant(out_l->impl != NULL);
   invariant(out_r->impl != NULL);
   invariant(out_l->impl != l->impl);
   invariant(out_r->impl != l->impl);
   invariant(out_l->impl != out_r->impl);
   invariant(list_size(out_l) == i);
   invariant(list_size(out_r) == right_size);
   list_debug_tree(out_l);
   list_debug_tree(out_r);
   list_debug_require_split_outputs_disjoint(out_l, out_r);
   list_debug_verify(l);
   list_debug_verify(out_l);
   list_debug_verify(out_r);
#ifdef DEBUG
   list_debug_require_split_sequence(l, out_l, out_r, i, debug_values, size);
   free(debug_values);
#endif
}

static inline list_t list_join(list_t *l, list_t *other) {
   list_require(l);
   list_require(other);
   invariant(l->vt->join_fn != NULL);
   invariant(l->vt == other->vt);
   invariant(other == l || other->impl != l->impl);
   u64 left_size = list_size(l);
   u64 right_size = list_size(other);
   void *left_impl = l->impl;
   void *right_impl = other->impl;
   u64 joined_size = size_sum(left_size, right_size);
#ifdef DEBUG
   u64 *left_values = list_debug_capture_sequence(l, left_size);
   u64 *right_values = list_debug_capture_sequence(other, right_size);
#endif
#ifdef PERSISTENT
   list_t joined;
   if (other == l) {
      list_t right = list_clone(other);
      joined = l->vt->join_fn(l, &right);
      invariant(joined.impl != right.impl);
      list_free(&right);
   } else {
      joined = l->vt->join_fn(l, other);
   }
#else
   list_t left = list_clone(l);
   list_t right = list_clone(other);
   tree_t *left_tree = list_tree(&left);
   tree_t *right_tree = list_tree(&right);
   list_t joined = left.vt->join_fn(&left, &right);
   invariant(joined.impl != left.impl);
   invariant(joined.impl != right.impl);
   if (left_tree != NULL) {
      *left_tree = (tree_t){0};
   }
   if (right_tree != NULL) {
      *right_tree = (tree_t){0};
   }
   list_free(&left);
   list_free(&right);
#endif
   invariant(l->impl == left_impl);
   invariant(other->impl == right_impl);
   invariant(list_size(l) == left_size);
   invariant(list_size(other) == right_size);
   invariant(joined.vt == l->vt);
   invariant(joined.impl != NULL);
   invariant(joined.impl != left_impl);
   invariant(joined.impl != right_impl);
   invariant(list_size(&joined) == joined_size);
   list_debug_tree(&joined);
   list_debug_verify(l);
   list_debug_verify(other);
   list_debug_verify(&joined);
#ifdef DEBUG
   list_debug_require_joined_sequence(l, other, &joined, left_values, left_size,
                                      right_values, right_size);
   free(left_values);
   free(right_values);
#endif
   return joined;
}

#endif // LIST_H
