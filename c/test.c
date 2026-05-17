#include "bst.h"
#include "list.h"
#include "random.h"
#include "distributions.h"
#include "splay.h"
#include "randomized.h"
#include "zip.h"
#include "treap_bottomup.h"
#include "treap_topdown.h"
#include "conc.h"
#include "llrb.h"
#include "aa.h"
#include "avl_bottomup.h"
#include "avl_topdown.h"
#include "avl_weak_bottomup.h"
#include "avl_weak_topdown.h"
#include "avl_relaxed_bottomup.h"
#include "avl_relaxed_topdown.h"
#include "redblack_bottomup.h"
#include "redblack_topdown.h"
#include "redblack_relaxed_bottomup.h"
#include "redblack_relaxed_topdown.h"
#include "wbst_bottomup.h"
#include "wbst_topdown.h"
#include "wbst_relaxed.h"
#include "lbst_bottomup.h"
#include "lbst_topdown.h"
#include "lbst_relaxed.h"
#include "treap_fingertree.h"
#include "balancers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// ─────────────────────────────────────────────────────────────────────────────
// Reference implementation (array-based)
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   u64 *data;
   u64  len;
   u64  cap;
} reference_t;

static void reference_init(reference_t *ref) {
   ref->data = NULL;
   ref->len = 0;
   ref->cap = 0;
}

static void reference_grow(reference_t *ref, u64 needed) {
   if (needed <= ref->cap) return;
   u64 new_cap = ref->cap == 0 ? 8 : ref->cap;
   while (new_cap < needed) new_cap *= 2;
   ref->data = (u64 *)realloc(ref->data, new_cap * sizeof(u64));
   ref->cap = new_cap;
}

static void reference_destroy(reference_t *ref) {
   free(ref->data);
   ref->data = NULL;
   ref->len = 0;
   ref->cap = 0;
}

static u64 reference_size(reference_t *ref) {
   return ref->len;
}

static u64 reference_select(reference_t *ref, u64 i) {
   return ref->data[i];
}

static void reference_update(reference_t *ref, u64 i, u64 x) {
   ref->data[i] = x;
}

static void reference_insert(reference_t *ref, u64 i, u64 x) {
   reference_grow(ref, ref->len + 1);
   memmove(&ref->data[i + 1], &ref->data[i], (ref->len - i) * sizeof(u64));
   ref->data[i] = x;
   ref->len++;
}

static u64 reference_delete(reference_t *ref, u64 i) {
   u64 x = ref->data[i];
   memmove(&ref->data[i], &ref->data[i + 1], (ref->len - i - 1) * sizeof(u64));
   ref->len--;
   return x;
}

static void reference_each(reference_t *ref, void (*visit)(u64, void *), void *ctx) {
   for (u64 i = 0; i < ref->len; i++) {
      visit(ref->data[i], ctx);
   }
}

static reference_t reference_clone(reference_t *ref) {
   reference_t clone;
   clone.len = ref->len;
   clone.cap = ref->len;
   clone.data = ref->len > 0 ? (u64 *)malloc(ref->len * sizeof(u64)) : NULL;
   if (ref->len > 0) memcpy(clone.data, ref->data, ref->len * sizeof(u64));
   return clone;
}


// ─────────────────────────────────────────────────────────────────────────────
// Reference vtable (so Reference can be used as list_t)
// ─────────────────────────────────────────────────────────────────────────────

static list_t ref_vt_new(const list_vtable_t *vt) {
   reference_t *r = (reference_t *)calloc(1, sizeof(reference_t));
   return (list_t){.impl = r, .vt = vt};
}

static list_t ref_vt_clone(list_t *self) {
   reference_t *orig = (reference_t *)self->impl;
   reference_t *copy = (reference_t *)malloc(sizeof(reference_t));
   *copy = reference_clone(orig);
   return (list_t){.impl = copy, .vt = self->vt};
}

static void ref_vt_free(list_t *self) {
   reference_t *r = (reference_t *)self->impl;
   reference_destroy(r);
   free(r);
   self->impl = NULL;
}

