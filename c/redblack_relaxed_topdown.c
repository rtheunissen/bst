#include "redblack_relaxed_topdown.h"
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

// rb_td_insert defined in redblack_topdown.c
node_t *rb_relaxed_join(node_t *l, node_t *r, u64 sl, u64 sr);
void rb_td_insert(node_t **p, u64 i, u64 x);

// RedBlackRelaxedTopDown (td insert, base delete, own join with base deleteMin/Max)
// ═══════════════════════════════════════════════════════════════════════════════

TREE_VT_SIMPLE(redblack_relaxed_topdown_t, rbr_td)
static void rbr_td_vt_verify(list_t *self) { redblack_relaxed_topdown_t *t = (redblack_relaxed_topdown_t *)self->impl; tree_verify_size(t->tree.root, t->tree.size); verifyRedBlackRelaxedRanks(t->tree.root); verifyRedBlackRelaxedHeight(t->tree.root); }
static void rbr_td_vt_insert(list_t *self, u64 i, u64 x) { redblack_relaxed_topdown_t *t = (redblack_relaxed_topdown_t *)self->impl; t->tree.size++; rb_td_insert(&t->tree.root, i, x); }
static u64 rbr_td_vt_delete(list_t *self, u64 i) { return tree_delete_base(&((redblack_relaxed_topdown_t *)self->impl)->tree, i); }
static void rbr_td_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   redblack_relaxed_topdown_t *t = (redblack_relaxed_topdown_t *)self->impl; tree_share(t->tree.root); node_t *l, *r;
   rb_split(t->tree.root, t->tree.size, i, &l, &r);
   redblack_relaxed_topdown_t *sl = (redblack_relaxed_topdown_t *)calloc(1, sizeof(redblack_relaxed_topdown_t)); sl->tree = (tree_t){l, i};
   redblack_relaxed_topdown_t *sr = (redblack_relaxed_topdown_t *)calloc(1, sizeof(redblack_relaxed_topdown_t)); sr->tree = (tree_t){r, t->tree.size - i};
   *out_l = (list_t){sl, self->vt}; *out_r = (list_t){sr, self->vt};
}
static list_t rbr_td_vt_join(list_t *self, list_t *other) {
   redblack_relaxed_topdown_t *t = (redblack_relaxed_topdown_t *)self->impl, *that = (redblack_relaxed_topdown_t *)other->impl;
   tree_share(t->tree.root); tree_share(that->tree.root);
   redblack_relaxed_topdown_t *res = (redblack_relaxed_topdown_t *)calloc(1, sizeof(redblack_relaxed_topdown_t));
   res->tree.root = rb_relaxed_join(t->tree.root, that->tree.root, t->tree.size, that->tree.size);
   res->tree.size = t->tree.size + that->tree.size; return (list_t){res, self->vt};
}
const list_vtable_t redblack_relaxed_topdown_vtable = {
   "RedBlackRelaxedTopDown", rbr_td_vt_new, rbr_td_vt_clone, rbr_td_vt_free,
   rbr_td_vt_size, rbr_td_vt_each, rbr_td_vt_verify,
   rbr_td_vt_select, rbr_td_vt_update, rbr_td_vt_insert, rbr_td_vt_delete,
   rbr_td_vt_split, rbr_td_vt_join,
};
