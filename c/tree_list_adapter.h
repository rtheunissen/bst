#ifndef TREE_LIST_ADAPTER_H
#define TREE_LIST_ADAPTER_H

#include "list.h"
#include "weight.h"

#include <stddef.h>

static inline tree_t tree_list_make_tree(node_t *root, u64 size) {
   tree_t tree = {.root = root, .size = size};
   tree_handle_require(tree);
   return tree;
}

static inline tree_t tree_list_make_sized_tree(node_t *root, u64 size) {
   tree_t tree = tree_list_make_tree(root, size);
   tree_sized_handle_require(tree);
   return tree;
}

static inline list_t tree_list_make(void *impl, const list_vtable_t *vt) {
   invariant(impl != NULL);
   list_vtable_require(vt);
   return (list_t){.impl = impl, .vt = vt};
}

static inline void *tree_list_alloc_impl(size_t impl_size, size_t tree_offset,
                                         node_t *root, u64 size,
                                         bool verify_size) {
   invariant(impl_size > 0);
   invariant(tree_offset <= impl_size);
   invariant(sizeof(tree_t) <= impl_size - tree_offset);
   invariant(tree_offset % _Alignof(tree_t) == 0);
   tree_t initial = verify_size ? tree_list_make_sized_tree(root, size)
                                : tree_list_make_tree(root, size);
#ifdef DEBUG
   if (verify_size) {
      tree_debug_require_size(root, size);
   }
#endif
   void *impl = bst_calloc(1, impl_size);
   tree_t *tree = (tree_t *)((char *)impl + tree_offset);
   *tree = initial;
   return impl;
}

static inline size_t tree_list_impl_tree_offset(list_t *self, void *orig,
                                                size_t impl_size,
                                                tree_t *tree) {
   list_require(self);
   invariant(orig != NULL);
   invariant(orig == self->impl);
   invariant(tree != NULL);
   invariant(impl_size >= sizeof(tree_t));

   uintptr_t base = (uintptr_t)orig;
   uintptr_t tree_addr = (uintptr_t)tree;
   invariant(tree_addr >= base);
   invariant(tree_addr - base <= SIZE_MAX);
   size_t tree_offset = (size_t)(tree_addr - base);
   invariant(tree_offset <= impl_size - sizeof(tree_t));
   invariant(tree_addr % _Alignof(tree_t) == 0);
   tree_handle_require(*tree);
   return tree_offset;
}

static inline list_t tree_list_clone_impl(list_t *self, void *orig,
                                          size_t impl_size, tree_t *tree) {
   size_t tree_offset = tree_list_impl_tree_offset(self, orig, impl_size, tree);
   tree_sized_handle_require(*tree);
   void *copy = bst_malloc(impl_size);
   bst_array_copy(copy, orig, 1, impl_size);
   tree_t *copy_tree = (tree_t *)((char *)copy + tree_offset);
   *copy_tree = tree_clone(tree);
   return tree_list_make(copy, self->vt);
}

static inline void tree_list_free_impl(list_t *self, void *orig,
                                       size_t impl_size, tree_t *tree) {
   (void)tree_list_impl_tree_offset(self, orig, impl_size, tree);
   free_nodes(tree->root);
   free(orig);
   self->impl = NULL;
}

static inline u64 tree_list_join_size(u64 left, u64 right) {
   return size_sum(left, right);
}

static inline u64 tree_list_split_right_size(u64 size, u64 left) {
   return sequence_split_inputs_require(left, size);
}

static inline void tree_list_verify_tree(tree_t tree) {
   tree_sized_handle_require(tree);
   invariant(tree_verify_size(tree.root, tree.size) == tree.size);
}

static inline tree_t *tree_list_require_sized_tree(tree_t *tree) {
   invariant(tree != NULL);
   tree_sized_handle_require(*tree);
   return tree;
}

static inline void tree_list_inserted(tree_t *tree) {
   invariant(tree != NULL);
   tree->size = size_after_inserting_one(tree->size);
   tree_handle_require(*tree);
}

static inline void tree_list_sized_inserted(tree_t *tree) {
   tree_list_inserted(tree);
   tree_sized_handle_require(*tree);
}

static inline void tree_list_deleted(tree_t *tree) {
   invariant(tree != NULL);
   tree->size = size_after_removing_one(tree->size);
   tree_handle_require(*tree);
}

static inline void tree_list_sized_deleted(tree_t *tree) {
   tree_list_deleted(tree);
   tree_sized_handle_require(*tree);
}

