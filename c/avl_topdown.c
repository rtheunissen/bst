#include "avl_topdown.h"
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


static node_t *avl_td_balance(node_t *p) {
   if (isTwoTwo(p)) { demote(p); return p; }
   if (isThreeChild(p, p->l)) {
      if (isTwoChild(p->r, p->r->r)) {
         treeRotateRL(&p); promote(p); demote(p->r); demote(p->l); demote(p->l);
      } else {
         treeRotateL(&p); demote(p->l); demote(p->l);
      }
   } else if (isThreeChild(p, p->r)) {
      if (isTwoChild(p->l, p->l->l)) {
         treeRotateLR(&p); promote(p); demote(p->l); demote(p->r); demote(p->r);
      } else {
         treeRotateR(&p); demote(p->r); demote(p->r);
      }
   }
   return p;
}

static void avl_td_insert(node_t **p, u64 i, u64 x) {
   if (*p == NULL) { tree_attach(p, x); return; }
   tree_persist(p);
   node_t **a = p;
   u64 j = i;
   for (;;) {
      if (i <= sizeL(*p)) {
         (*p)->s = sizeL(*p) + 1;
         p = &(*p)->l;
         if (*p == NULL) { *p = tree_allocate((node_t){.x = x}); break; }
      } else {
         i = i - (sizeL(*p) + 1);
         p = &(*p)->r;
         if (*p == NULL) { *p = tree_allocate((node_t){.x = x}); break; }
      }
      tree_persist(p);
      if (!isOneOne(*p)) { a = p; j = i; }
   }
   for (node_t *q = *a; q != *p; ) {
      promote(q);
      if (j <= sizeL(q)) { q = q->l; }
      else { j = j - (sizeL(q) + 1); q = q->r; }
   }
   *a = avl_td_balance(*a);
}

TREE_VT_SIMPLE(avl_topdown_t, avl_td)

static void avl_td_vt_verify(list_t *self) {
   avl_topdown_t *t = (avl_topdown_t *)self->impl;
   tree_verify_size(t->tree.root, t->tree.size);
   verifyAVLRanks(t->tree.root, t->tree.size);
}
static void avl_td_vt_insert(list_t *self, u64 i, u64 x) {
   avl_topdown_t *t = (avl_topdown_t *)self->impl;
   avl_td_insert(&t->tree.root, i, x);
   t->tree.size++;
}
static u64 avl_td_vt_delete(list_t *self, u64 i) {
   avl_topdown_t *t = (avl_topdown_t *)self->impl;
   assert_bst(i < t->tree.size); u64 x;
   t->tree.root = avl_bu_delete_op(t->tree.root, t->tree.size, i, &x);
   t->tree.size--; return x;
}
static void avl_td_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   avl_topdown_t *t = (avl_topdown_t *)self->impl;
   tree_share(t->tree.root);
   node_t *l, *r; avl_split(t->tree.root, i, t->tree.size, &l, &r);
   avl_topdown_t *sl = (avl_topdown_t *)calloc(1, sizeof(avl_topdown_t)); sl->tree = (tree_t){l, i};
   avl_topdown_t *sr = (avl_topdown_t *)calloc(1, sizeof(avl_topdown_t)); sr->tree = (tree_t){r, t->tree.size - i};
   *out_l = (list_t){sl, self->vt}; *out_r = (list_t){sr, self->vt};
}
static list_t avl_td_vt_join(list_t *self, list_t *other) {
   avl_topdown_t *t = (avl_topdown_t *)self->impl, *that = (avl_topdown_t *)other->impl;
   tree_share(t->tree.root); tree_share(that->tree.root);
   avl_topdown_t *res = (avl_topdown_t *)calloc(1, sizeof(avl_topdown_t));
   res->tree.root = avl_join(t->tree.root, that->tree.root, t->tree.size, that->tree.size);
   res->tree.size = t->tree.size + that->tree.size;
   return (list_t){res, self->vt};
}
const list_vtable_t avl_topdown_vtable = {
   "AVLTopDown", avl_td_vt_new, avl_td_vt_clone, avl_td_vt_free,
   avl_td_vt_size, avl_td_vt_each, avl_td_vt_verify,
   avl_td_vt_select, avl_td_vt_update, avl_td_vt_insert, avl_td_vt_delete,
   avl_td_vt_split, avl_td_vt_join,
};
