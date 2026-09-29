#include "zip.h"
#include "tree_list_adapter.h"

// ─────────────────────────────────────────────────────────────────────────────
// Internal operations
// ─────────────────────────────────────────────────────────────────────────────

static u64 zip_randomRank(zip_t *tree) {
   invariant(tree != NULL);
   u64 word = xoshiro256_uint64(&tree->source);
   return word == 0 ? 64 : (u64)__builtin_clzll(word);
}

static u64 zip_rank(node_t *p) {
   if (p == NULL) {
      return 0;
   }
   return p->y;
}

static node_t *zip_zip(node_t *l, node_t *r, u64 sl, u64 sr) {
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

      if (zip_rank(l) >= zip_rank(r)) {
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

static void zip_insert(zip_t *tree, u64 i, u64 x) {
   invariant(tree != NULL);
   u64 size = tree->tree.size;
   u64 inserted_size = tree_insert_inputs_require(tree->tree.root, i, size);

   node_t **p = &tree->tree.root;
   node_t *n = tree_allocate((node_t){.x = x, .y = zip_randomRank(tree)});
   u64 subtree_size = size;

   while (*p != NULL) {
      node_t *node = *p;
      invariant(node->s < subtree_size);
      if (n->y > node->y) {
         break;
      }
      if (i <= node->s) {
         if (n->y == node->y) {
            break;
         }
         subtree_size = sizeL(node);
         tree_persist_unshared(p);
         p = insertL(*p);
      } else {
         subtree_size = sizeR(node, subtree_size);
         tree_persist_unshared(p);
         p = insertR(*p, &i);
      }
   }
   invariant(zip_rank(n) >= zip_rank(*p));
   invariant(i <= subtree_size);
   invariant((*p != NULL) == (subtree_size > 0));
   tree_split_result_t split = tree_split_roots_sized(*p, i, subtree_size);
   n->l = split.left;
   n->r = split.right;
   n->s = i;
   *p = n;
   tree_list_sized_inserted(&tree->tree);
   invariant(tree->tree.size == inserted_size);
}

static u64 zip_delete_op(zip_t *tree, u64 i) {
   invariant(tree != NULL);
   u64 original_size = tree->tree.size;
   u64 deleted_size =
       tree_delete_inputs_require(tree->tree.root, i, original_size);
   u64 subtree_size = tree->tree.size;
   node_t **p = &tree->tree.root;
   u64 x;
   for (;;) {
      invariant(*p != NULL);
      if (i == (*p)->s) {
         node_t *old = *p;
         tree_share(old->l);
         tree_share(old->r);
         x = old->x;
         *p = zip_zip(old->l, old->r, old->s, sizeR(old, subtree_size));
         tree_release(old);
         tree_list_sized_deleted(&tree->tree);
         invariant(tree->tree.size == deleted_size);
         return x;
      }
      tree_persist_unshared(p);
      node_t *node = *p;
      invariant(node != NULL);
      if (i < node->s) {
         invariant(node->l != NULL);
         subtree_size = sizeL(node);
         p = deleteL(node);
      } else {
         invariant(node->r != NULL);
         subtree_size = sizeR(node, subtree_size);
         p = deleteR(node, &i);
      }
   }
}

static void zip_verifyRankNode(node_t *p, void *ctx) {
   (void)ctx;
   invariant(p->l == NULL || p->y > p->l->y);
   invariant(p->r == NULL || p->y >= p->r->y);
}

static void zip_verifyRanks(node_t *p, u64 size) {
   invariant(tree_verify_size_each_node(p, size, zip_verifyRankNode, NULL) ==
             size);
}

// ─────────────────────────────────────────────────────────────────────────────
// List vtable
// ─────────────────────────────────────────────────────────────────────────────

TREE_LIST_COMMON_FUNCTIONS(zip_t, zip,
                           xoshiro256_seed(&t->source, random_uint64()))

static void zip_vt_verify(list_t *self) {
   list_require(self);
   zip_t *t = (zip_t *)self->impl;
   zip_verifyRanks(t->tree.root, t->tree.size);
}

static void zip_vt_insert(list_t *self, u64 i, u64 x) {
   list_require(self);
   zip_t *t = (zip_t *)self->impl;
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   zip_insert(t, i, x);
   invariant(t->tree.size == inserted_size);
}

static u64 zip_vt_delete(list_t *self, u64 i) {
   list_require(self);
   zip_t *t = (zip_t *)self->impl;
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   u64 x = zip_delete_op(t, i);
   invariant(t->tree.size == deleted_size);
   return x;
}

static void zip_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   tree_list_require_split(self, i, out_l, out_r);
   zip_t *t = (zip_t *)self->impl;
   u64 size = t->tree.size;
   u64 right_size = tree_split_inputs_require(t->tree.root, i, size);
   tree_share(t->tree.root);
   tree_split_result_t split = tree_split_roots_sized(t->tree.root, i, size);
   TREE_LIST_SPLIT_RESULT_FIELD_PREPARED(
       zip_t, size, source, t->source, split.left, i, split.right, right_size);
}

static list_t zip_vt_join(list_t *self, list_t *other) {
   tree_list_require_join(self, other);
   zip_t *t = (zip_t *)self->impl;
   zip_t *that = (zip_t *)other->impl;
   tree_join_roots_t roots =
       tree_list_prepare_join_roots(&t->tree, &that->tree);
   node_t *root =
       zip_zip(roots.left, roots.right, t->tree.size, that->tree.size);
   TREE_LIST_JOIN_RESULT_FIELD(
       zip_t, source, t->source, root,
       tree_list_join_size(t->tree.size, that->tree.size));
}

const list_vtable_t zip_vtable = TREE_LIST_VTABLE("Zip", zip);
