#include "avl_weak_bottomup.h"
#include "avl_bottomup.h"
#include "rank.h"

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


// AVLWeakBottomUp
// ═══════════════════════════════════════════════════════════════════════════════

static node_t *avlw_rebalanceOnDelete(node_t *p) {
   if (isLeaf(p) && isTwoTwo(p)) { demote(p); return p; }
   if (isThreeChild(p, p->r)) {
      if (isTwoChild(p, p->l)) { demote(p); }
      else if (isTwoTwo(p->l)) { demote(p->l); demote(p); }
      else if (isOneChild(p->l, p->l->l)) {
         treeRotateR(&p); promote(p); demote(p->r);
         if (p->r->l == NULL) { assert_bst(isTwoTwo(p->r)); demote(p->r); }
      } else {
         treeRotateLR(&p); promote(p); promote(p); demote(p->l); demote(p->r); demote(p->r);
      }
   } else if (isThreeChild(p, p->l)) {
      if (isTwoChild(p, p->r)) { demote(p); }
      else if (isTwoTwo(p->r)) { demote(p->r); demote(p); }
      else if (isOneChild(p->r, p->r->r)) {
         treeRotateL(&p); promote(p); demote(p->l);
         if (p->l->r == NULL) { assert_bst(isTwoTwo(p->l)); demote(p->l); }
      } else {
         treeRotateRL(&p); promote(p); promote(p); demote(p->l); demote(p->l); demote(p->r);
      }
   }
   return p;
}

static node_t *avlw_extractMin(node_t *p, node_t **min) {
   if (p->l == NULL) { *min = tree_replacedByRightSubtree(&p); return p; }
   tree_persist(&p); p->s--; p->l = avlw_extractMin(p->l, min);
   return avlw_rebalanceOnDelete(p);
}

static node_t *avlw_extractMax(node_t *p, node_t **max) {
   if (p->r == NULL) { *max = tree_replacedByLeftSubtree(&p); return p; }
   tree_persist(&p); p->r = avlw_extractMax(p->r, max);
   return avlw_rebalanceOnDelete(p);
}

static node_t *avlw_buildL(node_t *l, node_t *p, node_t *r, u64 sl) {
   if (rankDifference(r, l) <= 1) {
      p->l = l; p->r = r; p->s = sl; p->y = (u64)(rank_of(r) + 1); return p;
   }
   tree_persist(&r); r->s = sizeL(r) + sl + 1;
   r->l = avlw_buildL(l, p, r->l, sl);
   return avl_balanceInsertL(r);
}

static node_t *avlw_buildR(node_t *l, node_t *p, node_t *r, u64 sl) {
   if (rankDifference(l, r) <= 1) {
      p->l = l; p->r = r; p->s = sl; p->y = (u64)(rank_of(l) + 1); return p;
   }
   tree_persist(&l); l->r = avlw_buildR(l->r, p, r, sizeR(l, sl));
   return avl_balanceInsertR(l);
}

node_t *avlw_build(node_t *l, node_t *p, node_t *r, u64 sl) {
   if (rank_of(l) < rank_of(r)) return avlw_buildL(l, p, r, sl);
   else return avlw_buildR(l, p, r, sl);
}

node_t *avlw_join(node_t *l, node_t *r, u64 sl, u64 sr) {
   node_t *p = NULL;
   if (l == NULL) return r;
   if (r == NULL) return l;
   if (rank_of(l) < rank_of(r) || (rank_of(l) == rank_of(r) && sl <= sr)) {
      node_t *r2 = avlw_extractMin(r, &p);
      return avlw_build(l, p, r2, sl);
   } else {
      node_t *l2 = avlw_extractMax(l, &p);
      return avlw_build(l2, p, r, sl - 1);
   }
}

static node_t *avlw_delete(node_t *p, u64 s, u64 i, u64 *x) {
   u64 sl = sizeL(p), sr = sizeR(p, s);
   if (i == p->s) {
      *x = p->x; tree_share(p->l); tree_share(p->r);
      node_t *result = avlw_join(p->l, p->r, sl, sr);
      tree_release(p); return result;
   }
   tree_persist(&p);
   if (i < p->s) { p->s--; p->l = avlw_delete(p->l, sl, i, x); }
   else { p->r = avlw_delete(p->r, sr, i - p->s - 1, x); }
   return avlw_rebalanceOnDelete(p);
}

