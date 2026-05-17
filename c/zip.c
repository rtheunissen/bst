#include "zip.h"

// ─────────────────────────────────────────────────────────────────────────────
// Internal operations
// ─────────────────────────────────────────────────────────────────────────────

static u64 zip_randomRank(zip_t *tree) {
   return (u64)__builtin_clzll(xoshiro256_uint64(&tree->source));
}

static u64 zip_rank(node_t *p) {
   if (p == NULL) return 0;
   return p->y;
}

static void zip_unzip(node_t *p, u64 i, node_t **l, node_t **r) {
   while (p != NULL) {
      tree_persist(&p);
      if (i <= p->s) {
         *r = p;
         p->s = p->s - i;
         r = &p->l;
         p = *r;
      } else {
         *l = p;
         i = i - p->s - 1;
         l = &p->r;
         p = *l;
      }
   }
   *l = NULL;
   *r = NULL;
}

static node_t *zip_zip(node_t *l, node_t *r, u64 sl) {
   assert_bst(sl == node_size(l));
   node_t *root = NULL;
   node_t **p = &root;
   for (;;) {
      if (l == NULL) { *p = r; return root; }
      if (r == NULL) { *p = l; return root; }

      if (zip_rank(l) >= zip_rank(r)) {
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

static void zip_insert(zip_t *tree, u64 i, u64 x) {
   tree->tree.size++;

   node_t **p = &tree->tree.root;
   node_t *n = tree_allocate((node_t){.x = x, .y = zip_randomRank(tree)});

   while (*p != NULL) {
      if (n->y > (*p)->y) {
         break;
      }
      if (i <= (*p)->s) {
         if (n->y == (*p)->y) {
            break;
         }
         tree_persist(p);
         (*p)->s = (*p)->s + 1;
         p = &(*p)->l;
      } else {
         tree_persist(p);
         i = i - ((*p)->s + 1);
         p = &(*p)->r;
      }
   }
   assert_bst(zip_rank(n) >= zip_rank(*p));
   zip_unzip(*p, i, &n->l, &n->r);
   n->s = i;
   *p = n;
}

static u64 zip_delete_op(zip_t *tree, u64 i) {
   assert_bst(i < tree->tree.size);
   node_t **p = &tree->tree.root;
   u64 x;
   for (;;) {
      if (i == (*p)->s) {
         node_t *old = *p;
         tree_share(old->l);
         tree_share(old->r);
         x = old->x;
         *p = zip_zip(old->l, old->r, old->s);
         tree_release(old);
         tree->tree.size--;
         return x;
      }
      tree_persist(p);
      if (i < (*p)->s) {
         (*p)->s = (*p)->s - 1;
         p = &(*p)->l;
      } else {
         i = i - ((*p)->s + 1);
         p = &(*p)->r;
      }
   }
}

static void zip_verifyRanks(node_t *p) {
   if (p == NULL) return;
   invariant(p->l == NULL || p->y > p->l->y);
   invariant(p->r == NULL || p->y > p->r->y || p->y == p->r->y);
   zip_verifyRanks(p->l);
   zip_verifyRanks(p->r);
}


// ─────────────────────────────────────────────────────────────────────────────
// List vtable
// ─────────────────────────────────────────────────────────────────────────────

static list_t zip_vt_new(const list_vtable_t *vt) {
   zip_t *t = (zip_t *)calloc(1, sizeof(zip_t));
   xoshiro256_seed(&t->source, random_uint64());
   return (list_t){.impl = t, .vt = vt};
}

static list_t zip_vt_clone(list_t *self) {
   zip_t *orig = (zip_t *)self->impl;
   zip_t *copy = (zip_t *)malloc(sizeof(zip_t));
   tree_share(orig->tree.root);
   *copy = *orig;
   return (list_t){.impl = copy, .vt = self->vt};
}

static void zip_vt_free(list_t *self) {
   zip_t *t = (zip_t *)self->impl;
   free_nodes(t->tree.root);
   free(t);
   self->impl = NULL;
}

static u64 zip_vt_size(list_t *self) { return ((zip_t *)self->impl)->tree.size; }

static void zip_vt_each(list_t *self, void (*visit)(u64, void *), void *ctx) {
   inorder(((zip_t *)self->impl)->tree.root, visit, ctx);
}

static void zip_vt_verify(list_t *self) {
   zip_t *t = (zip_t *)self->impl;
   tree_verify_size(t->tree.root, t->tree.size);
   zip_verifyRanks(t->tree.root);
}

static u64  zip_vt_select(list_t *self, u64 i) { return tree_lookup(&((zip_t *)self->impl)->tree, i); }
static void zip_vt_update(list_t *self, u64 i, u64 x) { tree_update_base(&((zip_t *)self->impl)->tree, i, x); }
static void zip_vt_insert(list_t *self, u64 i, u64 x) { zip_insert((zip_t *)self->impl, i, x); }
static u64  zip_vt_delete(list_t *self, u64 i) { return zip_delete_op((zip_t *)self->impl, i); }

static void zip_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   zip_t *t = (zip_t *)self->impl;
   assert_bst(i <= t->tree.size);
   tree_share(t->tree.root);
   node_t *l, *r;
   tree_split_internal(t->tree.root, i, &l, &r);
   zip_t *sl = (zip_t *)calloc(1, sizeof(zip_t)); sl->tree = (tree_t){l, i}; sl->source = t->source;
   zip_t *sr = (zip_t *)calloc(1, sizeof(zip_t)); sr->tree = (tree_t){r, t->tree.size - i}; sr->source = t->source;
   *out_l = (list_t){sl, self->vt};
   *out_r = (list_t){sr, self->vt};
}

static list_t zip_vt_join(list_t *self, list_t *other) {
   zip_t *t = (zip_t *)self->impl;
   zip_t *that = (zip_t *)other->impl;
   tree_share(t->tree.root);
   tree_share(that->tree.root);
   zip_t *result = (zip_t *)calloc(1, sizeof(zip_t));
   result->tree.root = zip_zip(t->tree.root, that->tree.root, t->tree.size);
   result->tree.size = t->tree.size + that->tree.size;
   result->source = t->source;
   return (list_t){result, self->vt};
}

const list_vtable_t zip_vtable = {
   .name      = "Zip",
   .new_fn    = zip_vt_new,
   .clone_fn  = zip_vt_clone,
   .free_fn   = zip_vt_free,
   .size_fn   = zip_vt_size,
   .each_fn   = zip_vt_each,
   .verify_fn = zip_vt_verify,
   .select_fn = zip_vt_select,
   .update_fn = zip_vt_update,
   .insert_fn = zip_vt_insert,
   .delete_fn = zip_vt_delete,
   .split_fn  = zip_vt_split,
   .join_fn   = zip_vt_join,
};