static inline void tree_list_require_split(list_t *self, u64 i, list_t *out_l,
                                           list_t *out_r) {
   list_require(self);
   invariant(out_l != NULL);
   invariant(out_r != NULL);
   invariant(out_l != self);
   invariant(out_r != self);
   invariant(out_l != out_r);
   (void)sequence_split_inputs_require(i, list_size(self));
}

static inline void tree_list_require_join_handles(list_t *self, list_t *other) {
   list_require(self);
   list_require(other);
   invariant(other != self);
   invariant(other->impl != self->impl);
   invariant(self->vt == other->vt);
}

static inline void tree_list_require_join(list_t *self, list_t *other) {
   tree_list_require_join_handles(self, other);
#ifndef PERSISTENT
   tree_t *left = list_tree(self);
   tree_t *right = list_tree(other);
   invariant(left != NULL);
   invariant(right != NULL);
   invariant(left->root == NULL || right->root == NULL ||
             left->root != right->root);
#ifdef DEBUG
   assert_bst(!tree_roots_overlap(left, right));
#endif
#endif
}

static inline void
tree_list_require_prepared_split_result(list_t *self, u64 i, u64 original_size,
                                        u64 left_size, u64 right_size,
                                        list_t *out_l, list_t *out_r) {
   list_require(self);
   invariant(out_l != NULL);
   invariant(out_r != NULL);
   invariant(out_l != self);
   invariant(out_r != self);
   invariant(out_l != out_r);
   invariant(left_size == i);
   invariant(right_size == tree_list_split_right_size(original_size, i));
}

static inline void tree_list_require_split_result(list_t *self, u64 i,
                                                  u64 left_size, u64 right_size,
                                                  list_t *out_l,
                                                  list_t *out_r) {
   tree_list_require_split(self, i, out_l, out_r);
   u64 size = list_size(self);
   tree_list_require_prepared_split_result(self, i, size, left_size, right_size,
                                           out_l, out_r);
}

static inline void tree_list_require_join_result(list_t *self, list_t *other,
                                                 u64 size) {
   tree_list_require_join_handles(self, other);
   u64 left_size = list_size(self);
   u64 right_size = list_size(other);
   invariant(size == tree_list_join_size(left_size, right_size));
}

static inline void tree_list_require_prepared_join_result(list_t *self,
                                                          list_t *other) {
   // Some join algorithms destructively re-rank input roots while building the
   // returned root. At that point final allocation validates the output tree;
   // the still-available generic check is handle identity and vtable agreement.
   tree_list_require_join_handles(self, other);
}

static inline tree_join_roots_t tree_list_prepare_join_roots(tree_t *left,
                                                             tree_t *right) {
   return tree_prepare_join_roots(left, right);
}

#define TREE_LIST_ALLOC(TYPE, ROOT, SIZE)                                      \
   ((TYPE *)tree_list_alloc_impl(sizeof(TYPE), offsetof(TYPE, tree), ROOT,     \
                                 SIZE, true))

#define TREE_LIST_ALLOC_UNCHECKED(TYPE, ROOT, SIZE)                            \
   ((TYPE *)tree_list_alloc_impl(sizeof(TYPE), offsetof(TYPE, tree), ROOT,     \
                                 SIZE, false))

#ifdef PERSISTENT
#define TREE_LIST_CLEAR_MOVED_SOURCE(TYPE) ((void)0)
#else
#define TREE_LIST_CLEAR_MOVED_SOURCE(TYPE)                                     \
   do {                                                                        \
      TYPE *tree_list_source = (TYPE *)self->impl;                             \
      tree_list_source->tree = (tree_t){0};                                    \
   } while (0)
#endif

#define TREE_LIST_DEFINE_TREE_ACCESSOR(TYPE, PREFIX)                           \
   static tree_t *PREFIX##_vt_tree(list_t *self) {                             \
      list_require(self);                                                      \
      TYPE *tree_list_self = (TYPE *)self->impl;                               \
      tree_handle_require(tree_list_self->tree);                               \
      return &tree_list_self->tree;                                            \
   }

#define TREE_LIST_DEFINE_SIZED_TREE_ACCESSOR(TYPE, PREFIX)                     \
   static tree_t *PREFIX##_vt_tree(list_t *self) {                             \
      list_require(self);                                                      \
      TYPE *tree_list_self = (TYPE *)self->impl;                               \
      return tree_list_require_sized_tree(&tree_list_self->tree);              \
   }