void avlw_split(node_t *p, u64 i, u64 s, node_t **out_l, node_t **out_r) {
   if (p == NULL) { *out_l = NULL; *out_r = NULL; return; }
   tree_persist(&p);
   u64 sl = p->s, sr = s - p->s - 1;
   if (i <= p->s) {
      node_t *l, *r; avlw_split(p->l, i, sl, &l, &r);
      *out_l = l; *out_r = avlw_build(r, p, p->r, sl - i);
   } else {
      node_t *l, *r; avlw_split(p->r, i - sl - 1, sr, &l, &r);
      *out_l = avlw_build(p->l, p, l, sl); *out_r = r;
   }
}

TREE_VT_SIMPLE(avl_weak_bottomup_t, avlw_bu)

static void avlw_bu_vt_verify(list_t *self) {
   avl_weak_bottomup_t *t = (avl_weak_bottomup_t *)self->impl;
   tree_verify_size(t->tree.root, t->tree.size);
   verifyAVLWeakRanks(t->tree.root);
   verifyAVLWeakHeight(t->tree.root, t->tree.size);
}
static void avlw_bu_vt_insert(list_t *self, u64 i, u64 x) {
   avl_weak_bottomup_t *t = (avl_weak_bottomup_t *)self->impl;
   // Uses AVLBottomUp's insert (which uses avl_balanceInsertL/R)
   t->tree.root = avl_bu_insert_op(t->tree.root, i, x);
   t->tree.size++;
}
static u64 avlw_bu_vt_delete(list_t *self, u64 i) {
   avl_weak_bottomup_t *t = (avl_weak_bottomup_t *)self->impl;
   assert_bst(i < t->tree.size); u64 x;
   t->tree.root = avlw_delete(t->tree.root, t->tree.size, i, &x);
   t->tree.size--; return x;
}
static void avlw_bu_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   avl_weak_bottomup_t *t = (avl_weak_bottomup_t *)self->impl;
   tree_share(t->tree.root); node_t *l, *r;
   avlw_split(t->tree.root, i, t->tree.size, &l, &r);
   avl_weak_bottomup_t *sl = (avl_weak_bottomup_t *)calloc(1, sizeof(avl_weak_bottomup_t)); sl->tree = (tree_t){l, i};
   avl_weak_bottomup_t *sr = (avl_weak_bottomup_t *)calloc(1, sizeof(avl_weak_bottomup_t)); sr->tree = (tree_t){r, t->tree.size - i};
   *out_l = (list_t){sl, self->vt}; *out_r = (list_t){sr, self->vt};
}
static list_t avlw_bu_vt_join(list_t *self, list_t *other) {
   avl_weak_bottomup_t *t = (avl_weak_bottomup_t *)self->impl, *that = (avl_weak_bottomup_t *)other->impl;
   tree_share(t->tree.root); tree_share(that->tree.root);
   avl_weak_bottomup_t *res = (avl_weak_bottomup_t *)calloc(1, sizeof(avl_weak_bottomup_t));
   res->tree.root = avlw_join(t->tree.root, that->tree.root, t->tree.size, that->tree.size);
   res->tree.size = t->tree.size + that->tree.size;
   return (list_t){res, self->vt};
}

// avl_bu_insert_op is defined in avl_bottomup.c and declared in avl_bottomup.h

const list_vtable_t avl_weak_bottomup_vtable = {
   "AVLWeakBottomUp", avlw_bu_vt_new, avlw_bu_vt_clone, avlw_bu_vt_free,
   avlw_bu_vt_size, avlw_bu_vt_each, avlw_bu_vt_verify,
   avlw_bu_vt_select, avlw_bu_vt_update, avlw_bu_vt_insert, avlw_bu_vt_delete,
   avlw_bu_vt_split, avlw_bu_vt_join,
};


// ═══════════════════════════════════════════════════════════════════════════════
