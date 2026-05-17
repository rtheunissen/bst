#include "redblack_bottomup.h"
#include <math.h>

// RedBlack family: BottomUp, TopDown, RelaxedBottomUp, RelaxedTopDown

#define TREE_VT_SIMPLE(TYPE, PREFIX) \
static list_t PREFIX##_vt_new(const list_vtable_t *vt) { \
   TYPE *t = (TYPE *)calloc(1, sizeof(TYPE)); return (list_t){t, vt}; } \
static list_t PREFIX##_vt_clone(list_t *self) { \
   TYPE *o = (TYPE *)self->impl; TYPE *c = (TYPE *)malloc(sizeof(TYPE)); \
   tree_share(o->tree.root); *c = *o; return (list_t){c, self->vt}; } \
static void PREFIX##_vt_free(list_t *self) { \
   TYPE *t = (TYPE *)self->impl; free_nodes(t->tree.root); free(t); self->impl = NULL; } \
static u64  PREFIX##_vt_size(list_t *self) { return ((TYPE *)self->impl)->tree.size; } \
static void PREFIX##_vt_each(list_t *self, void (*v)(u64, void*), void *c) { inorder(((TYPE *)self->impl)->tree.root, v, c); } \
static u64  PREFIX##_vt_select(list_t *self, u64 i) { return tree_lookup(&((TYPE *)self->impl)->tree, i); } \
static void PREFIX##_vt_update(list_t *self, u64 i, u64 x) { tree_update_base(&((TYPE *)self->impl)->tree, i, x); }

#include "rank.h"
#include <math.h>

// Forward declarations
node_t *rb_build(node_t *l, node_t *p, node_t *r, u64 sl);
node_t *rb_join(node_t *l, node_t *r, u64 sl, u64 sr);

// ─────────────────────────────────────────────────────────────────────────────
// Shared RedBlack balancing
// ─────────────────────────────────────────────────────────────────────────────

node_t *rb_balanceInsertL(node_t *p) {
   if (isZeroChild(p, p->l)) {
      if (isZeroChild(p, p->r)) {
         if (isZeroChild(p->l, p->l->l) || isZeroChild(p->l, p->l->r)) {
            promote(p); return p;
         }
      } else {
         if (isZeroChild(p->l, p->l->l)) { treeRotateR(&p); return p; }
         if (isZeroChild(p->l, p->l->r)) { treeRotateLR(&p); return p; }
      }
   }
   return p;
}

node_t *rb_balanceInsertR(node_t *p) {
   if (isZeroChild(p, p->r)) {
      if (isZeroChild(p, p->l)) {
         if (isZeroChild(p->r, p->r->r) || isZeroChild(p->r, p->r->l)) {
            promote(p); return p;
         }
      } else {
         if (isZeroChild(p->r, p->r->r)) { treeRotateL(&p); return p; }
         if (isZeroChild(p->r, p->r->l)) { treeRotateRL(&p); return p; }
      }
   }
   return p;
}

node_t *rb_balanceDeleteL(node_t *p) {
   if (isZeroChild(p, p->l)) { return p; }
   if (isOneChild(p, p->l))  { return p; }
   if (isZeroChild(p, p->r)) {
      assert_bst(isOneOne(p->r));
      treeRotateL(&p);
      if (isZeroChild(p->l->r, p->l->r->r)) {
         treeRotateL(&p->l); promote(p->l); demote(p->l->l); return p;
      }
      if (isZeroChild(p->l->r, p->l->r->l)) {
         treeRotateRL(&p->l); promote(p->l); demote(p->l->l); return p;
      }
      demote(p->l); return p;
   } else {
      if (isZeroChild(p->r, p->r->r)) { treeRotateL(&p); promote(p); demote(p->l); return p; }
      if (isZeroChild(p->r, p->r->l)) { treeRotateRL(&p); promote(p); demote(p->l); return p; }
      assert_bst(isOneOne(p->r)); demote(p); return p;
   }
}

