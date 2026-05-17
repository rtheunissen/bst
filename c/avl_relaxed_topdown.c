#include "avl_relaxed_topdown.h"
#include "avl_relaxed_bottomup.h"
#include "avl_weak_topdown.h"
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


// AVLRelaxedTopDown (uses AVLWeakTopDown insert, base Tree delete)
// ═══════════════════════════════════════════════════════════════════════════════

TREE_VT_SIMPLE(avl_relaxed_topdown_t, avlr_td)

static void avlr_td_vt_verify(list_t *self) {
   avl_relaxed_topdown_t *t = (avl_relaxed_topdown_t *)self->impl;
   tree_verify_size(t->tree.root, t->tree.size);
   verifyAVLRelaxedRanks(t->tree.root);
}
static void avlr_td_vt_insert(list_t *self, u64 i, u64 x) {
   avl_relaxed_topdown_t *t = (avl_relaxed_topdown_t *)self->impl;
   avlw_td_insert(&t->tree.root, i, x); t->tree.size++;
}
static u64 avlr_td_vt_delete(list_t *self, u64 i) {
   return tree_delete_base(&((avl_relaxed_topdown_t *)self->impl)->tree, i);
}
static void avlr_td_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   avl_relaxed_topdown_t *t = (avl_relaxed_topdown_t *)self->impl;
   tree_share(t->tree.root); node_t *l, *r;
   avlr_split(t->tree.root, i, t->tree.size, &l, &r);
   avl_relaxed_topdown_t *sl = (avl_relaxed_topdown_t *)calloc(1, sizeof(avl_relaxed_topdown_t)); sl->tree = (tree_t){l, i};
   avl_relaxed_topdown_t *sr = (avl_relaxed_topdown_t *)calloc(1, sizeof(avl_relaxed_topdown_t)); sr->tree = (tree_t){r, t->tree.size - i};
   *out_l = (list_t){sl, self->vt}; *out_r = (list_t){sr, self->vt};
}
static list_t avlr_td_vt_join(list_t *self, list_t *other) {
   avl_relaxed_topdown_t *t = (avl_relaxed_topdown_t *)self->impl, *that = (avl_relaxed_topdown_t *)other->impl;
   tree_share(t->tree.root); tree_share(that->tree.root);
   avl_relaxed_topdown_t *res = (avl_relaxed_topdown_t *)calloc(1, sizeof(avl_relaxed_topdown_t));
   res->tree.root = avlr_join(t->tree.root, that->tree.root, t->tree.size, that->tree.size);
   res->tree.size = t->tree.size + that->tree.size;
   return (list_t){res, self->vt};
}
const list_vtable_t avl_relaxed_topdown_vtable = {
   "AVLRelaxedTopDown", avlr_td_vt_new, avlr_td_vt_clone, avlr_td_vt_free,
   avlr_td_vt_size, avlr_td_vt_each, avlr_td_vt_verify,
   avlr_td_vt_select, avlr_td_vt_update, avlr_td_vt_insert, avlr_td_vt_delete,
   avlr_td_vt_split, avlr_td_vt_join,
};
