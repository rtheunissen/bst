#include "reference_list.h"

#include "random.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
   u64 *data;
   u64 len;
   u64 cap;
} reference_t;

static reference_t *reference_alloc(void) {
   return (reference_t *)bst_calloc(1, sizeof(reference_t));
}

static u64 *reference_alloc_data(u64 len) {
   if (len == 0) {
      return NULL;
   }

   return (u64 *)bst_malloc_array(len, sizeof(u64));
}

static void reference_verify(reference_t *ref) {
   invariant(ref != NULL);
   invariant(ref->len <= ref->cap);
   invariant(ref->cap <= bst_array_max_count(sizeof(ref->data[0])));
   invariant((ref->data == NULL) == (ref->cap == 0));
}

static reference_t reference_of_length(u64 len) {
   reference_t ref = {
       .data = reference_alloc_data(len),
       .len = len,
       .cap = len,
   };
   reference_verify(&ref);
   return ref;
}

static list_t reference_make(reference_t *ref, const list_vtable_t *vt) {
   reference_verify(ref);
   list_vtable_require(vt);
   return (list_t){.impl = ref, .vt = vt};
}

static reference_t reference_from_range(const u64 *src, u64 len) {
   invariant(len == 0 || src != NULL);
   reference_t ref = reference_of_length(len);
   bst_array_copy(ref.data, src, len, sizeof(ref.data[0]));
   reference_verify(&ref);
   return ref;
}

static reference_t reference_joined(reference_t *left, reference_t *right) {
   reference_verify(left);
   reference_verify(right);

   u64 total = size_sum(left->len, right->len);
   reference_t joined = reference_of_length(total);
   bst_array_copy(joined.data, left->data, left->len, sizeof(joined.data[0]));
   u64 *right_data = right->len == 0 ? NULL : &joined.data[left->len];
   bst_array_copy(right_data, right->data, right->len, sizeof(joined.data[0]));
   reference_verify(&joined);
   return joined;
}

static void reference_grow(reference_t *ref, u64 needed) {
   invariant(ref != NULL);

   if (needed <= ref->cap) {
      return;
   }

   u64 new_cap = bst_array_next_cap(ref->cap, needed, 8, sizeof(ref->data[0]));
   invariant(new_cap >= needed);

   u64 *data =
       ref->data == NULL
           ? (u64 *)bst_malloc_array(new_cap, sizeof(ref->data[0]))
           : (u64 *)bst_realloc_array(ref->data, new_cap, sizeof(ref->data[0]));
   ref->data = data;
   ref->cap = new_cap;
   reference_verify(ref);
}

static void reference_destroy(reference_t *ref) {
   invariant(ref != NULL);
   reference_verify(ref);
   free(ref->data);
   ref->data = NULL;
   ref->len = 0;
   ref->cap = 0;
   reference_verify(ref);
}

static u64 reference_size(reference_t *ref) {
   reference_verify(ref);
   return ref->len;
}

static u64 reference_select(reference_t *ref, u64 i) {
   reference_verify(ref);
   sequence_rank_inputs_require(i, ref->len);
   return ref->data[i];
}

static void reference_update(reference_t *ref, u64 i, u64 x) {
   reference_verify(ref);
   sequence_rank_inputs_require(i, ref->len);
   ref->data[i] = x;
   invariant(ref->data[i] == x);
   reference_verify(ref);
}

static void reference_insert(reference_t *ref, u64 i, u64 x) {
   reference_verify(ref);
   u64 old_len = ref->len;
   u64 new_len = sequence_insert_inputs_require(i, old_len);
   reference_grow(ref, new_len);
   u64 move_count = sequence_split_inputs_require(i, old_len);
   u64 suffix_offset = size_after_inserting_one(i);
   bst_array_move(&ref->data[suffix_offset], &ref->data[i], move_count,
                  sizeof(ref->data[0]));
   ref->data[i] = x;
   ref->len = new_len;
   invariant(ref->len == new_len);
   invariant(ref->data[i] == x);
   reference_verify(ref);
}

static u64 reference_delete(reference_t *ref, u64 i) {
   reference_verify(ref);
   u64 old_len = ref->len;
   u64 new_len = sequence_delete_inputs_require(i, old_len);
   u64 x = ref->data[i];
   u64 move_count = suffix_size_after_rank(old_len, i);
   u64 suffix_offset = size_after_inserting_one(i);
   bst_array_move(&ref->data[i], &ref->data[suffix_offset], move_count,
                  sizeof(ref->data[0]));
   ref->len = new_len;
   invariant(ref->len == new_len);
   reference_verify(ref);
   return x;
}

static void reference_each(reference_t *ref, void (*visit)(u64, void *),
                           void *ctx) {
   reference_verify(ref);
   invariant(visit != NULL);
   for (u64 i = 0; i < ref->len; i++) {
      visit(ref->data[i], ctx);
   }
}

static reference_t reference_clone(reference_t *ref) {
   reference_verify(ref);
   return reference_from_range(ref->data, ref->len);
}

static list_t ref_vt_new(const list_vtable_t *vt) {
   list_vtable_require(vt);
   reference_t *r = reference_alloc();
   return reference_make(r, vt);
}