#define TREE_LIST_VTABLE(NAME, PREFIX)                                         \
   {                                                                           \
       .name = NAME,                                                           \
       .new_fn = PREFIX##_vt_new,                                              \
       .clone_fn = PREFIX##_vt_clone,                                          \
       .free_fn = PREFIX##_vt_free,                                            \
       .tree_fn = PREFIX##_vt_tree,                                            \
       .size_fn = PREFIX##_vt_size,                                            \
       .each_fn = PREFIX##_vt_each,                                            \
       .verify_fn = PREFIX##_vt_verify,                                        \
       .select_fn = PREFIX##_vt_select,                                        \
       .update_fn = PREFIX##_vt_update,                                        \
       .insert_fn = PREFIX##_vt_insert,                                        \
       .delete_fn = PREFIX##_vt_delete,                                        \
       .apply_inserts_fn = PREFIX##_vt_apply_inserts,                          \
       .apply_insert_delete_fn = PREFIX##_vt_apply_insert_delete,              \
       .apply_events_fn = PREFIX##_vt_apply_events,                            \
       .time_inserts_fn = PREFIX##_vt_time_inserts,                            \
       .time_insert_delete_fn = PREFIX##_vt_time_insert_delete,                \
       .split_fn = PREFIX##_vt_split,                                          \
       .join_fn = PREFIX##_vt_join,                                            \
   }

#define TREE_LIST_SPLIT_RESULT(TYPE, LEFT_ROOT, LEFT_SIZE, RIGHT_ROOT,         \
                               RIGHT_SIZE)                                     \
   do {                                                                        \
      tree_list_require_split_result(self, i, LEFT_SIZE, RIGHT_SIZE, out_l,    \
                                     out_r);                                   \
      TYPE *tree_list_left = TREE_LIST_ALLOC(TYPE, LEFT_ROOT, LEFT_SIZE);      \
      TYPE *tree_list_right = TREE_LIST_ALLOC(TYPE, RIGHT_ROOT, RIGHT_SIZE);   \
      *out_l = tree_list_make(tree_list_left, self->vt);                       \
      *out_r = tree_list_make(tree_list_right, self->vt);                      \
   } while (0)

#define TREE_LIST_SPLIT_RESULT_PREPARED(TYPE, ORIGINAL_SIZE, LEFT_ROOT,        \
                                        LEFT_SIZE, RIGHT_ROOT, RIGHT_SIZE)     \
   do {                                                                        \
      tree_list_require_prepared_split_result(                                 \
          self, i, ORIGINAL_SIZE, LEFT_SIZE, RIGHT_SIZE, out_l, out_r);        \
      TYPE *tree_list_left = TREE_LIST_ALLOC(TYPE, LEFT_ROOT, LEFT_SIZE);      \
      TYPE *tree_list_right = TREE_LIST_ALLOC(TYPE, RIGHT_ROOT, RIGHT_SIZE);   \
      *out_l = tree_list_make(tree_list_left, self->vt);                       \
      *out_r = tree_list_make(tree_list_right, self->vt);                      \
      TREE_LIST_CLEAR_MOVED_SOURCE(TYPE);                                      \
   } while (0)

#define TREE_LIST_JOIN_RESULT(TYPE, ROOT, SIZE)                                \
   do {                                                                        \
      tree_list_require_prepared_join_result(self, other);                     \
      TYPE *tree_list_joined = TREE_LIST_ALLOC(TYPE, ROOT, SIZE);              \
      return tree_list_make(tree_list_joined, self->vt);                       \
   } while (0)

#define TREE_LIST_SPLIT_RESULT_FIELD(TYPE, FIELD, VALUE, LEFT_ROOT, LEFT_SIZE, \
                                     RIGHT_ROOT, RIGHT_SIZE)                   \
   do {                                                                        \
      tree_list_require_split_result(self, i, LEFT_SIZE, RIGHT_SIZE, out_l,    \
                                     out_r);                                   \
      TYPE *tree_list_left = TREE_LIST_ALLOC(TYPE, LEFT_ROOT, LEFT_SIZE);      \
      tree_list_left->FIELD = VALUE;                                           \
      TYPE *tree_list_right = TREE_LIST_ALLOC(TYPE, RIGHT_ROOT, RIGHT_SIZE);   \
      tree_list_right->FIELD = VALUE;                                          \
      *out_l = tree_list_make(tree_list_left, self->vt);                       \
      *out_r = tree_list_make(tree_list_right, self->vt);                      \
   } while (0)

