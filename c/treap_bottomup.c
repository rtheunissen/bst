#include "treap_bottomup.h"

// ─────────────────────────────────────────────────────────────────────────────
// Internal operations
// ─────────────────────────────────────────────────────────────────────────────

static u64 treap_rank(node_t *p) {
   if (p == NULL) return 0;
   return p->y;
}

static node_t *treap_bu_join(node_t *l, node_t *r, u64 sl) {
   node_t *root = NULL;
   node_t **p = &root;
   for (;;) {
      if (l == NULL) { *p = r; return root; }
      if (r == NULL) { *p = l; return root; }

      if (treap_rank(l) >= treap_rank(r)) {
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

static node_t *treap_bu_insert(treap_bottomup_t *tree, node_t *p, u64 i, u64 x) {
   if (p == NULL) {
      return tree_allocate((node_t){.x = x, .y = xoshiro256_uint64(&tree->source)});
   }
   tree_persist(&p);
   if (i <= p->s) {
      p->s = p->s + 1;
      p->l = treap_bu_insert(tree, p->l, i, x);
      if (p->l->y > p->y) {
         treeRotateR(&p);
      }
   } else {
      p->r = treap_bu_insert(tree, p->r, i - p->s - 1, x);
      if (p->r->y > p->y) {
         treeRotateL(&p);
      }
   }
   return p;
}

static node_t *treap_bu_delete(node_t *p, u64 i, u64 *x) {
   if (i == p->s) {
      *x = p->x;
      tree_share(p->l);
      tree_share(p->r);
      node_t *result = treap_bu_join(p->l, p->r, p->s);
      tree_release(p);
      return result;
   }
   tree_persist(&p);
   if (i < p->s) {
      p->s = p->s - 1;
      p->l = treap_bu_delete(p->l, i, x);
   } else {
      p->r = treap_bu_delete(p->r, i - p->s - 1, x);
   }
   return p;
}

static void treap_bu_verifyMaxRankHeap(node_t *p) {
   if (p == NULL) return;
   invariant(treap_rank(p) >= treap_rank(p->l));
   invariant(treap_rank(p) >= treap_rank(p->r));
   treap_bu_verifyMaxRankHeap(p->l);
   treap_bu_verifyMaxRankHeap(p->r);
}


// ─────────────────────────────────────────────────────────────────────────────
// List vtable
// ─────────────────────────────────────────────────────────────────────────────

static list_t treap_bu_vt_new(const list_vtable_t *vt) {
   treap_bottomup_t *t = (treap_bottomup_t *)calloc(1, sizeof(treap_bottomup_t));
   xoshiro256_seed(&t->source, random_uint64());
   return (list_t){.impl = t, .vt = vt};
}

static list_t treap_bu_vt_clone(list_t *self) {
   treap_bottomup_t *orig = (treap_bottomup_t *)self->impl;
   treap_bottomup_t *copy = (treap_bottomup_t *)malloc(sizeof(treap_bottomup_t));
   tree_share(orig->tree.root);
   *copy = *orig;
   return (list_t){.impl = copy, .vt = self->vt};
}

static void treap_bu_vt_free(list_t *self) {
   treap_bottomup_t *t = (treap_bottomup_t *)self->impl;
   free_nodes(t->tree.root);
   free(t);
   self->impl = NULL;
}

static u64  treap_bu_vt_size(list_t *self) { return ((treap_bottomup_t *)self->impl)->tree.size; }
static void treap_bu_vt_each(list_t *self, void (*visit)(u64, void *), void *ctx) { inorder(((treap_bottomup_t *)self->impl)->tree.root, visit, ctx); }

static void treap_bu_vt_verify(list_t *self) {
   treap_bottomup_t *t = (treap_bottomup_t *)self->impl;
   tree_verify_size(t->tree.root, t->tree.size);
   treap_bu_verifyMaxRankHeap(t->tree.root);
}

static u64  treap_bu_vt_select(list_t *self, u64 i) { return tree_lookup(&((treap_bottomup_t *)self->impl)->tree, i); }
static void treap_bu_vt_update(list_t *self, u64 i, u64 x) { tree_update_base(&((treap_bottomup_t *)self->impl)->tree, i, x); }

static void treap_bu_vt_insert(list_t *self, u64 i, u64 x) {
   treap_bottomup_t *t = (treap_bottomup_t *)self->impl;
   assert_bst(i <= t->tree.size);
   t->tree.root = treap_bu_insert(t, t->tree.root, i, x);
   t->tree.size++;
}

static u64 treap_bu_vt_delete(list_t *self, u64 i) {
   treap_bottomup_t *t = (treap_bottomup_t *)self->impl;
   assert_bst(i < t->tree.size);
   u64 x;
   t->tree.root = treap_bu_delete(t->tree.root, i, &x);
   t->tree.size--;
   return x;
}

static void treap_bu_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   treap_bottomup_t *t = (treap_bottomup_t *)self->impl;
   assert_bst(i <= t->tree.size);
   tree_share(t->tree.root);
   node_t *l, *r;
   tree_split_internal(t->tree.root, i, &l, &r);
   treap_bottomup_t *sl = (treap_bottomup_t *)calloc(1, sizeof(treap_bottomup_t));
   sl->tree = (tree_t){l, i}; sl->source = t->source;
   treap_bottomup_t *sr = (treap_bottomup_t *)calloc(1, sizeof(treap_bottomup_t));
   sr->tree = (tree_t){r, t->tree.size - i}; sr->source = t->source;
   *out_l = (list_t){sl, self->vt};
   *out_r = (list_t){sr, self->vt};
}

static list_t treap_bu_vt_join(list_t *self, list_t *other) {
   treap_bottomup_t *t = (treap_bottomup_t *)self->impl;
   treap_bottomup_t *that = (treap_bottomup_t *)other->impl;
   tree_share(t->tree.root);
   tree_share(that->tree.root);
   treap_bottomup_t *result = (treap_bottomup_t *)calloc(1, sizeof(treap_bottomup_t));
   result->tree.root = treap_bu_join(t->tree.root, that->tree.root, t->tree.size);
   result->tree.size = t->tree.size + that->tree.size;
   result->source = t->source;
   return (list_t){result, self->vt};
}

const list_vtable_t treap_bottomup_vtable = {
   .name      = "TreapBottomUp",
   .new_fn    = treap_bu_vt_new,
   .clone_fn  = treap_bu_vt_clone,
   .free_fn   = treap_bu_vt_free,
   .size_fn   = treap_bu_vt_size,
   .each_fn   = treap_bu_vt_each,
   .verify_fn = treap_bu_vt_verify,
   .select_fn = treap_bu_vt_select,
   .update_fn = treap_bu_vt_update,
   .insert_fn = treap_bu_vt_insert,
   .delete_fn = treap_bu_vt_delete,
   .split_fn  = treap_bu_vt_split,
   .join_fn   = treap_bu_vt_join,
};