static list_t ref_vt_clone(list_t *self) {
   list_require(self);
   reference_t *orig = (reference_t *)self->impl;
   reference_t *copy = reference_alloc();
   *copy = reference_clone(orig);
   return reference_make(copy, self->vt);
}

static void ref_vt_free(list_t *self) {
   list_require(self);
   reference_t *r = (reference_t *)self->impl;
   reference_destroy(r);
   free(r);
   self->impl = NULL;
}

static tree_t *ref_vt_tree(list_t *self) {
   list_require(self);
   return NULL;
}

static u64 ref_vt_size(list_t *self) {
   list_require(self);
   return reference_size((reference_t *)self->impl);
}

static void ref_vt_each(list_t *self, void (*visit)(u64, void *), void *ctx) {
   list_require(self);
   invariant(visit != NULL);
   reference_each((reference_t *)self->impl, visit, ctx);
}

static void ref_vt_verify(list_t *self) {
   list_require(self);
   reference_verify((reference_t *)self->impl);
}

static u64 ref_vt_select(list_t *self, u64 i) {
   list_require(self);
   return reference_select((reference_t *)self->impl, i);
}

static void ref_vt_update(list_t *self, u64 i, u64 x) {
   list_require(self);
   reference_update((reference_t *)self->impl, i, x);
}

static void ref_vt_insert(list_t *self, u64 i, u64 x) {
   list_require(self);
   reference_insert((reference_t *)self->impl, i, x);
}

static u64 ref_vt_delete(list_t *self, u64 i) {
   list_require(self);
   return reference_delete((reference_t *)self->impl, i);
}

static void ref_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   list_require(self);
   invariant(out_l != NULL);
   invariant(out_r != NULL);
   invariant(out_l != self);
   invariant(out_r != self);
   invariant(out_l != out_r);

   reference_t *ref = (reference_t *)self->impl;
   reference_verify(ref);
   u64 size = ref->len;
   u64 rlen = sequence_split_inputs_require(i, size);

   reference_t *l = reference_alloc();
   *l = reference_from_range(ref->data, i);

   reference_t *r = reference_alloc();
   *r = reference_from_range(rlen > 0 ? &ref->data[i] : NULL, rlen);

   *out_l = reference_make(l, self->vt);
   *out_r = reference_make(r, self->vt);
   invariant(reference_size(l) == i);
   invariant(reference_size(r) == rlen);
   invariant(ref->len == size);
}

static list_t ref_vt_join(list_t *self, list_t *other) {
   list_require(self);
   list_require(other);
   invariant(other != self);
   invariant(other->impl != self->impl);
   invariant(self->vt == other->vt);

   reference_t *a = (reference_t *)self->impl;
   reference_t *b = (reference_t *)other->impl;

   reference_t *joined = reference_alloc();
   *joined = reference_joined(a, b);
   reference_verify(joined);
   invariant(reference_size(joined) == size_sum(a->len, b->len));

   return reference_make(joined, self->vt);
}

const list_vtable_t reference_vtable = {
    .name = "Reference",
    .new_fn = ref_vt_new,
    .clone_fn = ref_vt_clone,
    .free_fn = ref_vt_free,
    .tree_fn = ref_vt_tree,
    .size_fn = ref_vt_size,
    .each_fn = ref_vt_each,
    .verify_fn = ref_vt_verify,
    .select_fn = ref_vt_select,
    .update_fn = ref_vt_update,
    .insert_fn = ref_vt_insert,
    .delete_fn = ref_vt_delete,
    .apply_inserts_fn = list_apply_inserts_default,
    .apply_insert_delete_fn = list_apply_insert_delete_default,
    .apply_events_fn = list_apply_events_default,
    .time_inserts_fn = list_time_inserts_default,
    .time_insert_delete_fn = list_time_insert_delete_default,
    .split_fn = ref_vt_split,
    .join_fn = ref_vt_join,
};

typedef struct {
   reference_t *ref;
   u64 index;
} reference_fill_ctx_t;

static void reference_fill_visit(u64 x, void *ctx) {
   reference_fill_ctx_t *fill = (reference_fill_ctx_t *)ctx;
   invariant(fill != NULL);
   invariant(fill->ref != NULL);
   invariant(fill->index < fill->ref->len);
   u64 index = fill->index;
   fill->index = size_after_inserting_one(fill->index);
   fill->ref->data[index] = x;
}

list_t reference_for(list_t *instance) {
   u64 sz = list_size(instance);
   reference_t *ref = reference_alloc();
   *ref = reference_of_length(sz);
   reference_fill_ctx_t fill = {.ref = ref};
   list_each(instance, reference_fill_visit, &fill);
   invariant(fill.index == sz);
   reference_verify(ref);
   return reference_make(ref, &reference_vtable);
}

list_t reference_of_size(u64 size) {
   reference_t *ref = reference_alloc();
   *ref = reference_of_length(size);
   for (u64 i = 0; i < size; i++) {
      ref->data[i] = random_uint64();
   }
   reference_verify(ref);
   return reference_make(ref, &reference_vtable);
}
