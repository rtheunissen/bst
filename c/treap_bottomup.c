#include "treap_bottomup.h"
#include "rank.h"
#include "tree_list_adapter.h"

// ─────────────────────────────────────────────────────────────────────────────
// Internal operations
// ─────────────────────────────────────────────────────────────────────────────

static u64 treap_rank(node_t *p) {
   if (p == NULL) {
      return 0;
   }
   return p->y;
}

static node_t *treap_bu_join(node_t *l, node_t *r, u64 sl, u64 sr) {
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

      if (treap_rank(l) >= treap_rank(r)) {
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

static node_t *treap_bu_insert(treap_bottomup_t *tree, node_t *p, u64 size,
                               u64 i, u64 x) {
   invariant(tree != NULL);
   u64 inserted_size = tree_insert_inputs_require(p, i, size);
   if (p == NULL) {
      invariant(i == 0);
      return tree_allocate((node_t){
          .x = x,
          .y = xoshiro256_uint64(&tree->source),
      });
   }
   tree_persist_unshared(&p);
   if (i <= p->s) {
      u64 child_size = sizeL(p);
      node_t **l = insertL(p);
      *l = treap_bu_insert(tree, *l, child_size, i, x);
      if ((*l)->y > p->y) {
         treeRotateRSized(&p, inserted_size);
      }
   } else {
      u64 child_size = sizeR(p, size);
      node_t **r = insertR(p, &i);
      *r = treap_bu_insert(tree, *r, child_size, i, x);
      if ((*r)->y > p->y) {
         treeRotateLSized(&p, inserted_size);
      }
   }
   return p;
}

typedef struct {
   node_t *root;
   u64 value;
} treap_bu_delete_result_t;

static treap_bu_delete_result_t treap_bu_delete(node_t *p, u64 size, u64 i) {
   (void)tree_delete_inputs_require(p, i, size);
   if (i == p->s) {
      u64 value = p->x;
      tree_share(p->l);
      tree_share(p->r);
      node_t *root = treap_bu_join(p->l, p->r, p->s, sizeR(p, size));
      tree_release(p);
      return (treap_bu_delete_result_t){.root = root, .value = value};
   }
   tree_persist_unshared(&p);
   treap_bu_delete_result_t result;
   if (i < p->s) {
      invariant(p->l != NULL);
      u64 child_size = sizeL(p);
      node_t **l = deleteL(p);
      result = treap_bu_delete(*l, child_size, i);
      *l = result.root;
   } else {
      invariant(p->r != NULL);
      u64 child_size = sizeR(p, size);
      node_t **r = deleteR(p, &i);
      result = treap_bu_delete(*r, child_size, i);
      *r = result.root;
   }
   result.root = p;
   return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// List vtable
// ─────────────────────────────────────────────────────────────────────────────

TREE_LIST_COMMON_FUNCTIONS(treap_bottomup_t, treap_bu,
                           xoshiro256_seed(&t->source, random_uint64()))

static void treap_bu_vt_verify(list_t *self) {
   list_require(self);
   treap_bottomup_t *t = (treap_bottomup_t *)self->impl;
   verifyMaxRankHeapSized(t->tree.root, t->tree.size, treap_rank);
}

static void treap_bu_vt_insert(list_t *self, u64 i, u64 x) {
   list_require(self);
   treap_bottomup_t *t = (treap_bottomup_t *)self->impl;
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   t->tree.root = treap_bu_insert(t, t->tree.root, size, i, x);
   tree_list_sized_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
}

static u64 treap_bu_vt_delete(list_t *self, u64 i) {
   list_require(self);
   treap_bottomup_t *t = (treap_bottomup_t *)self->impl;
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   treap_bu_delete_result_t result =
       treap_bu_delete(t->tree.root, t->tree.size, i);
   t->tree.root = result.root;
   tree_list_sized_deleted(&t->tree);
   invariant(t->tree.size == deleted_size);
   return result.value;
}

static void treap_bu_vt_split(list_t *self, u64 i, list_t *out_l,
                              list_t *out_r) {
   tree_list_require_split(self, i, out_l, out_r);
   treap_bottomup_t *t = (treap_bottomup_t *)self->impl;
   u64 size = t->tree.size;
   u64 right_size = tree_split_inputs_require(t->tree.root, i, size);
   tree_share(t->tree.root);
   tree_split_result_t split = tree_split_roots_sized(t->tree.root, i, size);
   TREE_LIST_SPLIT_RESULT_FIELD_PREPARED(treap_bottomup_t, size, source,
                                         t->source, split.left, i, split.right,
                                         right_size);
}

static list_t treap_bu_vt_join(list_t *self, list_t *other) {
   tree_list_require_join(self, other);
   treap_bottomup_t *t = (treap_bottomup_t *)self->impl;
   treap_bottomup_t *that = (treap_bottomup_t *)other->impl;
   tree_join_roots_t roots =
       tree_list_prepare_join_roots(&t->tree, &that->tree);
   node_t *root =
       treap_bu_join(roots.left, roots.right, t->tree.size, that->tree.size);
   TREE_LIST_JOIN_RESULT_FIELD(
       treap_bottomup_t, source, t->source, root,
       tree_list_join_size(t->tree.size, that->tree.size));
}

const list_vtable_t treap_bottomup_vtable =
    TREE_LIST_VTABLE("TreapBottomUp", treap_bu);
