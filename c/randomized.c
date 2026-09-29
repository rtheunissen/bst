#include "randomized.h"
#include "tree_list_adapter.h"

// ─────────────────────────────────────────────────────────────────────────────
// Internal operations
// ─────────────────────────────────────────────────────────────────────────────

static void randomized_insert_internal(randomized_t *tree, node_t **p, u64 s,
                                       u64 i, u64 x) {
   invariant(tree != NULL);
   invariant(p != NULL);
   (void)tree_insert_inputs_require(*p, i, s);

   for (;;) {
      invariant(p != NULL);
      invariant(i <= s);
      invariant(s < UINT64_MAX);
      invariant((*p != NULL) == (s > 0));
      invariant(*p == NULL || (*p)->s < s);
      u64 insertion_positions = sequence_insert_inputs_require(i, s);
      if (random_less_than_src(insertion_positions, &tree->source) == s) {
         tree_split_result_t split = tree_split_roots_sized(*p, i, s);
         *p = tree_allocate((node_t){
             .x = x,
             .s = i,
             .l = split.left,
             .r = split.right,
         });
         return;
      }
      tree_persist_unshared(p);
      node_t *node = *p;
      invariant(node != NULL);
      if (i > node->s) {
         u64 sr = sizeR(node, s);
         p = insertR(node, &i);
         s = sr;
      } else {
         u64 sl = sizeL(node);
         p = insertL(node);
         s = sl;
      }
   }
}

static node_t *randomized_join(randomized_t *tree, node_t *l, node_t *r, u64 sl,
                               u64 sr) {
   invariant(tree != NULL);
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
      invariant(sl > 0);
      invariant(sr > 0);
      u64 total = size_sum(sl, sr);

      if (random_less_than_src(total, &tree->source) < sl) {
         tree_persist_unshared(&l);
         invariant(l->s < sl);
         sl = sizeR(l, sl);
         *p = l;
         p = &l->r;
         l = *p;
      } else {
         tree_persist_unshared(&r);
         invariant(r->s < sr);
         sr = r->s;
         r->s = rank_after_prefix(sl, r);
         *p = r;
         p = &r->l;
         r = *p;
      }
   }
}

static u64 randomized_delete_internal(randomized_t *tree, node_t **p, u64 s,
                                      u64 i) {
   invariant(tree != NULL);
   invariant(p != NULL);
   (void)tree_delete_inputs_require(*p, i, s);

   for (;;) {
      invariant(p != NULL);
      invariant(*p != NULL);
      invariant(i < s);
      invariant((*p)->s < s);
      if (i == (*p)->s) {
         node_t *old = *p;
         invariant(old->s < s);
         tree_share(old->l);
         tree_share(old->r);
         u64 x = old->x;
         *p = randomized_join(tree, old->l, old->r, old->s, sizeR(old, s));
         tree_release(old);
         return x;
      }
      tree_persist_unshared(p);
      node_t *node = *p;
      invariant(node != NULL);
      if (i < node->s) {
         u64 sl = sizeL(node);
         invariant(node->l != NULL);
         p = deleteL(node);
         s = sl;
      } else {
         u64 sr = sizeR(node, s);
         invariant(node->r != NULL);
         p = deleteR(node, &i);
         s = sr;
      }
   }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public operations
// ─────────────────────────────────────────────────────────────────────────────

static void randomized_insert(randomized_t *t, u64 i, u64 x) {
   invariant(t != NULL);
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   randomized_insert_internal(t, &t->tree.root, t->tree.size, i, x);
   tree_list_sized_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
}

static u64 randomized_delete(randomized_t *t, u64 i) {
   invariant(t != NULL);
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   u64 x = randomized_delete_internal(t, &t->tree.root, t->tree.size, i);
   tree_list_sized_deleted(&t->tree);
   invariant(t->tree.size == deleted_size);
   return x;
}

// ─────────────────────────────────────────────────────────────────────────────
// List vtable
// ─────────────────────────────────────────────────────────────────────────────

TREE_LIST_COMMON_FUNCTIONS(randomized_t, randomized,
                           xoshiro256_seed(&t->source, random_uint64()))

static void randomized_vt_verify(list_t *self) {
   list_require(self);
   randomized_t *t = (randomized_t *)self->impl;
   tree_list_verify_tree(t->tree);
}

static void randomized_vt_insert(list_t *self, u64 i, u64 x) {
   list_require(self);
   randomized_insert((randomized_t *)self->impl, i, x);
}

static u64 randomized_vt_delete(list_t *self, u64 i) {
   list_require(self);
   return randomized_delete((randomized_t *)self->impl, i);
}

static void randomized_vt_split(list_t *self, u64 i, list_t *out_l,
                                list_t *out_r) {
   tree_list_require_split(self, i, out_l, out_r);
   randomized_t *t = (randomized_t *)self->impl;
   u64 size = t->tree.size;
   u64 right_size = tree_split_inputs_require(t->tree.root, i, size);
   tree_share(t->tree.root);
   tree_split_result_t split = tree_split_roots_sized(t->tree.root, i, size);
   TREE_LIST_SPLIT_RESULT_FIELD_PREPARED(randomized_t, size, source, t->source,
                                         split.left, i, split.right,
                                         right_size);
}

static list_t randomized_vt_join(list_t *self, list_t *other) {
   tree_list_require_join(self, other);
   randomized_t *t = (randomized_t *)self->impl;
   randomized_t *that = (randomized_t *)other->impl;
   randomized_t joined_state = {.source = t->source};

   tree_join_roots_t roots =
       tree_list_prepare_join_roots(&t->tree, &that->tree);

   node_t *root = randomized_join(&joined_state, roots.left, roots.right,
                                  t->tree.size, that->tree.size);
   TREE_LIST_JOIN_RESULT_FIELD(
       randomized_t, source, joined_state.source, root,
       tree_list_join_size(t->tree.size, that->tree.size));
}

const list_vtable_t randomized_vtable =
    TREE_LIST_VTABLE("Randomized", randomized);
