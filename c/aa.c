#include "aa.h"
#include "rank.h"

// ─────────────────────────────────────────────────────────────────────────────
// AA tree internals
//
// An AA tree by Arne Andersson. Red links may only lean right.
// This is the inverse of the LLRB constraint.
//
// Two operations maintain the invariant:
//    skew:  if the left child is a 0-child, rotate right.
//    split: if the right grandchild is a 0-child of the right child which
//           is itself a 0-child, rotate left and promote.
// ─────────────────────────────────────────────────────────────────────────────

static void aa_skew(node_t **p) {
   if (*p != NULL && isZeroChild(*p, (*p)->l)) {
      treeRotateR(p);
   }
}

static void aa_split(node_t **p) {
   if (*p != NULL && (*p)->r != NULL &&
       isZeroChild(*p, (*p)->r) &&
       isZeroChild((*p)->r, (*p)->r->r)) {
      treeRotateL(p);
      promote(*p);
   }
}

static node_t *aa_insert(node_t *p, u64 i, u64 x) {
   if (p == NULL) {
      return tree_allocate((node_t){.x = x});
   }
   tree_persist(&p);
   if (i <= p->s) {
      p->s = p->s + 1;
      p->l = aa_insert(p->l, i, x);
   } else {
      p->r = aa_insert(p->r, i - p->s - 1, x);
   }
   aa_skew(&p);
   aa_split(&p);
   return p;
}

static void aa_decreaseLevel(node_t *p) {
   int rl = rank_of(p->l);
   int rr = rank_of(p->r);

   int shouldBe = 1 + MIN(rl, rr);

   if (shouldBe < rank_of(p)) {
      p->y = (u64)shouldBe;
      if (shouldBe < rr) {
         tree_persist(&p->r);
         p->r->y = (u64)shouldBe;
      }
   }
}

static node_t *aa_fixAfterDelete(node_t *p) {
   aa_decreaseLevel(p);
   aa_skew(&p);
   if (p->r != NULL) {
      tree_persist(&p->r);
      aa_skew(&p->r);
      if (p->r->r != NULL) {
         tree_persist(&p->r->r);
         aa_skew(&p->r->r);
      }
   }
   aa_split(&p);
   if (p->r != NULL) {
      tree_persist(&p->r);
      aa_split(&p->r);
   }
   return p;
}

static node_t *aa_deleteMin(node_t *p, node_t **min) {
   tree_persist(&p);
   if (p->l == NULL) {
      node_t *r = p->r;
      p->r = NULL;
      *min = p;
      return r;
   }
   p->s = p->s - 1;
   p->l = aa_deleteMin(p->l, min);
   return aa_fixAfterDelete(p);
}

static node_t *aa_deleteMax(node_t *p, node_t **max) {
   tree_persist(&p);
   if (p->r == NULL) {
      node_t *l = p->l;
      p->l = NULL;
      *max = p;
      return l;
   }
   p->r = aa_deleteMax(p->r, max);
   return aa_fixAfterDelete(p);
}

static node_t *aa_delete(node_t *p, u64 i, u64 *x) {
   tree_persist(&p);
   if (i < p->s) {
      p->s = p->s - 1;
      p->l = aa_delete(p->l, i, x);
   } else if (i > p->s) {
      p->r = aa_delete(p->r, i - p->s - 1, x);
   } else {
      *x = p->x;
      if (p->l == NULL && p->r == NULL) {
         tree_release(p);
         return NULL;
      }
      if (p->l == NULL) {
         node_t *min = NULL;
         p->r = aa_deleteMin(p->r, &min);
         p->x = min->x;
         tree_release(min);
      } else {
         node_t *max = NULL;
         p->l = aa_deleteMax(p->l, &max);
         p->x = max->x;
         p->s = p->s - 1;
         tree_release(max);
      }
   }
   return aa_fixAfterDelete(p);
}

static int aa_abs(int x) { return x < 0 ? -x : x; }

static node_t *aa_build(node_t *l, node_t *p, node_t *r, u64 sl) {
   int rl = rank_of(l);
   int rr = rank_of(r);
   if (aa_abs(rl - rr) <= 1) {
      p->l = l;
      p->r = r;
      p->s = sl;
      p->y = (u64)(MAX(rl, rr) + 1);
      return aa_fixAfterDelete(p);
   }
   if (rl < rr) {
      tree_persist(&r);
      r->s = 1 + sl + r->s;
      r->l = aa_build(l, p, r->l, sl);
      return aa_fixAfterDelete(r);
   } else {
      tree_persist(&l);
      l->r = aa_build(l->r, p, r, sl - l->s - 1);
      return aa_fixAfterDelete(l);
   }
}

static node_t *aa_join(node_t *l, node_t *r, u64 sl, u64 sr) {
   node_t *p = NULL;
   if (l == NULL) return r;
   if (r == NULL) return l;
   if (rank_of(l) < rank_of(r) || (rank_of(l) == rank_of(r) && sl <= sr)) {
      node_t *r2 = aa_deleteMin(r, &p);
      return aa_build(l, p, r2, sl);
   } else {
      node_t *l2 = aa_deleteMax(l, &p);
      return aa_build(l2, p, r, sl - 1);
   }
}

