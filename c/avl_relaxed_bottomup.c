#include "avl_relaxed_bottomup.h"
#include "avl_bottomup.h"
#include "rank.h"

// Forward declarations
static node_t *avlr_buildL(node_t *l, node_t *p, node_t *r, u64 sl);
static node_t *avlr_buildR(node_t *l, node_t *p, node_t *r, u64 sl);

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


// AVLRelaxedBottomUp  (uses AVLBottomUp insert, base Tree delete)
// ═══════════════════════════════════════════════════════════════════════════════

static node_t *avlr_buildL(node_t *l, node_t *p, node_t *r, u64 sl) {
   if (rank_of(l) - rank_of(r) <= 1) {
      p->l = l; p->r = r; p->s = sl; p->y = (u64)(rank_of(l) + 1); return p;
   }
   tree_persist(&l); l->r = avlr_build(l->r, p, r, sl - l->s - 1);
   return avl_balanceInsertR(l);
}

static node_t *avlr_buildR(node_t *l, node_t *p, node_t *r, u64 sl) {
   if (rank_of(r) - rank_of(l) <= 1) {
      p->l = l; p->r = r; p->s = sl; p->y = (u64)(rank_of(r) + 1); return p;
   }
   tree_persist(&r); r->s = 1 + sl + r->s;
   r->l = avlr_build(l, p, r->l, sl);
   return avl_balanceInsertL(r);
}

node_t *avlr_build(node_t *l, node_t *p, node_t *r, u64 sl) {
   if (rank_of(l) <= rank_of(r)) return avlr_buildR(l, p, r, sl);
   else return avlr_buildL(l, p, r, sl);
}

node_t *avlr_join(node_t *l, node_t *r, u64 sl, u64 sr) {
   if (l == NULL) return r;
   if (r == NULL) return l;
   if (rank_of(l) < rank_of(r) || (rank_of(l) == rank_of(r) && sl <= sr)) {
      node_t *p = tree_deleteMin(&r);
      return avlr_build(l, p, r, sl);
   } else {
      node_t *p = tree_deleteMax(&l);
      return avlr_build(l, p, r, sl - 1);
   }
}

void avlr_split(node_t *p, u64 i, u64 s, node_t **out_l, node_t **out_r) {
   if (p == NULL) { *out_l = NULL; *out_r = NULL; return; }
   tree_persist(&p);
   u64 sl = p->s, sr = s - p->s - 1;
   if (i <= p->s) {
      node_t *l, *r; avlr_split(p->l, i, sl, &l, &r);
      *out_l = l; *out_r = avlr_build(r, p, p->r, sl - i);
   } else {
      node_t *l, *r; avlr_split(p->r, i - sl - 1, sr, &l, &r);
      *out_l = avlr_build(p->l, p, l, sl); *out_r = r;
   }
}

TREE_VT_SIMPLE(avl_relaxed_bottomup_t, avlr_bu)

static void avlr_bu_vt_verify(list_t *self) {
   avl_relaxed_bottomup_t *t = (avl_relaxed_bottomup_t *)self->impl;
   tree_verify_size(t->tree.root, t->tree.size);
   verifyAVLRelaxedRanks(t->tree.root);
}
static void avlr_bu_vt_insert(list_t *self, u64 i, u64 x) {
   avl_relaxed_bottomup_t *t = (avl_relaxed_bottomup_t *)self->impl;
   t->tree.root = avl_bu_insert_op(t->tree.root, i, x); t->tree.size++;
}
static u64 avlr_bu_vt_delete(list_t *self, u64 i) {
   return tree_delete_base(&((avl_relaxed_bottomup_t *)self->impl)->tree, i);
}
static void avlr_bu_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   avl_relaxed_bottomup_t *t = (avl_relaxed_bottomup_t *)self->impl;
   tree_share(t->tree.root); node_t *l, *r;
   avlr_split(t->tree.root, i, t->tree.size, &l, &r);
   avl_relaxed_bottomup_t *sl = (avl_relaxed_bottomup_t *)calloc(1, sizeof(avl_relaxed_bottomup_t)); sl->tree = (tree_t){l, i};
   avl_relaxed_bottomup_t *sr = (avl_relaxed_bottomup_t *)calloc(1, sizeof(avl_relaxed_bottomup_t)); sr->tree = (tree_t){r, t->tree.size - i};
   *out_l = (list_t){sl, self->vt}; *out_r = (list_t){sr, self->vt};
}
static list_t avlr_bu_vt_join(list_t *self, list_t *other) {
   avl_relaxed_bottomup_t *t = (avl_relaxed_bottomup_t *)self->impl, *that = (avl_relaxed_bottomup_t *)other->impl;
   tree_share(t->tree.root); tree_share(that->tree.root);
   avl_relaxed_bottomup_t *res = (avl_relaxed_bottomup_t *)calloc(1, sizeof(avl_relaxed_bottomup_t));
   res->tree.root = avlr_join(t->tree.root, that->tree.root, t->tree.size, that->tree.size);
   res->tree.size = t->tree.size + that->tree.size;
   return (list_t){res, self->vt};
}
const list_vtable_t avl_relaxed_bottomup_vtable = {
   "AVLRelaxedBottomUp", avlr_bu_vt_new, avlr_bu_vt_clone, avlr_bu_vt_free,
   avlr_bu_vt_size, avlr_bu_vt_each, avlr_bu_vt_verify,
   avlr_bu_vt_select, avlr_bu_vt_update, avlr_bu_vt_insert, avlr_bu_vt_delete,
   avlr_bu_vt_split, avlr_bu_vt_join,
};


// ═══════════════════════════════════════════════════════════════════════════════
