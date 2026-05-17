#include "lbst_topdown.h"
#include "wbst_topdown.h"

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


#define WBST_TD_SPLIT(TYPE) \
   TYPE *t = (TYPE *)self->impl; tree_share(t->tree.root); node_t *l, *r; \
   wbst_td_split(t->wb, t->tree.root, i, t->tree.size, &l, &r); \
   TYPE *sl = (TYPE *)calloc(1, sizeof(TYPE)); sl->tree = make_tree(l, i); sl->wb = t->wb; \
   TYPE *sr = (TYPE *)calloc(1, sizeof(TYPE)); sr->tree = make_tree(r, t->tree.size - i); sr->wb = t->wb; \
   *out_l = make_list(sl, self->vt); *out_r = make_list(sr, self->vt);

#define WBST_TD_JOIN(TYPE) \
   TYPE *t = (TYPE *)self->impl, *that = (TYPE *)other->impl; \
   tree_share(t->tree.root); tree_share(that->tree.root); \
   TYPE *res = (TYPE *)calloc(1, sizeof(TYPE)); res->wb = t->wb; \
   res->tree.root = wbst_td_join(t->wb, t->tree.root, that->tree.root, t->tree.size, that->tree.size); \
   res->tree.size = t->tree.size + that->tree.size; return make_list(res, self->vt);

// LBSTTopDown (LogWeight weight balance)
WB_VT_BODY(lbst_topdown_t, lbst_td, wb_log_weight,
   { lbst_topdown_t *t = (lbst_topdown_t *)self->impl; wbst_td_insert(t->wb, &t->tree.root, t->tree.size, i, x); t->tree.size++; },
   { lbst_topdown_t *t = (lbst_topdown_t *)self->impl; node_t *del; wbst_td_delete_internal(t->wb, &t->tree.root, t->tree.size, i, &del); t->tree.size--; u64 x = del->x; tree_release(del); return x; },
   { WBST_TD_SPLIT(lbst_topdown_t) },
   { WBST_TD_JOIN(lbst_topdown_t) },
   { lbst_topdown_t *t = (lbst_topdown_t *)self->impl; tree_verify_size(t->tree.root, t->tree.size); wbst_td_verifyBalance(t->wb, t->tree.root, t->tree.size); invariant(height(t->tree.root) <= t->wb.maxHeight(t->tree.size)); }
)

const list_vtable_t lbst_topdown_vtable = { "LBSTTopDown", lbst_td_vt_new, lbst_td_vt_clone, lbst_td_vt_free, lbst_td_vt_size, lbst_td_vt_each, lbst_td_vt_verify, lbst_td_vt_select, lbst_td_vt_update, lbst_td_vt_insert, lbst_td_vt_delete, lbst_td_vt_split, lbst_td_vt_join };
