#include "lbst_relaxed.h"
#include "wbst_relaxed.h"
#include "wbst_bottomup.h"

#include <math.h>

static inline tree_t make_tree(node_t *root, u64 size) {
   return (tree_t){.root = root, .size = size};
}
static inline list_t make_list(void *impl, const list_vtable_t *vt) {
   return (list_t){.impl = impl, .vt = vt};
}

#define WB_VT_BODY(TYPE, PREFIX, WB_INIT, INSF, DELF, SPLF, JOINF, VERF) \
static list_t PREFIX##_vt_new(const list_vtable_t *vt) { TYPE *t = (TYPE *)calloc(1, sizeof(TYPE)); t->wb = WB_INIT; return make_list(t, vt); } \
static list_t PREFIX##_vt_clone(list_t *self) { TYPE *o = (TYPE *)self->impl; TYPE *c = (TYPE *)malloc(sizeof(TYPE)); tree_share(o->tree.root); *c = *o; return make_list(c, self->vt); } \
static void PREFIX##_vt_free(list_t *self) { TYPE *t = (TYPE *)self->impl; free_nodes(t->tree.root); free(t); self->impl = NULL; } \
static u64  PREFIX##_vt_size(list_t *self) { return ((TYPE *)self->impl)->tree.size; } \
static void PREFIX##_vt_each(list_t *self, void (*v)(u64, void*), void *c) { inorder(((TYPE *)self->impl)->tree.root, v, c); } \
static u64  PREFIX##_vt_select(list_t *self, u64 i) { return tree_lookup(&((TYPE *)self->impl)->tree, i); } \
static void PREFIX##_vt_update(list_t *self, u64 i, u64 x) { tree_update_base(&((TYPE *)self->impl)->tree, i, x); } \
static void PREFIX##_vt_verify(list_t *self) { VERF } \
static void PREFIX##_vt_insert(list_t *self, u64 i, u64 x) { INSF } \
static u64  PREFIX##_vt_delete(list_t *self, u64 i) { DELF } \
static void PREFIX##_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) { SPLF } \
static list_t PREFIX##_vt_join(list_t *self, list_t *other) { JOINF }

// LBSTRelaxed (LogWeight) - written explicitly
static list_t lbst_rlx_vt_new(const list_vtable_t *vt) { lbst_relaxed_t *t = (lbst_relaxed_t *)calloc(1, sizeof(lbst_relaxed_t)); t->wb = wb_log_weight; return (list_t){t, vt}; }
static list_t lbst_rlx_vt_clone(list_t *self) { lbst_relaxed_t *o = (lbst_relaxed_t *)self->impl; lbst_relaxed_t *c = (lbst_relaxed_t *)malloc(sizeof(lbst_relaxed_t)); tree_share(o->tree.root); *c = *o; return (list_t){c, self->vt}; }
static void lbst_rlx_vt_free(list_t *self) { lbst_relaxed_t *t = (lbst_relaxed_t *)self->impl; free_nodes(t->tree.root); free(t); self->impl = NULL; }
static u64  lbst_rlx_vt_size(list_t *self) { return ((lbst_relaxed_t *)self->impl)->tree.size; }
static void lbst_rlx_vt_each(list_t *self, void (*v)(u64, void*), void *c) { inorder(((lbst_relaxed_t *)self->impl)->tree.root, v, c); }
static u64  lbst_rlx_vt_select(list_t *self, u64 i) { return tree_lookup(&((lbst_relaxed_t *)self->impl)->tree, i); }
static void lbst_rlx_vt_update(list_t *self, u64 i, u64 x) { tree_update_base(&((lbst_relaxed_t *)self->impl)->tree, i, x); }
static void lbst_rlx_vt_verify(list_t *self) { lbst_relaxed_t *t = (lbst_relaxed_t *)self->impl; tree_verify_size(t->tree.root, t->tree.size); }
static void lbst_rlx_vt_insert(list_t *self, u64 i, u64 x) { lbst_relaxed_t *t = (lbst_relaxed_t *)self->impl; wbst_rlx_insert(t->wb, &t->tree.root, t->tree.size, i, x, t->tree.size); t->tree.size++; }
static u64  lbst_rlx_vt_delete(list_t *self, u64 i) { return tree_delete_base(&((lbst_relaxed_t *)self->impl)->tree, i); }
static void lbst_rlx_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   lbst_relaxed_t *t = (lbst_relaxed_t *)self->impl; tree_share(t->tree.root); node_t *l, *r; tree_split_internal(t->tree.root, i, &l, &r);
   lbst_relaxed_t *sl = (lbst_relaxed_t *)calloc(1, sizeof(lbst_relaxed_t)); sl->tree = make_tree(l, i); sl->wb = t->wb;
   lbst_relaxed_t *sr = (lbst_relaxed_t *)calloc(1, sizeof(lbst_relaxed_t)); sr->tree = make_tree(r, t->tree.size - i); sr->wb = t->wb;
   *out_l = (list_t){sl, self->vt}; *out_r = (list_t){sr, self->vt};
}
static list_t lbst_rlx_vt_join(list_t *self, list_t *other) {
   lbst_relaxed_t *t = (lbst_relaxed_t *)self->impl, *that = (lbst_relaxed_t *)other->impl;
   tree_share(t->tree.root); tree_share(that->tree.root);
   lbst_relaxed_t *res = (lbst_relaxed_t *)calloc(1, sizeof(lbst_relaxed_t)); res->wb = t->wb;
   res->tree.root = wbst_rlx_join(t->wb, t->tree.root, that->tree.root, t->tree.size, that->tree.size);
   res->tree.size = t->tree.size + that->tree.size; return (list_t){res, self->vt};
}

const list_vtable_t lbst_relaxed_vtable = { "LBSTRelaxed", lbst_rlx_vt_new, lbst_rlx_vt_clone, lbst_rlx_vt_free, lbst_rlx_vt_size, lbst_rlx_vt_each, lbst_rlx_vt_verify, lbst_rlx_vt_select, lbst_rlx_vt_update, lbst_rlx_vt_insert, lbst_rlx_vt_delete, lbst_rlx_vt_split, lbst_rlx_vt_join };

