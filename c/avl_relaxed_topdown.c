#include "avl_relaxed_topdown.h"
#include "avl_relaxed_bottomup.h"
#include "avl_weak_topdown.h"
#include "avl_bottomup.h"
#include "rank.h"
#include "tree_list_adapter.h"

// AVLRelaxedTopDown (uses AVLWeakTopDown insert, base Tree delete)
// ═══════════════════════════════════════════════════════════════════════════════

TREE_VT_SIMPLE(avl_relaxed_topdown_t, avlr_td)

static void avlr_td_vt_verify(list_t *self) {
   list_require(self);
   avl_relaxed_topdown_t *t = (avl_relaxed_topdown_t *)self->impl;
   verifyAVLRelaxedRanksSized(t->tree.root, t->tree.size);
}
static void avlr_td_vt_insert(list_t *self, u64 i, u64 x) {
   list_require(self);
   avl_relaxed_topdown_t *t = (avl_relaxed_topdown_t *)self->impl;
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   avlw_td_insert(&t->tree.root, size, i, x);
   tree_list_sized_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
}
static u64 avlr_td_vt_delete(list_t *self, u64 i) {
   list_require(self);
   avl_relaxed_topdown_t *t = (avl_relaxed_topdown_t *)self->impl;
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   u64 x = tree_delete_base(&t->tree, i);
   invariant(t->tree.size == deleted_size);
   return x;
}
static void avlr_td_vt_split(list_t *self, u64 i, list_t *out_l,
                             list_t *out_r) {
   tree_list_require_split(self, i, out_l, out_r);
   avl_relaxed_topdown_t *t = (avl_relaxed_topdown_t *)self->impl;
   u64 size = t->tree.size;
   u64 right_size = tree_split_inputs_require(t->tree.root, i, size);
   tree_share(t->tree.root);
   tree_split_result_t split = avlr_split(t->tree.root, i, size);
   TREE_LIST_SPLIT_RESULT_PREPARED(avl_relaxed_topdown_t, size, split.left, i,
                                   split.right, right_size);
}
static list_t avlr_td_vt_join(list_t *self, list_t *other) {
   tree_list_require_join(self, other);
   avl_relaxed_topdown_t *t = (avl_relaxed_topdown_t *)self->impl;
   avl_relaxed_topdown_t *that = (avl_relaxed_topdown_t *)other->impl;
   tree_join_roots_t roots =
       tree_list_prepare_join_roots(&t->tree, &that->tree);
   node_t *root =
       avlr_join(roots.left, roots.right, t->tree.size, that->tree.size);
   u64 joined_size = tree_list_join_size(t->tree.size, that->tree.size);
   TREE_LIST_JOIN_RESULT(avl_relaxed_topdown_t, root, joined_size);
}
const list_vtable_t avl_relaxed_topdown_vtable =
    TREE_LIST_VTABLE("AVLRelaxedTopDown", avlr_td);