#define TREE_LIST_SPLIT_RESULT_FIELD_PREPARED(TYPE, ORIGINAL_SIZE, FIELD,      \
                                              VALUE, LEFT_ROOT, LEFT_SIZE,     \
                                              RIGHT_ROOT, RIGHT_SIZE)          \
   do {                                                                        \
      tree_list_require_prepared_split_result(                                 \
          self, i, ORIGINAL_SIZE, LEFT_SIZE, RIGHT_SIZE, out_l, out_r);        \
      TYPE *tree_list_left = TREE_LIST_ALLOC(TYPE, LEFT_ROOT, LEFT_SIZE);      \
      tree_list_left->FIELD = VALUE;                                           \
      TYPE *tree_list_right = TREE_LIST_ALLOC(TYPE, RIGHT_ROOT, RIGHT_SIZE);   \
      tree_list_right->FIELD = VALUE;                                          \
      *out_l = tree_list_make(tree_list_left, self->vt);                       \
      *out_r = tree_list_make(tree_list_right, self->vt);                      \
      TREE_LIST_CLEAR_MOVED_SOURCE(TYPE);                                      \
   } while (0)

#define TREE_LIST_JOIN_RESULT_FIELD(TYPE, FIELD, VALUE, ROOT, SIZE)            \
   do {                                                                        \
      tree_list_require_prepared_join_result(self, other);                     \
      TYPE *tree_list_joined = TREE_LIST_ALLOC(TYPE, ROOT, SIZE);              \
      tree_list_joined->FIELD = VALUE;                                         \
      return tree_list_make(tree_list_joined, self->vt);                       \
   } while (0)

#ifdef PERSISTENT
#define TREE_LIST_EVENT_SNAPSHOT(PREFIX)                                       \
   tree_t tree_list_event_snapshot = *PREFIX##_vt_tree(self);                  \
   tree_share(tree_list_event_snapshot.root)
#define TREE_LIST_EVENT_RELEASE() tree_release(tree_list_event_snapshot.root)
#else
#define TREE_LIST_EVENT_SNAPSHOT(PREFIX) ((void)0)
#define TREE_LIST_EVENT_RELEASE() ((void)0)
#endif

#define TREE_LIST_APPLY_INSERT_POSITIONS(PREFIX, POSITIONS, COUNT)             \
   do {                                                                        \
      for (u64 tree_list_i = 0; tree_list_i < (COUNT); tree_list_i++) {        \
         TREE_LIST_EVENT_SNAPSHOT(PREFIX);                                     \
         PREFIX##_vt_insert(self, (POSITIONS)[tree_list_i], 0);                \
         TREE_LIST_EVENT_RELEASE();                                            \
      }                                                                        \
   } while (0)

#define TREE_LIST_APPLY_INSERT_DELETE_POSITIONS(PREFIX, POSITIONS, COUNT,      \
                                                NEXT_IS_INSERT)                \
   do {                                                                        \
      if (NEXT_IS_INSERT) {                                                    \
         u64 tree_list_i = 0;                                                  \
         for (; tree_list_i + 1 < (COUNT); tree_list_i += 2) {                 \
            {                                                                  \
               TREE_LIST_EVENT_SNAPSHOT(PREFIX);                               \
               PREFIX##_vt_insert(self, (POSITIONS)[tree_list_i], 0);          \
               TREE_LIST_EVENT_RELEASE();                                      \
            }                                                                  \
            {                                                                  \
               TREE_LIST_EVENT_SNAPSHOT(PREFIX);                               \
               (void)PREFIX##_vt_delete(self, (POSITIONS)[tree_list_i + 1]);   \
               TREE_LIST_EVENT_RELEASE();                                      \
            }                                                                  \
         }                                                                     \
         if (tree_list_i < (COUNT)) {                                          \
            TREE_LIST_EVENT_SNAPSHOT(PREFIX);                                  \
            PREFIX##_vt_insert(self, (POSITIONS)[tree_list_i], 0);             \
            TREE_LIST_EVENT_RELEASE();                                         \
         }                                                                     \
      } else {                                                                 \
         u64 tree_list_i = 0;                                                  \
         for (; tree_list_i + 1 < (COUNT); tree_list_i += 2) {                 \
            {                                                                  \
               TREE_LIST_EVENT_SNAPSHOT(PREFIX);                               \
               (void)PREFIX##_vt_delete(self, (POSITIONS)[tree_list_i]);       \
               TREE_LIST_EVENT_RELEASE();                                      \
            }                                                                  \
            {                                                                  \
               TREE_LIST_EVENT_SNAPSHOT(PREFIX);                               \
               PREFIX##_vt_insert(self, (POSITIONS)[tree_list_i + 1], 0);      \
               TREE_LIST_EVENT_RELEASE();                                      \
            }                                                                  \
         }                                                                     \
         if (tree_list_i < (COUNT)) {                                          \
            TREE_LIST_EVENT_SNAPSHOT(PREFIX);                                  \
            (void)PREFIX##_vt_delete(self, (POSITIONS)[tree_list_i]);          \
            TREE_LIST_EVENT_RELEASE();                                         \
         }                                                                     \
      }                                                                        \
   } while (0)

