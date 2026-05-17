#include "wbst_topdown.h"
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

static void wbst_td_pathL(node_t ***p, u64 *s) { *s = (**p)->s; (**p)->s++; *p = &(**p)->l; }
static void wbst_td_pathR(node_t ***p, u64 *s, u64 *i) { *s = *s - (**p)->s - 1; *i = *i - (**p)->s - 1; *p = &(**p)->r; }
static void wbst_td_deleteL(node_t ***p, u64 *s) { *s = (**p)->s; (**p)->s--; *p = &(**p)->l; }
static void wbst_td_deleteR(node_t ***p, u64 *s, u64 *i) { *s = *s - (**p)->s - 1; *i = *i - (**p)->s - 1; *p = &(**p)->r; }

static void wbst_td_rebalanceR(weight_balance_t wb, node_t **p, u64 sr) {
   if (wb.singleRotation(sr - (*p)->r->s - 1, (*p)->r->s)) treeRotateL(p); else treeRotateRL(p);
}
static void wbst_td_rebalanceL(weight_balance_t wb, node_t **p, u64 sl) {
   if (wb.singleRotation((*p)->l->s, sl - (*p)->l->s - 1)) treeRotateR(p); else treeRotateLR(p);
}

void wbst_td_insert(weight_balance_t wb, node_t **p, u64 s, u64 i, u64 x) {
   for (;;) {
      if (*p == NULL) { *p = tree_allocate((node_t){.x = x}); return; }
      tree_persist(p);
      u64 sl = sizeL(*p), sr = sizeR(*p, s);
      if (i <= (*p)->s) {
         if (wb.isBalanced(sr, sl + 1)) { wbst_td_pathL(&p, &s); }
         else if (i <= (*p)->l->s) {
            if (wb.singleRotation(sizeL((*p)->l) + 1, sizeR((*p)->l, sl))) { treeRotateR(p); wbst_td_pathL(&p, &s); }
            else { treeRotateLR(p); wbst_td_pathL(&p, &s); wbst_td_pathL(&p, &s); }
         } else {
            if (wb.singleRotation(sizeL((*p)->l), sizeR((*p)->l, sl) + 1)) { treeRotateR(p); wbst_td_pathR(&p, &s, &i); wbst_td_pathL(&p, &s); }
            else {
               if (i <= (*p)->l->s + (*p)->l->r->s + 1) { treeRotateLR(p); wbst_td_pathL(&p, &s); wbst_td_pathR(&p, &s, &i); }
               else { treeRotateLR(p); wbst_td_pathR(&p, &s, &i); wbst_td_pathL(&p, &s); }
            }
         }
      } else {
         if (wb.isBalanced(sl, sr + 1)) { wbst_td_pathR(&p, &s, &i); continue; }
         if (i > (*p)->s + (*p)->r->s + 1) {
            if (wb.singleRotation(sizeR((*p)->r, sr) + 1, sizeL((*p)->r))) { treeRotateL(p); wbst_td_pathR(&p, &s, &i); }
            else { treeRotateRL(p); wbst_td_pathR(&p, &s, &i); wbst_td_pathR(&p, &s, &i); }
         } else {
            if (wb.singleRotation(sizeR((*p)->r, sr), sizeL((*p)->r) + 1)) { treeRotateL(p); wbst_td_pathL(&p, &s); wbst_td_pathR(&p, &s, &i); }
            else {
               if (i > (*p)->s + (*p)->r->l->s + 1) { treeRotateRL(p); wbst_td_pathR(&p, &s, &i); wbst_td_pathL(&p, &s); }
               else { treeRotateRL(p); wbst_td_pathL(&p, &s); wbst_td_pathR(&p, &s, &i); }
            }
         }
      }
   }
}

void wbst_td_delete_internal(weight_balance_t wb, node_t **p, u64 s, u64 i, node_t **x) {
   for (;;) {
      u64 sl = (*p)->s, sr = s - (*p)->s - 1;
      if (i == (*p)->s) {
         node_t *old = *p;
         node_t *l = old->l;
         node_t *r = old->r;
         if (tree_shared(old)) {
            tree_share(l);
            tree_share(r);
            *x = tree_allocate((node_t){.x = old->x, .y = old->y});
            *p = wbst_bu_join(wb, l, r, sl, sr);
            tree_release(old);
         } else {
            *x = old;
            *p = wbst_bu_join(wb, l, r, sl, sr);
            old->l = NULL;
            old->r = NULL;
            old->s = 0;
         }
         return;
      }
      tree_persist(p);
      if (i <= (*p)->s) {
         if (wb.isBalanced(sl - 1, sr)) { wbst_td_deleteL(&p, &s); }
         else { wbst_td_rebalanceR(wb, p, sr); wbst_td_deleteL(&p, &s); wbst_td_deleteL(&p, &s); }
      } else {
         if (wb.isBalanced(sr - 1, sl)) { wbst_td_deleteR(&p, &s, &i); }
         else { wbst_td_rebalanceL(wb, p, sl); wbst_td_deleteR(&p, &s, &i); wbst_td_deleteR(&p, &s, &i); }
      }
   }
}

