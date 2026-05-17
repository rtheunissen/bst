#include "randomized.h"

// ─────────────────────────────────────────────────────────────────────────────
// Internal operations
// ─────────────────────────────────────────────────────────────────────────────

static void randomized_insert_internal(randomized_t *tree, node_t **p, u64 s, u64 i, u64 x) {
   for (;;) {
      if (random_less_than_src(s + 1, &tree->source) == s) {
         node_t *l, *r;
         tree_split_internal(*p, i, &l, &r);
         *p = tree_allocate((node_t){.x = x, .s = i, .l = l, .r = r});
         return;
      }
      tree_persist(p);
      if (i > (*p)->s) {
         s = s - (*p)->s - 1;
         i = i - (*p)->s - 1;
         p = &(*p)->r;
      } else {
         s = (*p)->s;
    (*p)->s = (*p)->s + 1;
         p = &(*p)->l;
      }
   }
}

static node_t *randomized_join(randomized_t *tree, node_t *l, node_t *r, u64 sl, u64 sr) {
   node_t *root = NULL;
   node_t **p = &root;
   for (;;) {
      if (l == NULL) { *p = r; return root; }
      if (r == NULL) { *p = l; return root; }

      if (random_less_than_src(sl + sr, &tree->source) < sl) {
          tree_persist(&l);
          sl = sl - l->s - 1;
          *p = l;
           p = &l->r;
           l = *p;
      } else {
          tree_persist(&r);
          sr = r->s;
         r->s = r->s + sl;
          *p = r;
           p = &r->l;
           r = *p;
      }
   }
}

static void randomized_delete_internal(randomized_t *tree, node_t **p, u64 s, u64 i, u64 *x) {
   for (;;) {
      if (i == (*p)->s) {
         node_t *old = *p;
         tree_share(old->l);
         tree_share(old->r);
         *x = old->x;
         *p = randomized_join(tree, old->l, old->r, old->s, s - old->s - 1);
         tree_release(old);
         return;
      }
      tree_persist(p);
      if (i < (*p)->s) {
         s = (*p)->s;
    (*p)->s = (*p)->s - 1;
         p = &(*p)->l;
      } else {
         s = s - (*p)->s - 1;
         i = i - (*p)->s - 1;
         p = &(*p)->r;
      }
   }
}


// ─────────────────────────────────────────────────────────────────────────────
// Public operations
// ─────────────────────────────────────────────────────────────────────────────

static void randomized_insert(randomized_t *t, u64 i, u64 x) {
   assert_bst(i <= t->tree.size);
   randomized_insert_internal(t, &t->tree.root, t->tree.size, i, x);
   t->tree.size++;
}

static u64 randomized_delete(randomized_t *t, u64 i) {
   assert_bst(i < t->tree.size);
   u64 x;
   randomized_delete_internal(t, &t->tree.root, t->tree.size, i, &x);
   t->tree.size--;
   return x;
}


// ─────────────────────────────────────────────────────────────────────────────
// List vtable
// ─────────────────────────────────────────────────────────────────────────────

static list_t randomized_vt_new(const list_vtable_t *vt) {
   randomized_t *t = (randomized_t *)calloc(1, sizeof(randomized_t));
   xoshiro256_seed(&t->source, random_uint64());
   return (list_t){.impl = t, .vt = vt};
}

static list_t randomized_vt_clone(list_t *self) {
   randomized_t *orig = (randomized_t *)self->impl;
   randomized_t *copy = (randomized_t *)malloc(sizeof(randomized_t));
   tree_share(orig->tree.root);
   *copy = *orig;
   return (list_t){.impl = copy, .vt = self->vt};
}

static void randomized_vt_free(list_t *self) {
   randomized_t *t = (randomized_t *)self->impl;
   free_nodes(t->tree.root);
   free(t);
   self->impl = NULL;
}

static u64 randomized_vt_size(list_t *self) {
   return ((randomized_t *)self->impl)->tree.size;
}

static void randomized_vt_each(list_t *self, void (*visit)(u64, void *), void *ctx) {
   inorder(((randomized_t *)self->impl)->tree.root, visit, ctx);
}

static void randomized_vt_verify(list_t *self) {
   randomized_t *t = (randomized_t *)self->impl;
   tree_verify_size(t->tree.root, t->tree.size);
}

static u64 randomized_vt_select(list_t *self, u64 i) {
   return tree_lookup(&((randomized_t *)self->impl)->tree, i);
}

static void randomized_vt_update(list_t *self, u64 i, u64 x) {
   tree_update_base(&((randomized_t *)self->impl)->tree, i, x);
}

static void randomized_vt_insert(list_t *self, u64 i, u64 x) {
   randomized_insert((randomized_t *)self->impl, i, x);
}

static u64 randomized_vt_delete(list_t *self, u64 i) {
   return randomized_delete((randomized_t *)self->impl, i);
}

static void randomized_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   randomized_t *t = (randomized_t *)self->impl;
   assert_bst(i <= t->tree.size);
   tree_share(t->tree.root);
   node_t *l, *r;
   tree_split_internal(t->tree.root, i, &l, &r);

   randomized_t *sl = (randomized_t *)calloc(1, sizeof(randomized_t));
   sl->tree = (tree_t){.root = l, .size = i};
   sl->source = t->source;

   randomized_t *sr = (randomized_t *)calloc(1, sizeof(randomized_t));
   sr->tree = (tree_t){.root = r, .size = t->tree.size - i};
   sr->source = t->source;

   *out_l = (list_t){.impl = sl, .vt = self->vt};
   *out_r = (list_t){.impl = sr, .vt = self->vt};
}

static list_t randomized_vt_join(list_t *self, list_t *other) {
   randomized_t *t = (randomized_t *)self->impl;
   randomized_t *that = (randomized_t *)other->impl;

   tree_share(t->tree.root);
   tree_share(that->tree.root);

   randomized_t *result = (randomized_t *)calloc(1, sizeof(randomized_t));
   result->tree.root = randomized_join(t, t->tree.root, that->tree.root, t->tree.size, that->tree.size);
   result->tree.size = t->tree.size + that->tree.size;
   result->source = t->source;

   return (list_t){.impl = result, .vt = self->vt};
}

const list_vtable_t randomized_vtable = {
   .name      = "Randomized",
   .new_fn    = randomized_vt_new,
   .clone_fn  = randomized_vt_clone,
   .free_fn   = randomized_vt_free,
   .size_fn   = randomized_vt_size,
   .each_fn   = randomized_vt_each,
   .verify_fn = randomized_vt_verify,
   .select_fn = randomized_vt_select,
   .update_fn = randomized_vt_update,
   .insert_fn = randomized_vt_insert,
   .delete_fn = randomized_vt_delete,
   .split_fn  = randomized_vt_split,
   .join_fn   = randomized_vt_join,
};