#define TREE_LIST_DEFINE_EVENT_APPLIERS(PREFIX)                                \
   static tree_t *PREFIX##_vt_tree(list_t *self);                              \
   static void PREFIX##_vt_insert(list_t *self, u64 i, u64 x);                 \
   static u64 PREFIX##_vt_delete(list_t *self, u64 i);                         \
   static void PREFIX##_vt_apply_inserts(list_t *self, const u64 *positions,   \
                                         u64 count) {                          \
      list_require(self);                                                      \
      invariant(positions != NULL || count == 0);                              \
      TREE_LIST_APPLY_INSERT_POSITIONS(PREFIX, positions, count);              \
   }                                                                           \
   static void PREFIX##_vt_apply_insert_delete(                                \
       list_t *self, const u64 *positions, u64 count, bool next_is_insert) {   \
      list_require(self);                                                      \
      invariant(positions != NULL || count == 0);                              \
      TREE_LIST_APPLY_INSERT_DELETE_POSITIONS(PREFIX, positions, count,        \
                                              next_is_insert);                 \
   }                                                                           \
   static u64 PREFIX##_vt_time_inserts(list_t *self, const u64 *positions,     \
                                       u64 count) {                            \
      list_require(self);                                                      \
      invariant(positions != NULL || count == 0);                              \
      u64 tree_list_start = monotonic_time_ns();                               \
      TREE_LIST_APPLY_INSERT_POSITIONS(PREFIX, positions, count);              \
      return elapsed_time_ns_since_nonzero(tree_list_start);                   \
   }                                                                           \
   static u64 PREFIX##_vt_time_insert_delete(                                  \
       list_t *self, const u64 *positions, u64 insert_count,                   \
       u64 insert_delete_count, bool next_is_insert) {                         \
      list_require(self);                                                      \
      invariant(positions != NULL);                                            \
      const u64 *tree_list_insert_delete_positions = positions + insert_count; \
      if (insert_count > 0 && insert_delete_count > 0) {                       \
         u64 tree_list_start = monotonic_time_ns();                            \
         TREE_LIST_APPLY_INSERT_POSITIONS(PREFIX, positions, insert_count);    \
         TREE_LIST_APPLY_INSERT_DELETE_POSITIONS(                              \
             PREFIX, tree_list_insert_delete_positions, insert_delete_count,   \
             next_is_insert);                                                  \
         return elapsed_time_ns_since_nonzero(tree_list_start);                \
      }                                                                        \
      if (insert_count > 0) {                                                  \
         u64 tree_list_start = monotonic_time_ns();                            \
         TREE_LIST_APPLY_INSERT_POSITIONS(PREFIX, positions, insert_count);    \
         return elapsed_time_ns_since_nonzero(tree_list_start);                \
      }                                                                        \
      invariant(insert_delete_count > 0);                                      \
      u64 tree_list_start = monotonic_time_ns();                               \
      TREE_LIST_APPLY_INSERT_DELETE_POSITIONS(                                 \
          PREFIX, tree_list_insert_delete_positions, insert_delete_count,      \
          next_is_insert);                                                     \
      return elapsed_time_ns_since_nonzero(tree_list_start);                   \
   }                                                                           \
   static void PREFIX##_vt_apply_events(                                       \
       list_t *self, const list_event_t *events, u64 count) {                  \
      list_require(self);                                                      \
      invariant(events != NULL || count == 0);                                 \
      for (u64 tree_list_i = 0; tree_list_i < count; tree_list_i++) {          \
         switch (events[tree_list_i].action) {                                 \
         case LIST_EVENT_INSERT: {                                             \
            TREE_LIST_EVENT_SNAPSHOT(PREFIX);                                  \
            PREFIX##_vt_insert(self, events[tree_list_i].position, 0);         \
            TREE_LIST_EVENT_RELEASE();                                         \
            break;                                                             \
         }                                                                     \
         case LIST_EVENT_DELETE: {                                             \
            TREE_LIST_EVENT_SNAPSHOT(PREFIX);                                  \
            (void)PREFIX##_vt_delete(self, events[tree_list_i].position);      \
            TREE_LIST_EVENT_RELEASE();                                         \
            break;                                                             \
         }                                                                     \
         default:                                                              \
            bst_unreachable();                                                 \
         }                                                                     \
      }                                                                        \
   }