static node_t *wbst_td_buildL(weight_balance_t wb, node_t *p, node_t *l, node_t *r, u64 sl, u64 sr) {
   if (wb.isBalanced(sr, sl)) { p->l = l; p->r = r; p->s = sl; return p; }
   tree_persist(&l); l->r = wbst_td_buildL(wb, p, l->r, r, sl - l->s - 1, sr);
   if (!wb.isBalanced(l->s, sl + sr - l->s)) wbst_td_rebalanceR(wb, &l, sr + sl - l->s);
   return l;
}

static node_t *wbst_td_buildR(weight_balance_t wb, node_t *p, node_t *l, node_t *r, u64 sl, u64 sr) {
   if (wb.isBalanced(sl, sr)) { p->l = l; p->r = r; p->s = sl; return p; }
   tree_persist(&r); r->l = wbst_td_buildR(wb, p, l, r->l, sl, r->s);
   r->s = 1 + sl + r->s;
   if (!wb.isBalanced(sl + sr - r->s, r->s)) wbst_td_rebalanceL(wb, &r, r->s);
   return r;
}

node_t *wbst_td_build(weight_balance_t wb, node_t *l, node_t *p, node_t *r, u64 sl, u64 sr) {
   tree_persist(&p);
   if (sl <= sr) return wbst_td_buildR(wb, p, l, r, sl, sr);
   else return wbst_td_buildL(wb, p, l, r, sl, sr);
}

node_t *wbst_td_join(weight_balance_t wb, node_t *l, node_t *r, u64 sl, u64 sr) {
   if (l == NULL) return r;
   if (r == NULL) return l;
   if (sl <= sr) {
      node_t *min; wbst_td_delete_internal(wb, &r, sr, 0, &min);
      return wbst_td_build(wb, l, min, r, sl, sr - 1);
   } else {
      node_t *max; wbst_td_delete_internal(wb, &l, sl, sl - 1, &max);
      return wbst_td_build(wb, l, max, r, sl - 1, sr);
   }
}

void wbst_td_split(weight_balance_t wb, node_t *p, u64 i, u64 s, node_t **out_l, node_t **out_r) {
   if (p == NULL) { *out_l = NULL; *out_r = NULL; return; }
   tree_persist(&p);
   u64 sl = p->s, sr = s - p->s - 1;
   if (i <= p->s) {
      node_t *l, *r; wbst_td_split(wb, p->l, i, sl, &l, &r);
      *out_l = l; *out_r = wbst_td_build(wb, r, p, p->r, sl - i, sr);
   } else {
      node_t *l, *r; wbst_td_split(wb, p->r, i - sl - 1, sr, &l, &r);
      *out_l = wbst_td_build(wb, p->l, p, l, sl, i - sl - 1); *out_r = r;
   }
}

void wbst_td_verifyBalance(weight_balance_t wb, node_t *p, u64 s) {
   if (s < 3) return;
   u64 sl = sizeL(p), sr = sizeR(p, s);
   invariant(wb.isBalanced(sl, sr));
   invariant(wb.isBalanced(sr, sl));
   wbst_td_verifyBalance(wb, p->l, sl);
   wbst_td_verifyBalance(wb, p->r, sr);
}

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


WB_VT_BODY(wbst_topdown_t, wbst_td, wb_three_two,
   { wbst_topdown_t *t = (wbst_topdown_t *)self->impl; wbst_td_insert(t->wb, &t->tree.root, t->tree.size, i, x); t->tree.size++; },
   { wbst_topdown_t *t = (wbst_topdown_t *)self->impl; node_t *del; wbst_td_delete_internal(t->wb, &t->tree.root, t->tree.size, i, &del); t->tree.size--; u64 x = del->x; tree_release(del); return x; },
   { WBST_TD_SPLIT(wbst_topdown_t) },
   { WBST_TD_JOIN(wbst_topdown_t) },
   { wbst_topdown_t *t = (wbst_topdown_t *)self->impl; tree_verify_size(t->tree.root, t->tree.size); wbst_td_verifyBalance(t->wb, t->tree.root, t->tree.size); invariant(height(t->tree.root) <= t->wb.maxHeight(t->tree.size)); }
)
const list_vtable_t wbst_topdown_vtable = { "WBSTTopDown", wbst_td_vt_new, wbst_td_vt_clone, wbst_td_vt_free, wbst_td_vt_size, wbst_td_vt_each, wbst_td_vt_verify, wbst_td_vt_select, wbst_td_vt_update, wbst_td_vt_insert, wbst_td_vt_delete, wbst_td_vt_split, wbst_td_vt_join };
