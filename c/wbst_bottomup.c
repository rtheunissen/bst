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

static node_t *wbst_bu_extractMin(weight_balance_t wb, node_t *p, u64 s, node_t **x) {
   tree_persist(&p);
   if (p->l == NULL) { node_t *r = p->r; p->r = NULL; *x = p; return r; }
   u64 sl = p->s, sr = s - p->s - 1;
   p->l = wbst_bu_extractMin(wb, p->l, p->s, x); p->s--;
   if (!wb.isBalanced(sl - 1, sr)) {
      u64 srl = p->r->s, srr = sr - p->r->s - 1;
      if (wb.singleRotation(srr, srl)) treeRotateL(&p);
      else treeRotateRL(&p);
   }
   return p;
}

static node_t *wbst_bu_extractMax(weight_balance_t wb, node_t *p, u64 s, node_t **x) {
   tree_persist(&p);
   if (p->r == NULL) { node_t *l = p->l; p->l = NULL; *x = p; return l; }
   u64 sl = p->s, sr = s - p->s - 1;
   p->r = wbst_bu_extractMax(wb, p->r, sr, x);
   if (!wb.isBalanced(sr - 1, sl)) {
      if (wb.singleRotation(p->l->s, sl - p->l->s - 1)) treeRotateR(&p);
      else treeRotateLR(&p);
   }
   return p;
}

static node_t *wbst_bu_buildL(weight_balance_t wb, node_t *p, node_t *l, node_t *r, u64 sl, u64 sr) {
   if (wb.isBalanced(sr, sl)) { p->l = l; p->r = r; p->s = sl; return p; }
   tree_persist(&l);
   u64 sll = l->s, slr = sl - l->s - 1;
   l->r = wbst_bu_buildL(wb, p, l->r, r, slr, sr);
   slr = 1 + sr + slr;
   if (!wb.isBalanced(sll, slr)) {
      u64 srr = slr - l->r->s - 1, srl = l->r->s;
      if (wb.singleRotation(srr, srl)) treeRotateL(&l); else treeRotateRL(&l);
   }
   return l;
}

static node_t *wbst_bu_buildR(weight_balance_t wb, node_t *p, node_t *l, node_t *r, u64 sl, u64 sr) {
   if (wb.isBalanced(sl, sr)) { p->l = l; p->r = r; p->s = sl; return p; }
   tree_persist(&r);
   u64 srl = r->s, srr = sr - r->s - 1;
   r->l = wbst_bu_buildR(wb, p, l, r->l, sl, srl);
   r->s = 1 + sl + srl;
   if (!wb.isBalanced(srr, r->s)) {
      if (wb.singleRotation(r->l->s, r->s - r->l->s - 1)) treeRotateR(&r);
      else treeRotateLR(&r);
   }
   return r;
}

node_t *wbst_bu_build(weight_balance_t wb, node_t *l, node_t *p, node_t *r, u64 sl, u64 sr) {
   if (sl <= sr) return wbst_bu_buildR(wb, p, l, r, sl, sr);
   else return wbst_bu_buildL(wb, p, l, r, sl, sr);
}

node_t *wbst_bu_join(weight_balance_t wb, node_t *l, node_t *r, u64 sl, u64 sr) {
   node_t *k = NULL;
   if (l == NULL) return r;
   if (r == NULL) return l;
   if (sl <= sr) {
      r = wbst_bu_extractMin(wb, r, sr, &k);
      return wbst_bu_build(wb, l, k, r, sl, sr - 1);
   } else {
      l = wbst_bu_extractMax(wb, l, sl, &k);
      return wbst_bu_build(wb, l, k, r, sl - 1, sr);
   }
}

node_t *wbst_bu_insert(weight_balance_t wb, node_t *p, u64 s, u64 i, u64 x) {
   if (p == NULL) return tree_allocate((node_t){.x = x});
   tree_persist(&p);
   u64 sl = p->s, sr = s - p->s - 1;
   if (i <= p->s) {
      p->l = wbst_bu_insert(wb, p->l, sl, i, x); p->s++;
      if (!wb.isBalanced(sr, sl + 1)) {
         if (!wb.singleRotation(p->l->s, p->s - p->l->s - 1)) treeRotateLR(&p);
         else treeRotateR(&p);
      }
   } else {
      p->r = wbst_bu_insert(wb, p->r, sr, i - sl - 1, x);
      if (!wb.isBalanced(sl, sr + 1)) {
         if (!wb.singleRotation(sr + 1 - p->r->s - 1, p->r->s)) treeRotateRL(&p);
         else treeRotateL(&p);
      }
   }
   return p;
}

