#include "redblack_topdown.h"
#include "redblack_bottomup.h"


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

void rb_td_insert(node_t **p, u64 i, u64 x) {
   if (*p == NULL) { tree_attach(p, x); return; }
   tree_persist(p);
   if (isRedRed(*p)) promote(*p);
   for (;;) {
      assert_bst(!isRedRed(*p));
      if (i <= (*p)->s) {
         if ((*p)->l == NULL) { tree_attachL(*p, x); return; }
         if (isRedRed((*p)->l)) { tree_pathLeft(&p); promote(*p); continue; }
         if (!isRed(*p, (*p)->l)) { tree_pathLeft(&p); continue; }
         assert_bst(isRed(*p, (*p)->l) && !isRedRed((*p)->l));
         if (i <= (*p)->l->s) {
            if ((*p)->l->l == NULL) { tree_attachLL(*p, x); if (isRed((*p)->l, (*p)->l->l)) treeRotateR(p); return; }
            if (!isBlack((*p)->l, (*p)->l->l)) { tree_pathLeft(&p); tree_pathLeft(&p); promote(*p); continue; }
            if (!isRedRed((*p)->l->l)) { tree_pathLeft(&p); tree_pathLeft(&p); continue; }
            treeRotateR(p); tree_pathLeft(&p); promote(*p); continue;
         } else {
            if ((*p)->l->r == NULL) { tree_attachLR(*p, x); if (isRed((*p)->l, (*p)->l->r)) treeRotateLR(p); return; }
            if (!isRedRed((*p)->l->r)) { tree_pathLeft(&p); tree_pathRight(&p, &i); continue; }
            if (!isBlack((*p)->l, (*p)->l->r)) { tree_pathLeft(&p); tree_pathRight(&p, &i); promote(*p); continue; }
            treeRotateLR(p); promote(*p);
            if (i <= (*p)->s) tree_pathLeft(&p); else tree_pathRight(&p, &i);
         }
      } else {
         if ((*p)->r == NULL) { tree_attachR(*p, x); return; }
         if (!isRedRed((*p)->r) && !isRed(*p, (*p)->r)) { tree_pathRight(&p, &i); continue; }
         if (isRedRed((*p)->r)) { tree_pathRight(&p, &i); promote(*p); continue; }
         if (i > (*p)->s + (*p)->r->s + 1) {
            if ((*p)->r->r == NULL) { tree_attachRR(*p, x); if (isRed((*p)->r, (*p)->r->r)) treeRotateL(p); return; }
            if (!isRedRed((*p)->r->r)) { tree_pathRight(&p, &i); tree_pathRight(&p, &i); continue; }
            if (!isOneChild((*p)->r, (*p)->r->r)) { tree_pathRight(&p, &i); tree_pathRight(&p, &i); promote(*p); continue; }
            treeRotateL(p); tree_pathRight(&p, &i); promote(*p); continue;
         } else {
            if ((*p)->r->l == NULL) { tree_attachRL(*p, x); if (isRed((*p)->r, (*p)->r->l)) treeRotateRL(p); return; }
            if (!isRedRed((*p)->r->l)) { tree_pathRight(&p, &i); tree_pathLeft(&p); continue; }
            if (!isBlack((*p)->r, (*p)->r->l)) { tree_pathRight(&p, &i); tree_pathLeft(&p); promote(*p); continue; }
            treeRotateRL(p); promote(*p);
            if (i > (*p)->s) tree_pathRight(&p, &i); else tree_pathLeft(&p);
         }
      }
   }
}

TREE_VT_SIMPLE(redblack_topdown_t, rb_td)
static void rb_td_vt_verify(list_t *self) { redblack_topdown_t *t = (redblack_topdown_t *)self->impl; tree_verify_size(t->tree.root, t->tree.size); verifyRedBlackRanks(t->tree.root); verifyRedBlackHeight(t->tree.root, t->tree.size); }
static void rb_td_vt_insert(list_t *self, u64 i, u64 x) { redblack_topdown_t *t = (redblack_topdown_t *)self->impl; t->tree.size++; rb_td_insert(&t->tree.root, i, x); }
static u64 rb_td_vt_delete(list_t *self, u64 i) {
   redblack_topdown_t *t = (redblack_topdown_t *)self->impl;
   assert_bst(i < t->tree.size); u64 x;
   t->tree.root = rb_bu_delete(t->tree.root, t->tree.size, i, &x); t->tree.size--; return x;
}
static void rb_td_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   redblack_topdown_t *t = (redblack_topdown_t *)self->impl; tree_share(t->tree.root); node_t *l, *r;
   rb_split(t->tree.root, t->tree.size, i, &l, &r);
   redblack_topdown_t *sl = (redblack_topdown_t *)calloc(1, sizeof(redblack_topdown_t)); sl->tree = (tree_t){l, i};
   redblack_topdown_t *sr = (redblack_topdown_t *)calloc(1, sizeof(redblack_topdown_t)); sr->tree = (tree_t){r, t->tree.size - i};
   *out_l = (list_t){sl, self->vt}; *out_r = (list_t){sr, self->vt};
}
static list_t rb_td_vt_join(list_t *self, list_t *other) {
   redblack_topdown_t *t = (redblack_topdown_t *)self->impl, *that = (redblack_topdown_t *)other->impl;
   tree_share(t->tree.root); tree_share(that->tree.root);
   redblack_topdown_t *res = (redblack_topdown_t *)calloc(1, sizeof(redblack_topdown_t));
   res->tree.root = rb_join(t->tree.root, that->tree.root, t->tree.size, that->tree.size);
   res->tree.size = t->tree.size + that->tree.size; return (list_t){res, self->vt};
}
const list_vtable_t redblack_topdown_vtable = {
   "RedBlackTopDown", rb_td_vt_new, rb_td_vt_clone, rb_td_vt_free,
   rb_td_vt_size, rb_td_vt_each, rb_td_vt_verify,
   rb_td_vt_select, rb_td_vt_update, rb_td_vt_insert, rb_td_vt_delete,
   rb_td_vt_split, rb_td_vt_join,
};


// ═══════════════════════════════════════════════════════════════════════════════