#define TREE_LIST_COMMON_FUNCTIONS(TYPE, PREFIX, INIT_BODY)                    \
   static void PREFIX##_vt_insert(list_t *self, u64 i, u64 x);                 \
   static u64 PREFIX##_vt_delete(list_t *self, u64 i);                         \
   static list_t PREFIX##_vt_new(const list_vtable_t *vt) {                    \
      list_vtable_require(vt);                                                 \
      TYPE *t = (TYPE *)bst_calloc(1, sizeof(TYPE));                           \
      INIT_BODY;                                                               \
      return tree_list_make(t, vt);                                            \
   }                                                                           \
   TREE_LIST_DEFINE_SIZED_TREE_ACCESSOR(TYPE, PREFIX)                          \
   static list_t PREFIX##_vt_clone(list_t *self) {                             \
      list_require(self);                                                      \
      TYPE *o = (TYPE *)self->impl;                                            \
      return tree_list_clone_impl(self, o, sizeof(TYPE), &o->tree);            \
   }                                                                           \
   static void PREFIX##_vt_free(list_t *self) {                                \
      list_require(self);                                                      \
      TYPE *t = (TYPE *)self->impl;                                            \
      tree_list_require_sized_tree(&t->tree);                                  \
      tree_list_free_impl(self, t, sizeof(TYPE), &t->tree);                    \
   }                                                                           \
   static u64 PREFIX##_vt_size(list_t *self) {                                 \
      list_require(self);                                                      \
      TYPE *tree_list_self = (TYPE *)self->impl;                               \
      tree_t *tree_list_tree =                                                 \
          tree_list_require_sized_tree(&tree_list_self->tree);                 \
      return tree_list_tree->size;                                             \
   }                                                                           \
   static void PREFIX##_vt_each(list_t *self, void (*v)(u64, void *),          \
                                void *c) {                                     \
      list_require(self);                                                      \
      invariant(v != NULL);                                                    \
      TYPE *tree_list_self = (TYPE *)self->impl;                               \
      tree_t *tree_list_tree =                                                 \
          tree_list_require_sized_tree(&tree_list_self->tree);                 \
      inorder(tree_list_tree->root, v, c);                                     \
   }                                                                           \
   static u64 PREFIX##_vt_select(list_t *self, u64 i) {                        \
      list_require(self);                                                      \
      TYPE *tree_list_self = (TYPE *)self->impl;                               \
      tree_t *tree_list_tree =                                                 \
          tree_list_require_sized_tree(&tree_list_self->tree);                 \
      return tree_lookup(tree_list_tree, i);                                   \
   }                                                                           \
   static void PREFIX##_vt_update(list_t *self, u64 i, u64 x) {                \
      list_require(self);                                                      \
      TYPE *tree_list_self = (TYPE *)self->impl;                               \
      tree_t *tree_list_tree =                                                 \
          tree_list_require_sized_tree(&tree_list_self->tree);                 \
      tree_update_base(tree_list_tree, i, x);                                  \
   }                                                                           \
   static void PREFIX##_vt_apply_inserts(list_t *self, const u64 *positions,   \
                                         u64 count) {                          \
      list_require(self);                                                      \
      invariant(positions != NULL || count == 0);                              \
      TREE_LIST_APPLY_INSERT_POSITIONS(PREFIX, positions, count);              \
   }                                                                           \
   static void PREFIX##_vt_apply_insert_delete(                                \
       list_t *self, const u64 *positions, u64 count, bool next_is_insert) {   \
      list_require(self);                                                      \
      invariant(positions != NULL || count == 0);                              \
      TREE_LIST_APPLY_INSERT_DELETE_POSITIONS(PREFIX, positions, count,        \
                                              next_is_insert);                 \
   }                                                                           \
   static u64 PREFIX##_vt_time_inserts(list_t *self, const u64 *positions,     \
                                       u64 count) {                            \
      list_require(self);                                                      \
      invariant(positions != NULL || count == 0);                              \
      u64 tree_list_start = monotonic_time_ns();                               \
      TREE_LIST_APPLY_INSERT_POSITIONS(PREFIX, positions, count);              \
      return elapsed_time_ns_since_nonzero(tree_list_start);                   \
   }                                                                           \
   static u64 PREFIX##_vt_time_insert_delete(                                  \
       list_t *self, const u64 *positions, u64 insert_count,                   \
       u64 insert_delete_count, bool next_is_insert) {                         \
      list_require(self);                                                      \
      invariant(positions != NULL);                                            \
      const u64 *tree_list_insert_delete_positions = positions + insert_count; \
      if (insert_count > 0 && insert_delete_count > 0) {                       \
         u64 tree_list_start = monotonic_time_ns();                            \
         TREE_LIST_APPLY_INSERT_POSITIONS(PREFIX, positions, insert_count);    \
         TREE_LIST_APPLY_INSERT_DELETE_POSITIONS(                              \
             PREFIX, tree_list_insert_delete_positions, insert_delete_count,   \
             next_is_insert);                                                  \
         return elapsed_time_ns_since_nonzero(tree_list_start);                \
      }                                                                        \
      if (insert_count > 0) {                                                  \
         u64 tree_list_start = monotonic_time_ns();                            \
         TREE_LIST_APPLY_INSERT_POSITIONS(PREFIX, positions, insert_count);    \
         return elapsed_time_ns_since_nonzero(tree_list_start);                \
      }                                                                        \
      invariant(insert_delete_count > 0);                                      \
      u64 tree_list_start = monotonic_time_ns();                               \
      TREE_LIST_APPLY_INSERT_DELETE_POSITIONS(                                 \
          PREFIX, tree_list_insert_delete_positions, insert_delete_count,      \
          next_is_insert);                                                     \
      return elapsed_time_ns_since_nonzero(tree_list_start);                   \
   }                                                                           \
   static void PREFIX##_vt_apply_events(                                       \
       list_t *self, const list_event_t *events, u64 count) {                  \
      list_require(self);                                                      \
      invariant(events != NULL || count == 0);                                 \
      for (u64 tree_list_i = 0; tree_list_i < count; tree_list_i++) {          \
         switch (events[tree_list_i].action) {                                 \
         case LIST_EVENT_INSERT: {                                             \
            TREE_LIST_EVENT_SNAPSHOT(PREFIX);                                  \
            PREFIX##_vt_insert(self, events[tree_list_i].position, 0);         \
            TREE_LIST_EVENT_RELEASE();                                         \
            break;                                                             \
         }                                                                     \
         case LIST_EVENT_DELETE: {                                             \
            TREE_LIST_EVENT_SNAPSHOT(PREFIX);                                  \
            (void)PREFIX##_vt_delete(self, events[tree_list_i].position);      \
            TREE_LIST_EVENT_RELEASE();                                         \
            break;                                                             \
         }                                                                     \
         default:                                                              \
            bst_unreachable();                                                 \
         }                                                                     \
      }                                                                        \
   }