node_t *wbst_bu_delete(weight_balance_t wb, node_t *p, u64 s, u64 i, u64 *x) {
   u64 sl = p->s, sr = s - p->s - 1;
   if (i == p->s) {
      *x = p->x;
      if (p->l == NULL) { node_t *r = p->r; tree_share(r); tree_release(p); return r; }
      if (p->r == NULL) { node_t *l = p->l; tree_share(l); tree_release(p); return l; }
      tree_persist(&p);
      if (sl > sr) {
         node_t *max; p->l = wbst_bu_extractMax(wb, p->l, sl, &max);
         p->x = max->x; p->s--;
         tree_release(max);
      } else {
         node_t *min; p->r = wbst_bu_extractMin(wb, p->r, sr, &min);
         p->x = min->x;
         tree_release(min);
      }
      return p;
   }
   tree_persist(&p);
   if (i < p->s) {
      p->l = wbst_bu_delete(wb, p->l, sl, i, x); p->s--;
      if (!wb.isBalanced(sl - 1, sr)) {
         u64 srl = p->r->s, srr = sr - p->r->s - 1;
         if (wb.singleRotation(srr, srl)) treeRotateL(&p); else treeRotateRL(&p);
      }
   } else {
      p->r = wbst_bu_delete(wb, p->r, sr, i - sl - 1, x);
      if (!wb.isBalanced(sr - 1, sl)) {
         u64 sll = p->l->s, slr = sl - p->l->s - 1;
         if (wb.singleRotation(sll, slr)) treeRotateR(&p); else treeRotateLR(&p);
      }
   }
   return p;
}

void wbst_bu_split(weight_balance_t wb, node_t *p, u64 i, u64 s, node_t **out_l, node_t **out_r) {
   if (p == NULL) { *out_l = NULL; *out_r = NULL; return; }
   tree_persist(&p);
   u64 sl = p->s, sr = s - p->s - 1;
   if (i <= p->s) {
      node_t *l, *r; wbst_bu_split(wb, p->l, i, sl, &l, &r);
      *out_l = l; *out_r = wbst_bu_build(wb, r, p, p->r, sl - i, sr);
   } else {
      node_t *l, *r; wbst_bu_split(wb, p->r, i - sl - 1, sr, &l, &r);
      *out_l = wbst_bu_build(wb, p->l, p, l, sl, i - sl - 1); *out_r = r;
   }
}

void wbst_bu_verifyBalance(weight_balance_t wb, node_t *p, u64 s) {
   if (p == NULL) return;
   u64 sl = sizeL(p), sr = sizeR(p, s);
   invariant(wb.isBalanced(sl, sr));
   invariant(wb.isBalanced(sr, sl));
   wbst_bu_verifyBalance(wb, p->l, sl);
   wbst_bu_verifyBalance(wb, p->r, sr);
}


WB_VT_BODY(wbst_bottomup_t, wbst_bu, wb_three_two,
   { wbst_bottomup_t *t = (wbst_bottomup_t *)self->impl; t->tree.root = wbst_bu_insert(t->wb, t->tree.root, t->tree.size, i, x); t->tree.size++; },
   { wbst_bottomup_t *t = (wbst_bottomup_t *)self->impl; u64 x; t->tree.root = wbst_bu_delete(t->wb, t->tree.root, t->tree.size, i, &x); t->tree.size--; return x; },
   { WBST_BU_SPLIT(wbst_bottomup_t, wbst_bu) },
   { WBST_BU_JOIN(wbst_bottomup_t, wbst_bu) },
   { wbst_bottomup_t *t = (wbst_bottomup_t *)self->impl; tree_verify_size(t->tree.root, t->tree.size); wbst_bu_verifyBalance(t->wb, t->tree.root, t->tree.size); invariant(height(t->tree.root) <= t->wb.maxHeight(t->tree.size)); }
)
const list_vtable_t wbst_bottomup_vtable = { "WBSTBottomUp", wbst_bu_vt_new, wbst_bu_vt_clone, wbst_bu_vt_free, wbst_bu_vt_size, wbst_bu_vt_each, wbst_bu_vt_verify, wbst_bu_vt_select, wbst_bu_vt_update, wbst_bu_vt_insert, wbst_bu_vt_delete, wbst_bu_vt_split, wbst_bu_vt_join };
