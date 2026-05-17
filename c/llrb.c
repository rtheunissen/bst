#include "llrb.h"
#include "rank.h"
#include <math.h>

// ─────────────────────────────────────────────────────────────────────────────
// LLRB internals
//
// A left-leaning red-black tree, based on Sedgewick's simplification.
// Red links (0-children) may only lean left.
// ─────────────────────────────────────────────────────────────────────────────

static node_t *llrb_fixUp(node_t *p) {
   //
   // Right-leaning red link: rotate left to make it left-leaning.
   //
   if (isRed(p, p->r) && !isRed(p, p->l)) {
      treeRotateL(&p);
   }
   //
   // Two consecutive left red links: rotate right.
   //
   if (isRed(p, p->l) && isRed(p->l, p->l->l)) {
      treeRotateR(&p);
   }
   //
   // Both children red: promote (color flip).
   //
   if (isRed(p, p->l) && isRed(p, p->r)) {
      promote(p);
   }
   return p;
}

static node_t *llrb_insert(node_t *p, u64 i, u64 x) {
   if (p == NULL) {
      return tree_allocate((node_t){.x = x});
   }
   tree_persist(&p);
   if (i <= p->s) {
      p->s = p->s + 1;
      p->l = llrb_insert(p->l, i, x);
   } else {
      p->r = llrb_insert(p->r, i - p->s - 1, x);
   }
   return llrb_fixUp(p);
}

// moveRedLeft makes p->l or one of its children red, assuming that p is red
// and both p->l and p->l->l are black.
static node_t *llrb_moveRedLeft(node_t *p) {
   demote(p);
   if (p->r != NULL && isRed(p->r, p->r->l)) {
      tree_persist(&p->r);
      treeRotateR(&p->r);
      treeRotateL(&p);
      promote(p);
   }
   return p;
}

// moveRedRight makes p->r or one of its children red, assuming that p is red
// and both p->r and p->r->l are black.
static node_t *llrb_moveRedRight(node_t *p) {
   demote(p);
   if (p->l != NULL && isRed(p->l, p->l->l)) {
      treeRotateR(&p);
      promote(p);
   }
   return p;
}

static node_t *llrb_deleteMin(node_t *p, node_t **min) {
   tree_persist(&p);
   if (p->l == NULL) {
      *min = p;
      p->r = NULL;
      return NULL;
   }
   if (!isRed(p, p->l) && !isRed(p->l, p->l->l)) {
      p = llrb_moveRedLeft(p);
   }
   p->s = p->s - 1;
   p->l = llrb_deleteMin(p->l, min);
   return llrb_fixUp(p);
}

static node_t *llrb_delete(node_t *p, u64 i, u64 *x) {
   tree_persist(&p);
   if (i < p->s) {
      if (!isRed(p, p->l) && (p->l == NULL || !isRed(p->l, p->l->l))) {
         p = llrb_moveRedLeft(p);
      }
      p->s = p->s - 1;
      p->l = llrb_delete(p->l, i, x);
   } else {
      if (isRed(p, p->l)) {
         treeRotateR(&p);
      }
      if (i == p->s && p->r == NULL) {
         *x = p->x;
         tree_release(p);
         return NULL;
      }
      if (!isRed(p, p->r) && (p->r == NULL || !isRed(p->r, p->r->l))) {
         p = llrb_moveRedRight(p);
      }
      if (i == p->s) {
         *x = p->x;
         node_t *min = NULL;
         p->r = llrb_deleteMin(p->r, &min);
         p->x = min->x;
         tree_release(min);
      } else {
         p->r = llrb_delete(p->r, i - p->s - 1, x);
      }
   }
   return llrb_fixUp(p);
}

static node_t *llrb_balanceInsertL(node_t *p) {
   if (isRed(p, p->l)) {
      if (isRed(p, p->r)) {
         if (isRed(p->l, p->l->l) || isRed(p->l, p->l->r)) {
            promote(p);
         }
      } else {
         if (isRed(p->l, p->l->l)) {
            treeRotateR(&p);
         } else if (isRed(p->l, p->l->r)) {
            treeRotateLR(&p);
         }
      }
   }
   return llrb_fixUp(p);
}

static node_t *llrb_balanceInsertR(node_t *p) {
   if (isRed(p, p->r)) {
      if (isRed(p, p->l)) {
         if (isRed(p->r, p->r->r) || isRed(p->r, p->r->l)) {
            promote(p);
         }
      } else {
         if (isRed(p->r, p->r->r)) {
            treeRotateL(&p);
         } else if (isRed(p->r, p->r->l)) {
            treeRotateRL(&p);
         }
      }
   }
   return llrb_fixUp(p);
}

static node_t *llrb_build(node_t *l, node_t *p, node_t *r, u64 sl) {
   if (rank_of(l) == rank_of(r)) {
      p->l = l;
      p->r = r;
      p->s = sl;
      p->y = (u64)rank_of(l);
      if (r == NULL || hasZeroChild(l) || hasZeroChild(r)) {
         promote(p);
      }
      return llrb_fixUp(p);
   }
   if (rank_of(l) < rank_of(r)) {
      tree_persist(&r);
      r->s = 1 + sl + r->s;
      r->l = llrb_build(l, p, r->l, sl);
      return llrb_balanceInsertL(r);
   } else {
      tree_persist(&l);
      l->r = llrb_build(l->r, p, r, sl - l->s - 1);
      return llrb_balanceInsertR(l);
   }
}

static node_t *llrb_deleteMax(node_t *p, node_t **max) {
   tree_persist(&p);
   if (isRed(p, p->l)) {
      treeRotateR(&p);
   }
   if (p->r == NULL) {
      node_t *l = p->l;
      p->l = NULL;
      *max = p;
      return l;
   }
   if (!isRed(p, p->r) && (p->r->l == NULL || !isRed(p->r, p->r->l))) {
      p = llrb_moveRedRight(p);
   }
   p->r = llrb_deleteMax(p->r, max);
   return llrb_fixUp(p);
}

