#include "avl_weak_topdown.h"
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


// AVLWeakTopDown
// ═══════════════════════════════════════════════════════════════════════════════

static node_t *avlw_td_rebalanceOnDelete(node_t *p) {
   if (isThreeChild(p, p->r)) {
      if (isOneChild(p->l, p->l->l)) {
         treeRotateR(&p); promote(p); demote(p->r);
         if (p->r->l == NULL) { assert_bst(isTwoTwo(p->r)); demote(p->r); }
      } else {
         treeRotateLR(&p); promote(p); promote(p); demote(p->l); demote(p->r); demote(p->r);
      }
   } else if (isThreeChild(p, p->l)) {
      if (isOneChild(p->r, p->r->r)) {
         treeRotateL(&p); promote(p); demote(p->l);
         if (p->l->r == NULL) { assert_bst(isTwoTwo(p->l)); demote(p->l); }
      } else {
         treeRotateRL(&p); promote(p); promote(p); demote(p->l); demote(p->l); demote(p->r);
      }
   }
   return p;
}

static void avlw_td_rebalanceAfterDissolve(node_t **g, node_t **p) {
   if (isLeaf(*p) && isTwoTwo(*p)) { demote(*p); }
   *g = avlw_td_rebalanceOnDelete(*g);
   *p = avlw_td_rebalanceOnDelete(*p);
}

static bool avlw_td_resetSafeNode(node_t *p) {
   if (isTwoTwo(p)) { demote(p); return true; }
   if (isTwoChild(p, p->l) && isTwoTwo(p->r)) {
      tree_persist(&p->r); demote(p); demote(p->r); return true;
   }
   if (isTwoChild(p, p->r) && isTwoTwo(p->l)) {
      tree_persist(&p->l); demote(p); demote(p->l); return true;
   }
   return false;
}

static void avlw_td_dissolve(node_t **p, u64 *x) {
   node_t *old = *p;
   tree_share(old->l); tree_share(old->r);
   *x = old->x;
   *p = avlw_join(old->l, old->r, old->s, old->s);
   tree_release(old);
}

static u64 avlw_td_delete(node_t **p, u64 i) {
   u64 x;
   if ((*p)->s == i) { avlw_td_dissolve(p, &x); return x; }
   node_t **g = p;
   for (;;) {
      tree_persist(p);
      if (avlw_td_resetSafeNode(*p)) { *g = avlw_td_rebalanceOnDelete(*g); }
      if (i < (*p)->s) {
         deleteL(*p);
         if ((*p)->l->s == i) { avlw_td_dissolve(&(*p)->l, &x); avlw_td_rebalanceAfterDissolve(g, p); return x; }
         g = p; p = &(*p)->l;
      } else {
         deleteR(*p, &i);
         if ((*p)->r->s == i) { avlw_td_dissolve(&(*p)->r, &x); avlw_td_rebalanceAfterDissolve(g, p); return x; }
         g = p; p = &(*p)->r;
      }
   }
}

static node_t *avlw_td_tdDeleteMin(node_t **p) {
   node_t **g = p;
   if ((*p)->l == NULL) return tree_replacedByRightSubtree(p);
   for (;;) {
      tree_persist(p);
      if (avlw_td_resetSafeNode(*p)) { *g = avlw_td_rebalanceOnDelete(*g); }
      node_t **l = deleteL(*p);
      if ((*p)->l->l == NULL) {
         node_t *min = tree_replacedByRightSubtree(l);
         avlw_td_rebalanceAfterDissolve(g, p);
         return min;
      }
      g = p; p = l;
   }
}

static node_t *avlw_td_tdDeleteMax(node_t **p, u64 s) {
   node_t **g = p;
   if ((*p)->r == NULL) return tree_replacedByLeftSubtree(p);
   for (;;) {
      tree_persist(p);
      if (avlw_td_resetSafeNode(*p)) { *g = avlw_td_rebalanceOnDelete(*g); }
      node_t **r = pathDeletingRightIgnoringIndex(*p);
      if ((*p)->r->r == NULL) {
         node_t *max = tree_replacedByLeftSubtree(r);
         avlw_td_rebalanceAfterDissolve(g, p);
         return max;
      }
      g = p; p = r;
   }
}

static node_t *avlw_td_join(node_t *l, node_t *r, u64 sl, u64 sr) {
   if (l == NULL) return r;
   if (r == NULL) return l;
   if (rank_of(l) < rank_of(r) || (rank_of(l) == rank_of(r) && sl <= sr)) {
      node_t *p = avlw_td_tdDeleteMin(&r);
      return avlw_build(l, p, r, sl);
   } else {
      node_t *p = avlw_td_tdDeleteMax(&l, sl);
      return avlw_build(l, p, r, sl - 1);
   }
}