node_t *rb_balanceDeleteR(node_t *p) {
   if (isZeroChild(p, p->r)) { return p; }
   if (isOneChild(p, p->r))  { return p; }
   if (isZeroChild(p, p->l)) {
      assert_bst(isOneOne(p->l));
      treeRotateR(&p);
      if (isZeroChild(p->r->l, p->r->l->l)) {
         treeRotateR(&p->r); promote(p->r); demote(p->r->r); return p;
      }
      if (isZeroChild(p->r->l, p->r->l->r)) {
         treeRotateLR(&p->r); promote(p->r); demote(p->r->r); return p;
      }
      demote(p->r); return p;
   } else {
      if (isZeroChild(p->l, p->l->l)) { treeRotateR(&p); promote(p); demote(p->r); return p; }
      if (isZeroChild(p->l, p->l->r)) { treeRotateLR(&p); promote(p); demote(p->r); return p; }
      assert_bst(isOneOne(p->l)); demote(p); return p;
   }
}


// ─────────────────────────────────────────────────────────────────────────────
// RedBlack Build / Join / Split
// ─────────────────────────────────────────────────────────────────────────────

node_t *rb_deleteMin(node_t *p, node_t **min) {
   tree_persist(&p);
   if (p->l == NULL) { node_t *r = p->r; p->r = NULL; *min = p; return r; }
   p->s--; p->l = rb_deleteMin(p->l, min);
   return rb_balanceDeleteL(p);
}

node_t *rb_deleteMax(node_t *p, node_t **max) {
   tree_persist(&p);
   if (p->r == NULL) { node_t *l = p->l; p->l = NULL; *max = p; return l; }
   p->r = rb_deleteMax(p->r, max);
   return rb_balanceDeleteR(p);
}

node_t *rb_build(node_t *l, node_t *p, node_t *r, u64 sl) {
   if (rank_of(l) == rank_of(r)) {
      p->l = l; p->r = r; p->s = sl; p->y = (u64)rank_of(l);
      if (r == NULL || hasZeroChild(l) || hasZeroChild(r)) promote(p);
      return p;
   }
   if (rank_of(l) < rank_of(r)) {
      tree_persist(&r); r->s = 1 + sl + r->s;
      r->l = rb_build(l, p, r->l, sl);
      return rb_balanceInsertL(r);
   } else {
      tree_persist(&l); l->r = rb_build(l->r, p, r, sl - l->s - 1);
      return rb_balanceInsertR(l);
   }
}

node_t *rb_join(node_t *l, node_t *r, u64 sl, u64 sr) {
   node_t *p = NULL;
   if (l == NULL) return r;
   if (r == NULL) return l;
   if (rank_of(l) < rank_of(r) || (rank_of(l) == rank_of(r) && sl <= sr)) {
      node_t *r2 = rb_deleteMin(r, &p);
      return rb_build(l, p, r2, sl);
   } else {
      node_t *l2 = rb_deleteMax(l, &p);
      return rb_build(l2, p, r, sl - 1);
   }
}

void rb_split(node_t *p, u64 s, u64 i, node_t **out_l, node_t **out_r) {
   if (p == NULL) { *out_l = NULL; *out_r = NULL; return; }
   tree_persist(&p);
   if (i <= sizeL(p)) {
      node_t *l, *r; rb_split(p->l, sizeL(p), i, &l, &r);
      *out_r = rb_build(r, p, p->r, sizeL(p) - i); *out_l = l;
   } else {
      node_t *l, *r; rb_split(p->r, sizeR(p, s), i - sizeL(p) - 1, &l, &r);
      *out_l = rb_build(p->l, p, l, sizeL(p)); *out_r = r;
   }
}


// ─────────────────────────────────────────────────────────────────────────────
// RedBlackBottomUp insert/delete
// ─────────────────────────────────────────────────────────────────────────────