#define TREE_LIST_SIMPLE_FUNCTIONS(TYPE, PREFIX)                               \
   TREE_LIST_COMMON_FUNCTIONS(TYPE, PREFIX, (void)t)

#define TREE_VT_SIMPLE(TYPE, PREFIX) TREE_LIST_SIMPLE_FUNCTIONS(TYPE, PREFIX)
#define SIMPLE_VT_BODY(TYPE, PREFIX) TREE_LIST_SIMPLE_FUNCTIONS(TYPE, PREFIX)

#define WB_VT_BODY(TYPE, PREFIX, WB_INIT, INSF, DELF, SPLF, JOINF, VERF)       \
   TREE_LIST_COMMON_FUNCTIONS(TYPE, PREFIX, t->wb = WB_INIT)                   \
   static void PREFIX##_vt_verify(list_t *self) {                              \
      list_require(self);                                                      \
      VERF                                                                     \
   }                                                                           \
   static void PREFIX##_vt_insert(list_t *self, u64 i, u64 x) {                \
      list_require(self);                                                      \
      TYPE *tree_list_self = (TYPE *)self->impl;                               \
      u64 tree_list_size = tree_list_self->tree.size;                          \
      u64 tree_list_inserted_size = tree_insert_inputs_require(                \
          tree_list_self->tree.root, i, tree_list_size);                       \
      INSF;                                                                    \
      invariant(tree_list_self->tree.size == tree_list_inserted_size);         \
      tree_list_require_sized_tree(&tree_list_self->tree);                     \
   }                                                                           \
   static u64 PREFIX##_vt_delete(list_t *self, u64 i) {                        \
      list_require(self);                                                      \
      TYPE *tree_list_self = (TYPE *)self->impl;                               \
      (void)tree_delete_inputs_require(tree_list_self->tree.root, i,           \
                                       tree_list_self->tree.size);             \
      DELF                                                                     \
   }                                                                           \
   static void PREFIX##_vt_split(list_t *self, u64 i, list_t *out_l,           \
                                 list_t *out_r) {                              \
      tree_list_require_split(self, i, out_l, out_r);                          \
      SPLF                                                                     \
   }                                                                           \
   static list_t PREFIX##_vt_join(list_t *self, list_t *other) {               \
      tree_list_require_join(self, other);                                     \
      JOINF                                                                    \
   }