void avlw_td_insert(node_t **p, u64 i, u64 x) {
   if (*p == NULL) { tree_attach(p, x); return; }
   tree_persist(p);
   if (isOneOne(*p)) promote(*p);
   for (;;) {
      assert_bst(!isOneOne(*p));
      if (i <= (*p)->s) {
         if ((*p)->l == NULL) { tree_attachL(*p, x); return; }
         if (!isOneOne((*p)->l)) { tree_pathLeft(&p); continue; }
         if (!isOneChild(*p, (*p)->l)) { tree_pathLeft(&p); promote(*p); continue; }
         if (i <= (*p)->l->s) {
            if ((*p)->l->l == NULL) { tree_attachLL(*p, x); treeRotateR(p); promote(*p); demote((*p)->r); return; }
            if (!isOneOne((*p)->l->l)) { tree_pathLeft(&p); tree_pathLeft(&p); continue; }
            treeRotateR(p); promote(*p); demote((*p)->r); tree_pathLeft(&p); promote(*p); continue;
         } else {
            if ((*p)->l->r == NULL) { tree_attachLR(*p, x); treeRotateLR(p); promote(*p); demote((*p)->r); return; }
            if (!isOneOne((*p)->l->r)) { tree_pathLeft(&p); tree_pathRight(&p, &i); continue; }
            treeRotateLR(p); promote(*p); promote(*p); demote((*p)->r);
            if (i <= (*p)->s) tree_pathLeft(&p); else tree_pathRight(&p, &i);
         }
      } else {
         if ((*p)->r == NULL) { tree_attachR(*p, x); return; }
         if (!isOneOne((*p)->r)) { tree_pathRight(&p, &i); continue; }
         if (!isOneChild(*p, (*p)->r)) { tree_pathRight(&p, &i); promote(*p); continue; }
         if (i > (*p)->s + (*p)->r->s + 1) {
            if ((*p)->r->r == NULL) { tree_attachRR(*p, x); treeRotateL(p); promote(*p); demote((*p)->l); return; }
            if (!isOneOne((*p)->r->r)) { tree_pathRight(&p, &i); tree_pathRight(&p, &i); continue; }
            treeRotateL(p); promote(*p); demote((*p)->l); tree_pathRight(&p, &i); promote(*p); continue;
         } else {
            if ((*p)->r->l == NULL) { tree_attachRL(*p, x); treeRotateRL(p); demote((*p)->l); promote(*p); return; }
            if (!isOneOne((*p)->r->l)) { tree_pathRight(&p, &i); tree_pathLeft(&p); continue; }
            treeRotateRL(p); promote(*p); promote(*p); demote((*p)->l);
            if (i > (*p)->s) tree_pathRight(&p, &i); else tree_pathLeft(&p);
         }
      }
   }
}

static void avlw_td_split(node_t *p, u64 i, u64 s, node_t **out_l, node_t **out_r) {
   if (p == NULL) { *out_l = NULL; *out_r = NULL; return; }
   tree_persist(&p);
   u64 sl = p->s, sr = s - p->s - 1;
   if (i <= p->s) {
      node_t *l, *r; avlw_td_split(p->l, i, sl, &l, &r);
      *out_l = l; *out_r = avlw_build(r, p, p->r, sl - i);
   } else {
      node_t *l, *r; avlw_td_split(p->r, i - sl - 1, sr, &l, &r);
      *out_l = avlw_build(p->l, p, l, sl); *out_r = r;
   }
}

TREE_VT_SIMPLE(avl_weak_topdown_t, avlw_td)

static void avlw_td_vt_verify(list_t *self) {
   avl_weak_topdown_t *t = (avl_weak_topdown_t *)self->impl;
   tree_verify_size(t->tree.root, t->tree.size);
   verifyAVLWeakRanks(t->tree.root);
   verifyAVLWeakHeight(t->tree.root, t->tree.size);
}
static void avlw_td_vt_insert(list_t *self, u64 i, u64 x) {
   avl_weak_topdown_t *t = (avl_weak_topdown_t *)self->impl;
   avlw_td_insert(&t->tree.root, i, x); t->tree.size++;
}
static u64 avlw_td_vt_delete(list_t *self, u64 i) {
   avl_weak_topdown_t *t = (avl_weak_topdown_t *)self->impl;
   assert_bst(i < t->tree.size);
   u64 x = avlw_td_delete(&t->tree.root, i);
   t->tree.size--; return x;
}
static void avlw_td_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   avl_weak_topdown_t *t = (avl_weak_topdown_t *)self->impl;
   tree_share(t->tree.root); node_t *l, *r;
   avlw_td_split(t->tree.root, i, t->tree.size, &l, &r);
   avl_weak_topdown_t *sl = (avl_weak_topdown_t *)calloc(1, sizeof(avl_weak_topdown_t)); sl->tree = (tree_t){l, i};
   avl_weak_topdown_t *sr = (avl_weak_topdown_t *)calloc(1, sizeof(avl_weak_topdown_t)); sr->tree = (tree_t){r, t->tree.size - i};
   *out_l = (list_t){sl, self->vt}; *out_r = (list_t){sr, self->vt};
}
static list_t avlw_td_vt_join(list_t *self, list_t *other) {
   avl_weak_topdown_t *t = (avl_weak_topdown_t *)self->impl, *that = (avl_weak_topdown_t *)other->impl;
   tree_share(t->tree.root); tree_share(that->tree.root);
   avl_weak_topdown_t *res = (avl_weak_topdown_t *)calloc(1, sizeof(avl_weak_topdown_t));
   res->tree.root = avlw_td_join(t->tree.root, that->tree.root, t->tree.size, that->tree.size);
   res->tree.size = t->tree.size + that->tree.size;
   return (list_t){res, self->vt};
}
const list_vtable_t avl_weak_topdown_vtable = {
   "AVLWeakTopDown", avlw_td_vt_new, avlw_td_vt_clone, avlw_td_vt_free,
   avlw_td_vt_size, avlw_td_vt_each, avlw_td_vt_verify,
   avlw_td_vt_select, avlw_td_vt_update, avlw_td_vt_insert, avlw_td_vt_delete,
   avlw_td_vt_split, avlw_td_vt_join,
};


// ═══════════════════════════════════════════════════════════════════════════════