static node_t *llrb_join(node_t *l, node_t *r, u64 sl, u64 sr) {
   node_t *p = NULL;
   if (l == NULL) return r;
   if (r == NULL) return l;
   if (rank_of(l) < rank_of(r) || (rank_of(l) == rank_of(r) && sl <= sr)) {
      node_t *r2 = llrb_deleteMin(r, &p);
      return llrb_build(l, p, r2, sl);
   } else {
      node_t *l2 = llrb_deleteMax(l, &p);
      return llrb_build(l2, p, r, sl - 1);
   }
}

static void llrb_split(node_t *p, u64 i, u64 s, node_t **out_l, node_t **out_r) {
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
      llrb_split(p->l, i, sl, &l, &r);
      *out_l = l;
      *out_r = llrb_build(r, p, p->r, sl - i);
   } else {
      node_t *l, *r;
      llrb_split(p->r, i - sl - 1, sr, &l, &r);
      *out_l = llrb_build(p->l, p, l, sl);
      *out_r = r;
   }
}

static void llrb_verifyLeftLeaning(node_t *p) {
   if (p == NULL) return;
   // No right-leaning red links: if right is red, left must also be red.
   invariant(!isRed(p, p->r) || isRed(p, p->l));
   llrb_verifyLeftLeaning(p->l);
   llrb_verifyLeftLeaning(p->r);
}


// ─────────────────────────────────────────────────────────────────────────────
// List vtable
// ─────────────────────────────────────────────────────────────────────────────

static list_t llrb_vt_new(const list_vtable_t *vt) {
   llrb_t *t = (llrb_t *)calloc(1, sizeof(llrb_t));
   return (list_t){.impl = t, .vt = vt};
}

static list_t llrb_vt_clone(list_t *self) {
   llrb_t *orig = (llrb_t *)self->impl;
   llrb_t *copy = (llrb_t *)malloc(sizeof(llrb_t));
   tree_share(orig->tree.root);
   *copy = *orig;
   return (list_t){.impl = copy, .vt = self->vt};
}

static void llrb_vt_free(list_t *self) {
   llrb_t *t = (llrb_t *)self->impl;
   free_nodes(t->tree.root);
   free(t);
   self->impl = NULL;
}

static u64  llrb_vt_size(list_t *self) { return ((llrb_t *)self->impl)->tree.size; }
static void llrb_vt_each(list_t *self, void (*visit)(u64, void *), void *ctx) { inorder(((llrb_t *)self->impl)->tree.root, visit, ctx); }

static void llrb_vt_verify(list_t *self) {
   llrb_t *t = (llrb_t *)self->impl;
   tree_verify_size(t->tree.root, t->tree.size);
   verifyRedBlackRanks(t->tree.root);
   llrb_verifyLeftLeaning(t->tree.root);
   verifyRedBlackHeight(t->tree.root, t->tree.size);
}

static u64  llrb_vt_select(list_t *self, u64 i) { return tree_lookup(&((llrb_t *)self->impl)->tree, i); }
static void llrb_vt_update(list_t *self, u64 i, u64 x) { tree_update_base(&((llrb_t *)self->impl)->tree, i, x); }

static void llrb_vt_insert(list_t *self, u64 i, u64 x) {
   llrb_t *t = (llrb_t *)self->impl;
   assert_bst(i <= t->tree.size);
   t->tree.root = llrb_insert(t->tree.root, i, x);
   t->tree.size++;
}

static u64 llrb_vt_delete(list_t *self, u64 i) {
   llrb_t *t = (llrb_t *)self->impl;
   assert_bst(i < t->tree.size);
   u64 x;
   t->tree.root = llrb_delete(t->tree.root, i, &x);
   t->tree.size--;
   return x;
}

static void llrb_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   llrb_t *t = (llrb_t *)self->impl;
   assert_bst(i <= t->tree.size);
   tree_share(t->tree.root);
   node_t *l, *r;
   llrb_split(t->tree.root, i, t->tree.size, &l, &r);
   llrb_t *sl = (llrb_t *)calloc(1, sizeof(llrb_t)); sl->tree = (tree_t){l, i};
   llrb_t *sr = (llrb_t *)calloc(1, sizeof(llrb_t)); sr->tree = (tree_t){r, t->tree.size - i};
   *out_l = (list_t){sl, self->vt};
   *out_r = (list_t){sr, self->vt};
}

static list_t llrb_vt_join(list_t *self, list_t *other) {
   llrb_t *t = (llrb_t *)self->impl;
   llrb_t *that = (llrb_t *)other->impl;
   tree_share(t->tree.root);
   tree_share(that->tree.root);
   llrb_t *result = (llrb_t *)calloc(1, sizeof(llrb_t));
   result->tree.root = llrb_join(t->tree.root, that->tree.root, t->tree.size, that->tree.size);
   result->tree.size = t->tree.size + that->tree.size;
   return (list_t){result, self->vt};
}

const list_vtable_t llrb_vtable = {
   .name      = "LLRB",
   .new_fn    = llrb_vt_new,
   .clone_fn  = llrb_vt_clone,
   .free_fn   = llrb_vt_free,
   .size_fn   = llrb_vt_size,
   .each_fn   = llrb_vt_each,
   .verify_fn = llrb_vt_verify,
   .select_fn = llrb_vt_select,
   .update_fn = llrb_vt_update,
   .insert_fn = llrb_vt_insert,
   .delete_fn = llrb_vt_delete,
   .split_fn  = llrb_vt_split,
   .join_fn   = llrb_vt_join,
};