node_t *rb_bu_insert(node_t *p, u64 i, u64 x) {
   if (p == NULL) return tree_allocate((node_t){.x = x});
   tree_persist(&p);
   if (i <= p->s) { p->s++; p->l = rb_bu_insert(p->l, i, x); return rb_balanceInsertL(p); }
   else { p->r = rb_bu_insert(p->r, i - p->s - 1, x); return rb_balanceInsertR(p); }
}

node_t *rb_bu_delete(node_t *p, u64 s, u64 i, u64 *x) {
   u64 sl = sizeL(p), sr = sizeR(p, s);
   if (i == sl) {
      tree_share(p->l); tree_share(p->r); *x = p->x;
      node_t *result = rb_join(p->l, p->r, sl, sr);
      tree_release(p); return result;
   }
   tree_persist(&p);
   if (i < sl) { p->s--; p->l = rb_bu_delete(p->l, sl, i, x); return rb_balanceDeleteL(p); }
   else { p->r = rb_bu_delete(p->r, sr, i - p->s - 1, x); return rb_balanceDeleteR(p); }
}


// RedBlackBottomUp
// ═══════════════════════════════════════════════════════════════════════════════

TREE_VT_SIMPLE(redblack_bottomup_t, rb_bu)
static void rb_bu_vt_verify(list_t *self) {
   redblack_bottomup_t *t = (redblack_bottomup_t *)self->impl;
   tree_verify_size(t->tree.root, t->tree.size);
   verifyRedBlackRanks(t->tree.root);
   verifyRedBlackHeight(t->tree.root, t->tree.size);
}
static void rb_bu_vt_insert(list_t *self, u64 i, u64 x) {
   redblack_bottomup_t *t = (redblack_bottomup_t *)self->impl;
   t->tree.root = rb_bu_insert(t->tree.root, i, x); t->tree.size++;
}
static u64 rb_bu_vt_delete(list_t *self, u64 i) {
   redblack_bottomup_t *t = (redblack_bottomup_t *)self->impl;
   assert_bst(i < t->tree.size); u64 x;
   t->tree.root = rb_bu_delete(t->tree.root, t->tree.size, i, &x); t->tree.size--; return x;
}
static void rb_bu_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   redblack_bottomup_t *t = (redblack_bottomup_t *)self->impl;
   tree_share(t->tree.root); node_t *l, *r;
   rb_split(t->tree.root, t->tree.size, i, &l, &r);
   redblack_bottomup_t *sl = (redblack_bottomup_t *)calloc(1, sizeof(redblack_bottomup_t)); sl->tree = (tree_t){l, i};
   redblack_bottomup_t *sr = (redblack_bottomup_t *)calloc(1, sizeof(redblack_bottomup_t)); sr->tree = (tree_t){r, t->tree.size - i};
   *out_l = (list_t){sl, self->vt}; *out_r = (list_t){sr, self->vt};
}
static list_t rb_bu_vt_join(list_t *self, list_t *other) {
   redblack_bottomup_t *t = (redblack_bottomup_t *)self->impl, *that = (redblack_bottomup_t *)other->impl;
   tree_share(t->tree.root); tree_share(that->tree.root);
   redblack_bottomup_t *res = (redblack_bottomup_t *)calloc(1, sizeof(redblack_bottomup_t));
   res->tree.root = rb_join(t->tree.root, that->tree.root, t->tree.size, that->tree.size);
   res->tree.size = t->tree.size + that->tree.size;
   return (list_t){res, self->vt};
}
const list_vtable_t redblack_bottomup_vtable = {
   "RedBlackBottomUp", rb_bu_vt_new, rb_bu_vt_clone, rb_bu_vt_free,
   rb_bu_vt_size, rb_bu_vt_each, rb_bu_vt_verify,
   rb_bu_vt_select, rb_bu_vt_update, rb_bu_vt_insert, rb_bu_vt_delete,
   rb_bu_vt_split, rb_bu_vt_join,
};


// ═══════════════════════════════════════════════════════════════════════════════