static u64 ref_vt_size(list_t *self) {
   return reference_size((reference_t *)self->impl);
}

static void ref_vt_each(list_t *self, void (*visit)(u64, void *), void *ctx) {
   reference_each((reference_t *)self->impl, visit, ctx);
}

static void ref_vt_verify(list_t *self) {
   (void)self;
}

static u64 ref_vt_select(list_t *self, u64 i) {
   return reference_select((reference_t *)self->impl, i);
}

static void ref_vt_update(list_t *self, u64 i, u64 x) {
   reference_update((reference_t *)self->impl, i, x);
}

static void ref_vt_insert(list_t *self, u64 i, u64 x) {
   reference_insert((reference_t *)self->impl, i, x);
}

static u64 ref_vt_delete(list_t *self, u64 i) {
   return reference_delete((reference_t *)self->impl, i);
}

static void ref_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   reference_t *ref = (reference_t *)self->impl;

   reference_t *l = (reference_t *)calloc(1, sizeof(reference_t));
   l->len = i;
   l->cap = i;
   l->data = i > 0 ? (u64 *)malloc(i * sizeof(u64)) : NULL;
   if (i > 0) memcpy(l->data, ref->data, i * sizeof(u64));

   u64 rlen = ref->len - i;
   reference_t *r = (reference_t *)calloc(1, sizeof(reference_t));
   r->len = rlen;
   r->cap = rlen;
   r->data = rlen > 0 ? (u64 *)malloc(rlen * sizeof(u64)) : NULL;
   if (rlen > 0) memcpy(r->data, &ref->data[i], rlen * sizeof(u64));

   *out_l = (list_t){.impl = l, .vt = self->vt};
   *out_r = (list_t){.impl = r, .vt = self->vt};
}

static list_t ref_vt_join(list_t *self, list_t *other) {
   reference_t *a = (reference_t *)self->impl;
   reference_t *b = (reference_t *)other->impl;

   reference_t *joined = (reference_t *)calloc(1, sizeof(reference_t));
   u64 total = a->len + b->len;
   joined->len = total;
   joined->cap = total;
   joined->data = total > 0 ? (u64 *)malloc(total * sizeof(u64)) : NULL;
   if (a->len > 0) memcpy(joined->data, a->data, a->len * sizeof(u64));
   if (b->len > 0) memcpy(&joined->data[a->len], b->data, b->len * sizeof(u64));

   return (list_t){.impl = joined, .vt = self->vt};
}

static const list_vtable_t reference_vtable = {
   .name      = "Reference",
   .new_fn    = ref_vt_new,
   .clone_fn  = ref_vt_clone,
   .free_fn   = ref_vt_free,
   .size_fn   = ref_vt_size,
   .each_fn   = ref_vt_each,
   .verify_fn = ref_vt_verify,
   .select_fn = ref_vt_select,
   .update_fn = ref_vt_update,
   .insert_fn = ref_vt_insert,
   .delete_fn = ref_vt_delete,
   .split_fn  = ref_vt_split,
   .join_fn   = ref_vt_join,
};


// ─────────────────────────────────────────────────────────────────────────────
// Test helpers
// ─────────────────────────────────────────────────────────────────────────────

