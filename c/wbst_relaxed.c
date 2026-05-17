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

// Forward declaration
node_t *wbst_rlx_join(weight_balance_t wb, node_t *l, node_t *r, u64 sl, u64 sr);

static node_t *wbst_rlx_balance(weight_balance_t wb, node_t *p, u64 s) {
   if (s < 4) return p;
   if (!wb.isBalanced(sizeL(p), sizeR(p, s)) || !wb.isBalanced(sizeR(p, s), sizeL(p)))
      p = tree_partition(p, s >> 1);
   p->l = wbst_rlx_balance(wb, p->l, sizeL(p));
   p->r = wbst_rlx_balance(wb, p->r, sizeR(p, s));
   return p;
}

void wbst_rlx_insert(weight_balance_t wb, node_t **p, u64 s, u64 i, u64 x, u64 tree_size) {
   node_t **unbalancedNode = NULL;
   u64 unbalancedSize = 0;
   int h = 0;
   for (;;) {
      if (*p == NULL) {
         *p = tree_allocate((node_t){.x = x});
         if (h > wb.maxHeight(tree_size + 1)) {
            *unbalancedNode = wbst_rlx_balance(wb, *unbalancedNode, unbalancedSize);
         }
         return;
      }
      tree_persist(p); h++;
      u64 sl = sizeL(*p), sr = sizeR(*p, s);
      if (i <= sl) {
         if (unbalancedNode == NULL && !wb.isBalanced(sr, sl + 1)) { unbalancedNode = p; unbalancedSize = s + 1; }
         p = insertL(*p); s = sl;
      } else {
         if (unbalancedNode == NULL && !wb.isBalanced(sl, sr + 1)) { unbalancedNode = p; unbalancedSize = s + 1; }
         p = insertR(*p, &i); s = sr;
      }
   }
}

static node_t *wbst_rlx_joinLr(weight_balance_t wb, node_t *l, node_t *r, u64 sl, u64 sr) {
   if (r == NULL) return l;
   if (wb.isBalanced(sr, sl)) { node_t *p = tree_deleteMax(&l); p->l = l; p->r = r; p->s = sl - 1; return p; }
   tree_persist(&l); l->r = wbst_rlx_join(wb, l->r, r, sl - l->s - 1, sr); return l;
}

static node_t *wbst_rlx_joinlR(weight_balance_t wb, node_t *l, node_t *r, u64 sl, u64 sr) {
   if (l == NULL) return r;
   if (wb.isBalanced(sl, sr)) { node_t *p = tree_deleteMin(&r); p->l = l; p->r = r; p->s = sl; return p; }
   tree_persist(&r); r->l = wbst_rlx_join(wb, l, r->l, sl, r->s); r->s = sl + r->s; return r;
}

node_t *wbst_rlx_join(weight_balance_t wb, node_t *l, node_t *r, u64 sl, u64 sr) {
   if (sl > sr) return wbst_rlx_joinLr(wb, l, r, sl, sr);
   else return wbst_rlx_joinlR(wb, l, r, sl, sr);
}

// WBSTRelaxed - written explicitly to avoid macro comma issues
static list_t wbst_rlx_vt_new(const list_vtable_t *vt) { wbst_relaxed_t *t = (wbst_relaxed_t *)calloc(1, sizeof(wbst_relaxed_t)); t->wb = wb_three_two; return (list_t){t, vt}; }
static list_t wbst_rlx_vt_clone(list_t *self) { wbst_relaxed_t *o = (wbst_relaxed_t *)self->impl; wbst_relaxed_t *c = (wbst_relaxed_t *)malloc(sizeof(wbst_relaxed_t)); tree_share(o->tree.root); *c = *o; return (list_t){c, self->vt}; }
static void wbst_rlx_vt_free(list_t *self) { wbst_relaxed_t *t = (wbst_relaxed_t *)self->impl; free_nodes(t->tree.root); free(t); self->impl = NULL; }
static u64  wbst_rlx_vt_size(list_t *self) { return ((wbst_relaxed_t *)self->impl)->tree.size; }
static void wbst_rlx_vt_each(list_t *self, void (*v)(u64, void*), void *c) { inorder(((wbst_relaxed_t *)self->impl)->tree.root, v, c); }
static u64  wbst_rlx_vt_select(list_t *self, u64 i) { return tree_lookup(&((wbst_relaxed_t *)self->impl)->tree, i); }
static void wbst_rlx_vt_update(list_t *self, u64 i, u64 x) { tree_update_base(&((wbst_relaxed_t *)self->impl)->tree, i, x); }
static void wbst_rlx_vt_verify(list_t *self) { wbst_relaxed_t *t = (wbst_relaxed_t *)self->impl; tree_verify_size(t->tree.root, t->tree.size); }
static void wbst_rlx_vt_insert(list_t *self, u64 i, u64 x) { wbst_relaxed_t *t = (wbst_relaxed_t *)self->impl; wbst_rlx_insert(t->wb, &t->tree.root, t->tree.size, i, x, t->tree.size); t->tree.size++; }
static u64  wbst_rlx_vt_delete(list_t *self, u64 i) { return tree_delete_base(&((wbst_relaxed_t *)self->impl)->tree, i); }
static void wbst_rlx_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   wbst_relaxed_t *t = (wbst_relaxed_t *)self->impl; tree_share(t->tree.root); node_t *l, *r; tree_split_internal(t->tree.root, i, &l, &r);
   wbst_relaxed_t *sl = (wbst_relaxed_t *)calloc(1, sizeof(wbst_relaxed_t)); sl->tree = make_tree(l, i); sl->wb = t->wb;
   wbst_relaxed_t *sr = (wbst_relaxed_t *)calloc(1, sizeof(wbst_relaxed_t)); sr->tree = make_tree(r, t->tree.size - i); sr->wb = t->wb;
   *out_l = (list_t){sl, self->vt}; *out_r = (list_t){sr, self->vt};
}
static list_t wbst_rlx_vt_join(list_t *self, list_t *other) {
   wbst_relaxed_t *t = (wbst_relaxed_t *)self->impl, *that = (wbst_relaxed_t *)other->impl;
   tree_share(t->tree.root); tree_share(that->tree.root);
   wbst_relaxed_t *res = (wbst_relaxed_t *)calloc(1, sizeof(wbst_relaxed_t)); res->wb = t->wb;
   res->tree.root = wbst_rlx_join(t->wb, t->tree.root, that->tree.root, t->tree.size, that->tree.size);
   res->tree.size = t->tree.size + that->tree.size; return (list_t){res, self->vt};
}
const list_vtable_t wbst_relaxed_vtable = { "WBSTRelaxed", wbst_rlx_vt_new, wbst_rlx_vt_clone, wbst_rlx_vt_free, wbst_rlx_vt_size, wbst_rlx_vt_each, wbst_rlx_vt_verify, wbst_rlx_vt_select, wbst_rlx_vt_update, wbst_rlx_vt_insert, wbst_rlx_vt_delete, wbst_rlx_vt_split, wbst_rlx_vt_join };

