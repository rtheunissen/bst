#ifndef LIST_H
#define LIST_H

#include "bst.h"

//
// Abstract list interface (C vtable equivalent of Go's list.List interface)
//
// Each tree implementation provides a vtable with these function pointers.
// The test suite operates on list_t handles, dispatching through the vtable.
//

typedef struct list_vtable list_vtable_t;

typedef struct list {
   void                *impl;  // Pointer to the concrete tree struct
   const list_vtable_t *vt;    // Pointer to the vtable
} list_t;

struct list_vtable {
   const char *name;

   list_t (*new_fn)    (const list_vtable_t *vt);
   list_t (*clone_fn)  (list_t *self);
   void   (*free_fn)   (list_t *self);
   u64    (*size_fn)   (list_t *self);
   void   (*each_fn)   (list_t *self, void (*visit)(u64, void *), void *ctx);
   void   (*verify_fn) (list_t *self);
   u64    (*select_fn) (list_t *self, u64 i);
   void   (*update_fn) (list_t *self, u64 i, u64 x);
   void   (*insert_fn) (list_t *self, u64 i, u64 x);
   u64    (*delete_fn) (list_t *self, u64 i);
   void   (*split_fn)  (list_t *self, u64 i, list_t *out_l, list_t *out_r);
   list_t (*join_fn)   (list_t *self, list_t *other);
};


// ─────────────────────────────────────────────────────────────────────────────
// Convenience dispatchers
// ─────────────────────────────────────────────────────────────────────────────

static inline list_t list_new(const list_vtable_t *vt) {
   return vt->new_fn(vt);
}

static inline list_t list_clone(list_t *l) {
   return l->vt->clone_fn(l);
}

static inline void list_free(list_t *l) {
   if (l->vt && l->vt->free_fn) l->vt->free_fn(l);
}

static inline u64 list_size(list_t *l) {
   return l->vt->size_fn(l);
}

static inline void list_each(list_t *l, void (*visit)(u64, void *), void *ctx) {
   l->vt->each_fn(l, visit, ctx);
}

static inline void list_verify(list_t *l) {
   l->vt->verify_fn(l);
}

static inline u64 list_select(list_t *l, u64 i) {
   return l->vt->select_fn(l, i);
}

static inline void list_update(list_t *l, u64 i, u64 x) {
   l->vt->update_fn(l, i, x);
}

static inline void list_insert(list_t *l, u64 i, u64 x) {
   l->vt->insert_fn(l, i, x);
}

static inline u64 list_delete(list_t *l, u64 i) {
   return l->vt->delete_fn(l, i);
}

static inline void list_split(list_t *l, u64 i, list_t *out_l, list_t *out_r) {
   l->vt->split_fn(l, i, out_l, out_r);
}

static inline list_t list_join(list_t *l, list_t *other) {
   return l->vt->join_fn(l, other);
}

#endif // LIST_H
