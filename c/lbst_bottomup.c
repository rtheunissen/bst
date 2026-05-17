#include "lbst_bottomup.h"
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


#define WBST_BU_SPLIT(TYPE, PREFIX) \
   TYPE *t = (TYPE *)self->impl; tree_share(t->tree.root); node_t *l, *r; \
   wbst_bu_split(t->wb, t->tree.root, i, t->tree.size, &l, &r); \
   TYPE *sl = (TYPE *)calloc(1, sizeof(TYPE)); sl->tree = make_tree(l, i); sl->wb = t->wb; \
   TYPE *sr = (TYPE *)calloc(1, sizeof(TYPE)); sr->tree = make_tree(r, t->tree.size - i); sr->wb = t->wb; \
   *out_l = make_list(sl, self->vt); *out_r = make_list(sr, self->vt);

#define WBST_BU_JOIN(TYPE, PREFIX) \
   TYPE *t = (TYPE *)self->impl, *that = (TYPE *)other->impl; \
   tree_share(t->tree.root); tree_share(that->tree.root); \
   TYPE *res = (TYPE *)calloc(1, sizeof(TYPE)); res->wb = t->wb; \
   res->tree.root = wbst_bu_join(t->wb, t->tree.root, that->tree.root, t->tree.size, that->tree.size); \
   res->tree.size = t->tree.size + that->tree.size; return make_list(res, self->vt);

// LBSTBottomUp (LogSize weight balance)
WB_VT_BODY(lbst_bottomup_t, lbst_bu, wb_log_size,
   { lbst_bottomup_t *t = (lbst_bottomup_t *)self->impl; t->tree.root = wbst_bu_insert(t->wb, t->tree.root, t->tree.size, i, x); t->tree.size++; },
   { lbst_bottomup_t *t = (lbst_bottomup_t *)self->impl; u64 x; t->tree.root = wbst_bu_delete(t->wb, t->tree.root, t->tree.size, i, &x); t->tree.size--; return x; },
   { WBST_BU_SPLIT(lbst_bottomup_t, lbst_bu) },
   { WBST_BU_JOIN(lbst_bottomup_t, lbst_bu) },
   { lbst_bottomup_t *t = (lbst_bottomup_t *)self->impl; tree_verify_size(t->tree.root, t->tree.size); wbst_bu_verifyBalance(t->wb, t->tree.root, t->tree.size); invariant(height(t->tree.root) <= t->wb.maxHeight(t->tree.size)); }
)

const list_vtable_t lbst_bottomup_vtable = { "LBSTBottomUp", lbst_bu_vt_new, lbst_bu_vt_clone, lbst_bu_vt_free, lbst_bu_vt_size, lbst_bu_vt_each, lbst_bu_vt_verify, lbst_bu_vt_select, lbst_bu_vt_update, lbst_bu_vt_insert, lbst_bu_vt_delete, lbst_bu_vt_split, lbst_bu_vt_join };