static void aa_split_op(node_t *p, u64 i, u64 s, node_t **out_l, node_t **out_r) {
   if (p == NULL) {
      *out_l = NULL;
      *out_r = NULL;
      return;
   }
   tree_persist(&p);

   u64 sl = p->s;
   u64 sr = s - p->s - 1;

   if (i <= p->s) {
      node_t *l, *r;
      aa_split_op(p->l, i, sl, &l, &r);
      *out_l = l;
      *out_r = aa_build(r, p, p->r, sl - i);
   } else {
      node_t *l, *r;
      aa_split_op(p->r, i - sl - 1, sr, &l, &r);
      *out_l = aa_build(p->l, p, l, sl);
      *out_r = r;
   }
}

static void aa_verifyRanks(node_t *p) {
   if (p == NULL) return;
   if (isLeaf(p)) {
      invariant(rank_of(p) == 0);
   }
   // All rank differences are positive (no left 0-children allowed).
   invariant(rank_of(p) > rank_of(p->l));

   // Right child may be a 0-child or 1-child.
   invariant(isZeroChild(p, p->r) || isOneChild(p, p->r));

   // No two consecutive right 0-children.
   if (isZeroChild(p, p->r)) {
      invariant(!isZeroChild(p->r, p->r->r));
   }

   aa_verifyRanks(p->l);
   aa_verifyRanks(p->r);
}


// ─────────────────────────────────────────────────────────────────────────────
// List vtable
// ─────────────────────────────────────────────────────────────────────────────

static list_t aa_vt_new(const list_vtable_t *vt) {
   aa_t *t = (aa_t *)calloc(1, sizeof(aa_t));
   return (list_t){.impl = t, .vt = vt};
}

static list_t aa_vt_clone(list_t *self) {
   aa_t *orig = (aa_t *)self->impl;
   aa_t *copy = (aa_t *)malloc(sizeof(aa_t));
   tree_share(orig->tree.root);
   *copy = *orig;
   return (list_t){.impl = copy, .vt = self->vt};
}

static void aa_vt_free(list_t *self) {
   aa_t *t = (aa_t *)self->impl;
   free_nodes(t->tree.root);
   free(t);
   self->impl = NULL;
}

static u64  aa_vt_size(list_t *self) { return ((aa_t *)self->impl)->tree.size; }
static void aa_vt_each(list_t *self, void (*visit)(u64, void *), void *ctx) { inorder(((aa_t *)self->impl)->tree.root, visit, ctx); }

static void aa_vt_verify(list_t *self) {
   aa_t *t = (aa_t *)self->impl;
   tree_verify_size(t->tree.root, t->tree.size);
   aa_verifyRanks(t->tree.root);
}

static u64  aa_vt_select(list_t *self, u64 i) { return tree_lookup(&((aa_t *)self->impl)->tree, i); }
static void aa_vt_update(list_t *self, u64 i, u64 x) { tree_update_base(&((aa_t *)self->impl)->tree, i, x); }

static void aa_vt_insert(list_t *self, u64 i, u64 x) {
   aa_t *t = (aa_t *)self->impl;
   assert_bst(i <= t->tree.size);
   t->tree.root = aa_insert(t->tree.root, i, x);
   t->tree.size++;
}

static u64 aa_vt_delete(list_t *self, u64 i) {
   aa_t *t = (aa_t *)self->impl;
   assert_bst(i < t->tree.size);
   u64 x;
   t->tree.root = aa_delete(t->tree.root, i, &x);
   t->tree.size--;
   return x;
}

static void aa_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   aa_t *t = (aa_t *)self->impl;
   assert_bst(i <= t->tree.size);
   tree_share(t->tree.root);
   node_t *l, *r;
   aa_split_op(t->tree.root, i, t->tree.size, &l, &r);
   aa_t *sl = (aa_t *)calloc(1, sizeof(aa_t)); sl->tree = (tree_t){l, i};
   aa_t *sr = (aa_t *)calloc(1, sizeof(aa_t)); sr->tree = (tree_t){r, t->tree.size - i};
   *out_l = (list_t){sl, self->vt};
   *out_r = (list_t){sr, self->vt};
}

static list_t aa_vt_join(list_t *self, list_t *other) {
   aa_t *t = (aa_t *)self->impl;
   aa_t *that = (aa_t *)other->impl;
   tree_share(t->tree.root);
   tree_share(that->tree.root);
   aa_t *result = (aa_t *)calloc(1, sizeof(aa_t));
   result->tree.root = aa_join(t->tree.root, that->tree.root, t->tree.size, that->tree.size);
   result->tree.size = t->tree.size + that->tree.size;
   return (list_t){result, self->vt};
}

const list_vtable_t aa_vtable = {
   .name      = "AA",
   .new_fn    = aa_vt_new,
   .clone_fn  = aa_vt_clone,
   .free_fn   = aa_vt_free,
   .size_fn   = aa_vt_size,
   .each_fn   = aa_vt_each,
   .verify_fn = aa_vt_verify,
   .select_fn = aa_vt_select,
   .update_fn = aa_vt_update,
   .insert_fn = aa_vt_insert,
   .delete_fn = aa_vt_delete,
   .split_fn  = aa_vt_split,
   .join_fn   = aa_vt_join,
};