static void assert_equal(u64 expected, u64 actual) {
   if (expected != actual) {
      fprintf(stderr, "expected: %llu, actual: %llu\n",
              (unsigned long long)expected, (unsigned long long)actual);
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
   u64 expected = list_select(c->expected, c->i);
   assert_equal(expected, actual);
   c->i++;
   c->n++;
}

static void assert_list(list_t *expected, list_t *actual) {
   u64 en = list_size(expected);
   u64 an = list_size(actual);
   assert_equal(en, an);
   assert_list_ctx_t ctx = {expected, 0, 0};
   list_each(actual, assert_list_visit, &ctx);
   assert_equal(en, ctx.n);
}

// Creates a list_t (reference) from the current contents of an instance.
static list_t reference_for(list_t *instance) {
   u64 sz = list_size(instance);
   reference_t *ref = (reference_t *)calloc(1, sizeof(reference_t));
   if (sz > 0) {
      ref->data = (u64 *)malloc(sz * sizeof(u64));
      ref->len = sz;
      ref->cap = sz;
      // Fill by iterating
      u64 idx = 0;
      for (u64 j = 0; j < sz; j++) {
         ref->data[j] = list_select(instance, j);
      }
      (void)idx;
   }
   return (list_t){.impl = ref, .vt = &reference_vtable};
}

static list_t reference_of_size(u64 size) {
   reference_t *ref = (reference_t *)calloc(1, sizeof(reference_t));
   if (size > 0) {
      ref->data = (u64 *)malloc(size * sizeof(u64));
      ref->len = size;
      ref->cap = size;
      for (u64 i = 0; i < size; i++) {
         ref->data[i] = random_uint64();
      }
   }
   return (list_t){.impl = ref, .vt = &reference_vtable};
}

static list_t insert_to_size(const list_vtable_t *vt, u64 size, distribution_t *dist) {
   list_t instance = list_new(vt);
   while (list_size(&instance) < size) {
      list_insert(&instance, dist_less_than(dist, list_size(&instance) + 1), random_uint64());
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


// ─────────────────────────────────────────────────────────────────────────────
// Core test operations (in-place and persistent)
// ─────────────────────────────────────────────────────────────────────────────

static void test_insert_in_place(list_t *ref, list_t *seq, distribution_t *dist) {
   u64 i = dist_less_than(dist, list_size(ref) + 1);
   u64 x = random_uint64();
   list_insert(seq, i, x);
   list_insert(ref, i, x);
   list_verify(seq);
   assert_list(ref, seq);
}

static void test_delete_in_place(list_t *ref, list_t *seq, distribution_t *dist) {
   u64 i = dist_less_than(dist, list_size(ref));
   u64 x = list_delete(ref, i);
   assert_equal(x, list_delete(seq, i));
   list_verify(seq);
   assert_list(ref, seq);
}

static void test_insert_persistent(list_t *ref, list_t *seq, distribution_t *dist) {
   list_t snapshot = list_clone(seq);

   u64 i = dist_less_than(dist, list_size(ref) + 1);
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

static void test_persistent_delete(list_t *ref, list_t *seq, distribution_t *dist) {
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

typedef void (*test_fn)(const list_vtable_t *vt, u64 size, distribution_t *dist);

static void TestInsert(const list_vtable_t *vt, u64 size, distribution_t *dist) {
   list_t seq = list_new(vt);
   list_t ref = reference_for(&seq);
   while (list_size(&ref) < size) {
      test_insert_in_place(&ref, &seq, dist);
   }
   list_free(&seq);
   list_free(&ref);
}

static void TestInsertPersistent(const list_vtable_t *vt, u64 size, distribution_t *dist) {
   list_t seq = list_new(vt);
   list_t ref = reference_for(&seq);
   while (list_size(&ref) < size) {
      test_insert_persistent(&ref, &seq, dist);
   }
   list_free(&seq);
   list_free(&ref);
}

static void TestSelect(const list_vtable_t *vt, u64 size, distribution_t *dist) {
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

static void TestSelectPersistent(const list_vtable_t *vt, u64 size, distribution_t *dist) {
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

static void TestSelectAfterInsert(const list_vtable_t *vt, u64 scale, distribution_t *dist) {
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

static void TestSelectAfterInsertPersistent(const list_vtable_t *vt, u64 scale, distribution_t *dist) {
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

static void TestUpdate(const list_vtable_t *vt, u64 size, distribution_t *dist) {
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

static void TestUpdatePersistent(const list_vtable_t *vt, u64 size, distribution_t *dist) {
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

static void TestDelete(const list_vtable_t *vt, u64 size, distribution_t *dist) {
   list_t seq = insert_to_size(vt, size, dist);
   list_t ref = reference_for(&seq);
   while (list_size(&ref) > 0) {
      test_delete_in_place(&ref, &seq, dist);
   }
   list_free(&seq);
   list_free(&ref);
}

static void TestDeletePersistent(const list_vtable_t *vt, u64 size, distribution_t *dist) {
   list_t seq = insert_to_size(vt, size, dist);
   list_t ref = reference_for(&seq);
   while (list_size(&ref) > 0) {
      test_persistent_delete(&ref, &seq, dist);
   }
   list_free(&seq);
   list_free(&ref);
}

static void TestInsertDelete(const list_vtable_t *vt, u64 size, distribution_t *dist) {
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

static void TestInsertDeletePersistent(const list_vtable_t *vt, u64 size, distribution_t *dist) {
   list_t seq = list_new(vt);
   list_t ref = list_new(&reference_vtable);
   while (list_size(&ref) < size) {
      test_insert_persistent(&ref, &seq, dist);
      test_insert_persistent(&ref, &seq, dist);
      test_insert_persistent(&ref, &seq, dist);

      distribution_t *ud = uniform_from_global();
      test_persistent_delete(&ref, &seq, ud);
      test_persistent_delete(&ref, &seq, ud);
   }
   list_free(&seq);
   list_free(&ref);
}

static void TestSplit(const list_vtable_t *vt, u64 size, distribution_t *position) {
   list_t instance = insert_to_size(vt, size, position);
   list_t ref = reference_for(&instance);

   for (u64 i = 0; i <= list_size(&ref); i++) {

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
      while (list_size(&L) > 0) list_delete(&L, 0);
      while (list_size(&R) > 0) list_delete(&R, 0);

      list_verify(&instance);
      assert_list(&ref, &instance);

      list_free(&L);
      list_free(&R);
      list_free(&_L);
      list_free(&_R);
   }
   list_free(&instance);
   list_free(&ref);
}

static void TestJoin(const list_vtable_t *vt, u64 size, distribution_t *dist) {
   if (size == 0) return;
   for (u64 i = 0; i < size; i++) {
      list_t L = insert_to_size(vt, random_uint64() % size, dist);
      list_t R = insert_to_size(vt, random_uint64() % size, dist);

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

      list_free(&LL); list_free(&LR); list_free(&RL); list_free(&RR);
      list_free(&_LL); list_free(&_LR); list_free(&_RL); list_free(&_RR);
      list_free(&L); list_free(&R);
      list_free(&_L); list_free(&_R);
   }
}

static void TestJoinFromSplit(const list_vtable_t *vt, u64 size, distribution_t *dist) {
   list_t instance = insert_to_size(vt, size, dist);
   list_t ref = reference_for(&instance);

   for (u64 i = 0; i < size; i++) {

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

      list_free(&LL); list_free(&LR); list_free(&RL); list_free(&RR);
      list_free(&_LL); list_free(&_LR); list_free(&_RL); list_free(&_RR);
      list_free(&L); list_free(&R);
      list_free(&_L); list_free(&_R);
   }
   list_free(&instance);
   list_free(&ref);
}

static void TestJoinAfterInsertDelete(const list_vtable_t *vt, u64 size, distribution_t *dist) {
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

      list_free(&LL); list_free(&LR); list_free(&RL); list_free(&RR);
      list_free(&_LL); list_free(&_LR); list_free(&_RL); list_free(&_RR);
   }
   list_free(&L); list_free(&R);
   list_free(&_L); list_free(&_R);
}


// ─────────────────────────────────────────────────────────────────────────────
// Balancer tests (matching Go TestBalancers)
// ─────────────────────────────────────────────────────────────────────────────

static void TestBalancers(void) {
   const balancer_t *balancers[] = {
      &balancer_median,
      &balancer_height,
      &balancer_weight,
      &balancer_log,
      &balancer_cost,
      &balancer_dsw,
   };
   const char *balancer_dist_names[] = {
      "Uniform",
      "Normal",
      "Skewed",
      "Zipf",
      "Maximum",
   };
   const dist_factory_fn balancer_dist_factories[] = {
      uniform_new,
      normal_new,
      skewed_new,
      zipf_new,
      maximum_new,
   };

   int num_balancers = sizeof(balancers) / sizeof(balancers[0]);
   int num_distributions = sizeof(balancer_dist_factories) / sizeof(balancer_dist_factories[0]);

   printf("Balancers:\n");
   for (int b = 0; b < num_balancers; b++) {
      printf("  %s: ", balancers[b]->name);
      fflush(stdout);

      for (int d = 0; d < num_distributions; d++) {
         (void)balancer_dist_names[d];

         tree_t tree = {0};
         distribution_t *dist = balancer_dist_factories[d](1);

         while (tree.size < 1000) {
            u64 i = dist_less_than(dist, tree.size + 1);
            u64 x = random_uint64();

            tree_insert_base(&tree, i, x);

            tree_t clone = tree_clone(&tree);
            tree = balancers[b]->restore(clone);
            clone = tree_clone(&tree);
            tree = balancers[b]->restore(clone);

            balancers[b]->verify(tree);
         }

         dist_destroy(dist);
         free_nodes(tree.root);
         printf(".");
         fflush(stdout);
      }
      printf(" OK\n");
   }
}


// ─────────────────────────────────────────────────────────────────────────────
// Test runner
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   const char *name;
   test_fn fn;
} named_test_t;

static named_test_t all_tests[] = {
   {"TestInsert",                      TestInsert},
   {"TestInsertPersistent",            TestInsertPersistent},
   {"TestSelect",                      TestSelect},
   {"TestSelectPersistent",            TestSelectPersistent},
   {"TestSelectAfterInsert",           TestSelectAfterInsert},
   {"TestSelectAfterInsertPersistent", TestSelectAfterInsertPersistent},
   {"TestUpdate",                      TestUpdate},
   {"TestUpdatePersistent",            TestUpdatePersistent},
   {"TestDelete",                      TestDelete},
   {"TestDeletePersistent",            TestDeletePersistent},
   {"TestInsertDelete",                TestInsertDelete},
   {"TestInsertDeletePersistent",      TestInsertDeletePersistent},
   {"TestSplit",                       TestSplit},
   {"TestJoin",                        TestJoin},
   {"TestJoinFromSplit",               TestJoinFromSplit},
   {"TestJoinAfterInsertDelete",       TestJoinAfterInsertDelete},
};

static const int num_tests = sizeof(all_tests) / sizeof(all_tests[0]);

int main(void) {
   random_seed(1);
   TestBalancers();

   const list_vtable_t *trees[] = {
      &splay_vtable,
      &randomized_vtable,
      &zip_vtable,
      &treap_bottomup_vtable,
      &treap_topdown_vtable,
      &conc_vtable,
      &llrb_vtable,
      &aa_vtable,
      &avl_bottomup_vtable,
      &avl_topdown_vtable,
      &avl_weak_bottomup_vtable,
      &avl_weak_topdown_vtable,
      &avl_relaxed_bottomup_vtable,
      &avl_relaxed_topdown_vtable,
      &redblack_bottomup_vtable,
      &redblack_topdown_vtable,
      &redblack_relaxed_bottomup_vtable,
      &redblack_relaxed_topdown_vtable,
      &wbst_bottomup_vtable,
      &wbst_topdown_vtable,
      &wbst_relaxed_vtable,
      &lbst_bottomup_vtable,
      &lbst_topdown_vtable,
      &lbst_relaxed_vtable,
      &treap_fingertree_vtable,
   };
   int num_trees = sizeof(trees) / sizeof(trees[0]);

   u64 scale = 100;
   int total = 0;

   for (int t = 0; t < num_trees; t++) {
      printf("%s:\n", trees[t]->name);
      for (int d = 0; d < dist_count; d++) {
         printf("  %s: ", dist_names[d]);
         fflush(stdout);
         for (int f = 0; f < num_tests; f++) {
            for (u64 s = 0; s <= scale; s++) {
               distribution_t *dist = dist_factories[d](random_uint64());
               all_tests[f].fn(trees[t], s, dist);
               dist_destroy(dist);
               total++;
            }
            printf(".");
            fflush(stdout);
         }
         printf(" OK\n");
      }
   }

   printf("\nAll tests passed (%d tests).\n", total);
   return 0;
}