#define TREE_LIST_WB_SPLIT(TYPE, SPLIT_FN)                                     \
   do {                                                                        \
      TYPE *tree_list_self = (TYPE *)self->impl;                               \
      u64 tree_list_size = tree_list_self->tree.size;                          \
      tree_share(tree_list_self->tree.root);                                   \
      tree_split_result_t tree_list_split = SPLIT_FN(                          \
          tree_list_self->wb, tree_list_self->tree.root, i, tree_list_size);   \
      u64 tree_list_right_size =                                               \
          tree_list_split_right_size(tree_list_size, i);                       \
      tree_list_require_prepared_split_result(                                 \
          self, i, tree_list_size, i, tree_list_right_size, out_l, out_r);     \
      TYPE *tree_list_left = TREE_LIST_ALLOC(TYPE, tree_list_split.left, i);   \
      tree_list_left->wb = tree_list_self->wb;                                 \
      TYPE *tree_list_right =                                                  \
          TREE_LIST_ALLOC(TYPE, tree_list_split.right, tree_list_right_size);  \
      tree_list_right->wb = tree_list_self->wb;                                \
      *out_l = tree_list_make(tree_list_left, self->vt);                       \
      *out_r = tree_list_make(tree_list_right, self->vt);                      \
      TREE_LIST_CLEAR_MOVED_SOURCE(TYPE);                                      \
   } while (0)

#define TREE_LIST_WB_JOIN(TYPE, JOIN_FN)                                       \
   do {                                                                        \
      TYPE *tree_list_self = (TYPE *)self->impl;                               \
      TYPE *tree_list_other = (TYPE *)other->impl;                             \
      weight_balance_require_equal(tree_list_self->wb, tree_list_other->wb);   \
      u64 tree_list_joined_size = tree_list_join_size(                         \
          tree_list_self->tree.size, tree_list_other->tree.size);              \
      tree_list_require_join_result(self, other, tree_list_joined_size);       \
      tree_join_roots_t tree_list_roots = tree_list_prepare_join_roots(        \
          &tree_list_self->tree, &tree_list_other->tree);                      \
      node_t *tree_list_joined_root = JOIN_FN(                                 \
          tree_list_self->wb, tree_list_roots.left, tree_list_roots.right,     \
          tree_list_self->tree.size, tree_list_other->tree.size);              \
      TYPE *tree_list_joined =                                                 \
          TREE_LIST_ALLOC(TYPE, tree_list_joined_root, tree_list_joined_size); \
      tree_list_joined->wb = tree_list_self->wb;                               \
      return tree_list_make(tree_list_joined, self->vt);                       \
   } while (0)

#define WBST_BU_SPLIT(TYPE) TREE_LIST_WB_SPLIT(TYPE, wbst_bu_split)
#define WBST_BU_JOIN(TYPE) TREE_LIST_WB_JOIN(TYPE, wbst_bu_join)
#define WBST_TD_SPLIT(TYPE) TREE_LIST_WB_SPLIT(TYPE, wbst_td_split)
#define WBST_TD_JOIN(TYPE) TREE_LIST_WB_JOIN(TYPE, wbst_td_join)

#endif // TREE_LIST_ADAPTER_H
