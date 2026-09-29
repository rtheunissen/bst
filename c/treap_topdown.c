#include "treap_topdown.h"
#include "rank.h"
#include "tree_list_adapter.h"

// ─────────────────────────────────────────────────────────────────────────────
// Internal operations
// ─────────────────────────────────────────────────────────────────────────────

static u64 ttd_rank(node_t *p) {
   if (p == NULL) {
      return 0;
   }
   return p->y;
}

static node_t *ttd_join(node_t *l, node_t *r, u64 sl, u64 sr) {
   tree_join_inputs_require(l, r, sl, sr);
#ifdef DEBUG
   tree_debug_require_join_inputs(l, r, sl, sr);
#endif
   node_t *root = NULL;
   node_t **p = &root;
   for (;;) {
      tree_sized_root_shape_require(l, sl);
      tree_sized_root_shape_require(r, sr);
      if (l == NULL) {
         invariant(sl == 0);
         *p = r;
         return root;
      }
      if (r == NULL) {
         invariant(sr == 0);
         *p = l;
         return root;
      }

      if (ttd_rank(l) >= ttd_rank(r)) {
         tree_persist_unshared(&l);
         invariant(l->s < sl);
         sl = sizeR(l, sl);
         *p = l;
         p = &l->r;
         l = *p;
      } else {
         tree_persist_unshared(&r);
         invariant(r->s < sr);
         sr = sizeL(r);
         r->s = rank_after_prefix(sl, r);
         *p = r;
         p = &r->l;
         r = *p;
      }
   }
}

static u64 ttd_delete(node_t **p, u64 size, u64 i) {
   invariant(p != NULL);
   (void)tree_delete_inputs_require(*p, i, size);

   for (;;) {
      tree_rank_inputs_require(*p, i, size);
      node_t *old = *p;
      if (i == old->s) {
         tree_share(old->l);
         tree_share(old->r);
         u64 x = old->x;
         *p = ttd_join(old->l, old->r, old->s, sizeR(old, size));
         tree_release(old);
         return x;
      }
      tree_persist_unshared(p);
      node_t *node = *p;
      invariant(node != NULL);
      if (i < node->s) {
         invariant(node->l != NULL);
         size = sizeL(node);
         p = deleteL(node);
      } else {
         invariant(node->r != NULL);
         size = sizeR(node, size);
         p = deleteR(node, &i);
      }
   }
}

static void ttd_insert(treap_topdown_t *tree, node_t **p, u64 size, u64 i,
                       u64 x) {
   invariant(tree != NULL);
   invariant(p != NULL);
   (void)tree_insert_inputs_require(*p, i, size);
   u64 y = xoshiro256_uint64(&tree->source);
   for (;;) {
      invariant(p != NULL);
      if (ttd_rank(*p) <= y) {
         invariant(*p != NULL || i == 0);
         tree_split_result_t split = tree_split_roots_sized(*p, i, size);
         *p = tree_allocate((node_t){
             .x = x,
             .y = y,
             .s = i,
             .l = split.left,
             .r = split.right,
         });
         return;
      }
      invariant(*p != NULL);
      tree_persist_unshared(p);
      node_t *node = *p;
      invariant(node != NULL);
      if (i <= node->s) {
         size = sizeL(node);
         p = insertL(node);
      } else {
         size = sizeR(node, size);
         p = insertR(node, &i);
      }
   }
}

// ─────────────────────────────────────────────────────────────────────────────
// List vtable
// ─────────────────────────────────────────────────────────────────────────────

TREE_LIST_COMMON_FUNCTIONS(treap_topdown_t, ttd,
                           xoshiro256_seed(&t->source, random_uint64()))

static void ttd_vt_verify(list_t *self) {
   list_require(self);
   treap_topdown_t *t = (treap_topdown_t *)self->impl;
   verifyMaxRankHeapSized(t->tree.root, t->tree.size, ttd_rank);
}

static void ttd_vt_insert(list_t *self, u64 i, u64 x) {
   list_require(self);
   treap_topdown_t *t = (treap_topdown_t *)self->impl;
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   ttd_insert(t, &t->tree.root, size, i, x);
   tree_list_sized_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
}

static u64 ttd_vt_delete(list_t *self, u64 i) {
   list_require(self);
   treap_topdown_t *t = (treap_topdown_t *)self->impl;
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   u64 x = ttd_delete(&t->tree.root, t->tree.size, i);
   tree_list_sized_deleted(&t->tree);
   invariant(t->tree.size == deleted_size);
   return x;
}

static void ttd_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   tree_list_require_split(self, i, out_l, out_r);
   treap_topdown_t *t = (treap_topdown_t *)self->impl;
   u64 size = t->tree.size;
   u64 right_size = tree_split_inputs_require(t->tree.root, i, size);
   tree_share(t->tree.root);
   tree_split_result_t split = tree_split_roots_sized(t->tree.root, i, size);
   TREE_LIST_SPLIT_RESULT_FIELD_PREPARED(treap_topdown_t, size, source,
                                         t->source, split.left, i, split.right,
                                         right_size);
}

static list_t ttd_vt_join(list_t *self, list_t *other) {
   tree_list_require_join(self, other);
   treap_topdown_t *t = (treap_topdown_t *)self->impl;
   treap_topdown_t *that = (treap_topdown_t *)other->impl;
   tree_join_roots_t roots =
       tree_list_prepare_join_roots(&t->tree, &that->tree);
   node_t *root =
       ttd_join(roots.left, roots.right, t->tree.size, that->tree.size);
   TREE_LIST_JOIN_RESULT_FIELD(
       treap_topdown_t, source, t->source, root,
       tree_list_join_size(t->tree.size, that->tree.size));
}

const list_vtable_t treap_topdown_vtable =
    TREE_LIST_VTABLE("TreapTopDown", ttd);
