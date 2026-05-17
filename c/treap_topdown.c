#include "treap_topdown.h"

// ─────────────────────────────────────────────────────────────────────────────
// Internal operations
// ─────────────────────────────────────────────────────────────────────────────

static u64 ttd_rank(node_t *p) {
   if (p == NULL) return 0;
   return p->y;
}

static node_t *ttd_join(node_t *l, node_t *r, u64 sl) {
   node_t *root = NULL;
   node_t **p = &root;
   for (;;) {
      if (l == NULL) { *p = r; return root; }
      if (r == NULL) { *p = l; return root; }

      if (ttd_rank(l) >= ttd_rank(r)) {
         tree_persist(&l);
        sl = sl - l->s - 1;
        *p = l;
         p = &l->r;
         l = *p;
      } else {
         tree_persist(&r);
       r->s = r->s + sl;
        *p = r;
         p = &r->l;
         r = *p;
      }
   }
}

static void ttd_delete(node_t **p, u64 i, u64 *x) {
   for (;;) {
      if (i == (*p)->s) {
         node_t *old = *p;
         tree_share(old->l);
         tree_share(old->r);
         *x = old->x;
         *p = ttd_join(old->l, old->r, old->s);
         tree_release(old);
         return;
      }
      tree_persist(p);
      if (i < (*p)->s) {
    (*p)->s = (*p)->s - 1;
         p = &(*p)->l;
      } else {
         i = i - (*p)->s - 1;
         p = &(*p)->r;
      }
   }
}

static void ttd_insert(treap_topdown_t *tree, node_t **p, u64 i, u64 x) {
   u64 y = xoshiro256_uint64(&tree->source);
   for (;;) {
      if (ttd_rank(*p) <= y) {
         node_t *l, *r;
         tree_split_internal(*p, i, &l, &r);
         *p = tree_allocate((node_t){.x = x, .y = y, .s = i, .l = l, .r = r});
         return;
      }
      tree_persist(p);
      if (i <= (*p)->s) {
         p = insertL(*p);
      } else {
         p = insertR(*p, &i);
      }
   }
}

static void ttd_verifyMaxRankHeap(node_t *p) {
   if (p == NULL) return;
   invariant(ttd_rank(p) >= ttd_rank(p->l));
   invariant(ttd_rank(p) >= ttd_rank(p->r));
   ttd_verifyMaxRankHeap(p->l);
   ttd_verifyMaxRankHeap(p->r);
}


// ─────────────────────────────────────────────────────────────────────────────
// List vtable
// ─────────────────────────────────────────────────────────────────────────────

static list_t ttd_vt_new(const list_vtable_t *vt) {
   treap_topdown_t *t = (treap_topdown_t *)calloc(1, sizeof(treap_topdown_t));
   xoshiro256_seed(&t->source, random_uint64());
   return (list_t){.impl = t, .vt = vt};
}

static list_t ttd_vt_clone(list_t *self) {
   treap_topdown_t *orig = (treap_topdown_t *)self->impl;
   treap_topdown_t *copy = (treap_topdown_t *)malloc(sizeof(treap_topdown_t));
   tree_share(orig->tree.root);
   *copy = *orig;
   return (list_t){.impl = copy, .vt = self->vt};
}

static void ttd_vt_free(list_t *self) {
   treap_topdown_t *t = (treap_topdown_t *)self->impl;
   free_nodes(t->tree.root);
   free(t);
   self->impl = NULL;
}

static u64  ttd_vt_size(list_t *self) { return ((treap_topdown_t *)self->impl)->tree.size; }
static void ttd_vt_each(list_t *self, void (*visit)(u64, void *), void *ctx) { inorder(((treap_topdown_t *)self->impl)->tree.root, visit, ctx); }

static void ttd_vt_verify(list_t *self) {
   treap_topdown_t *t = (treap_topdown_t *)self->impl;
   tree_verify_size(t->tree.root, t->tree.size);
   ttd_verifyMaxRankHeap(t->tree.root);
}

static u64  ttd_vt_select(list_t *self, u64 i) { return tree_lookup(&((treap_topdown_t *)self->impl)->tree, i); }
static void ttd_vt_update(list_t *self, u64 i, u64 x) { tree_update_base(&((treap_topdown_t *)self->impl)->tree, i, x); }

static void ttd_vt_insert(list_t *self, u64 i, u64 x) {
   treap_topdown_t *t = (treap_topdown_t *)self->impl;
   assert_bst(i <= t->tree.size);
   ttd_insert(t, &t->tree.root, i, x);
   t->tree.size++;
}

static u64 ttd_vt_delete(list_t *self, u64 i) {
   treap_topdown_t *t = (treap_topdown_t *)self->impl;
   assert_bst(i < t->tree.size);
   u64 x;
   ttd_delete(&t->tree.root, i, &x);
   t->tree.size--;
   return x;
}

static void ttd_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   treap_topdown_t *t = (treap_topdown_t *)self->impl;
   assert_bst(i <= t->tree.size);
   tree_share(t->tree.root);
   node_t *l, *r;
   tree_split_internal(t->tree.root, i, &l, &r);
   treap_topdown_t *sl = (treap_topdown_t *)calloc(1, sizeof(treap_topdown_t));
   sl->tree = (tree_t){l, i}; sl->source = t->source;
   treap_topdown_t *sr = (treap_topdown_t *)calloc(1, sizeof(treap_topdown_t));
   sr->tree = (tree_t){r, t->tree.size - i}; sr->source = t->source;
   *out_l = (list_t){sl, self->vt};
   *out_r = (list_t){sr, self->vt};
}

static list_t ttd_vt_join(list_t *self, list_t *other) {
   treap_topdown_t *t = (treap_topdown_t *)self->impl;
   treap_topdown_t *that = (treap_topdown_t *)other->impl;
   tree_share(t->tree.root);
   tree_share(that->tree.root);
   treap_topdown_t *result = (treap_topdown_t *)calloc(1, sizeof(treap_topdown_t));
   result->tree.root = ttd_join(t->tree.root, that->tree.root, t->tree.size);
   result->tree.size = t->tree.size + that->tree.size;
   result->source = t->source;
   return (list_t){result, self->vt};
}

const list_vtable_t treap_topdown_vtable = {
   .name      = "TreapTopDown",
   .new_fn    = ttd_vt_new,
   .clone_fn  = ttd_vt_clone,
   .free_fn   = ttd_vt_free,
   .size_fn   = ttd_vt_size,
   .each_fn   = ttd_vt_each,
   .verify_fn = ttd_vt_verify,
   .select_fn = ttd_vt_select,
   .update_fn = ttd_vt_update,
   .insert_fn = ttd_vt_insert,
   .delete_fn = ttd_vt_delete,
   .split_fn  = ttd_vt_split,
   .join_fn   = ttd_vt_join,
};
