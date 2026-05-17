#include "conc.h"

// ─────────────────────────────────────────────────────────────────────────────
// Conc tree internals
//
// https://api.semanticscholar.org/CorpusID:21267485
// Prokopec, A., & Odersky, M. (2015). Conc-Trees for Functional and Parallel Programming.
//
// Note: Conc trees store data only in leaves. Internal nodes are "link" nodes.
// The `s` field stores the total number of leaves in the subtree.
// The `y` field stores the height.
// ─────────────────────────────────────────────────────────────────────────────

static node_t *conc_asLeaf(u64 x) {
   return tree_allocate((node_t){.x = x, .s = 1});
}

static node_t *conc_link(node_t *l, node_t *r) {
   return tree_allocate((node_t){
      .y = MAX(l->y, r->y) + 1,
      .s = l->s + r->s,
      .l = l,
      .r = r,
   });
}

static void conc_unpack(node_t *p, node_t **l, node_t **r) {
   tree_persist(&p);
   *l = p->l;
   *r = p->r;
   p->l = NULL;
   p->r = NULL;
   tree_release(p);
}

static node_t *conc_concat(node_t *l, node_t *r);

static node_t *conc_balanced(node_t *l, node_t *r) {
   if (l == NULL) return r;
   if (r == NULL) return l;
   return conc_concat(l, r);
}

static node_t *conc_concat(node_t *l, node_t *r) {
   if (l->y > r->y && ((l->y - r->y) > 1)) {
      u64 ly = l->y;
      node_t *ll, *lr;
      conc_unpack(l, &ll, &lr);
      if (ll->y >= lr->y) {
         return conc_link(ll, conc_concat(lr, r));
      }
      node_t *lrl, *lrr;
      conc_unpack(lr, &lrl, &lrr);
      node_t *rr = conc_concat(lrr, r);
      if (ly - rr->y < 3) {
         return conc_link(conc_link(ll, lrl), rr);
      } else {
         return conc_link(ll, conc_link(lrl, rr));
      }
   }
   if (r->y > l->y && ((r->y - l->y) > 1)) {
      u64 ry = r->y;
      node_t *rl, *rr;
      conc_unpack(r, &rl, &rr);
      if (rr->y >= rl->y) {
         return conc_link(conc_concat(l, rl), rr);
      }
      node_t *rll, *rlr;
      conc_unpack(rl, &rll, &rlr);
      node_t *ll = conc_concat(l, rll);
      if (ry - ll->y < 3) {
         return conc_link(ll, conc_link(rlr, rr));
      } else {
         return conc_link(conc_link(ll, rlr), rr);
      }
   }
   return conc_link(l, r);
}

static node_t *conc_lookup(node_t *p, u64 i) {
   if (isLeaf(p)) {
      assert_bst(i == 0);
      return p;
   }
   if (i < p->l->s) {
      return conc_lookup(p->l, i);
   } else {
      return conc_lookup(p->r, i - p->l->s);
   }
}

static node_t *conc_update(node_t *p, u64 i, u64 x) {
   if (isLeaf(p)) {
      tree_release(p);
      return conc_asLeaf(x);
   }
   u64 sl = p->l->s;
   node_t *l, *r;
   conc_unpack(p, &l, &r);
   if (i < sl) {
      return conc_link(conc_update(l, i, x), r);
   } else {
      return conc_link(l, conc_update(r, i - sl, x));
   }
}

static node_t *conc_insert(node_t *p, u64 i, u64 x) {
   if (isLeaf(p)) {
      if (i == 0) {
         return conc_link(conc_asLeaf(x), p);
      } else {
         return conc_link(p, conc_asLeaf(x));
      }
   }
   u64 sl = p->l->s;
   node_t *l, *r;
   conc_unpack(p, &l, &r);
   if (i < sl) {
      return conc_balanced(conc_insert(l, i, x), r);
   } else {
      return conc_balanced(l, conc_insert(r, i - sl, x));
   }
}

static node_t *conc_delete(node_t *p, u64 i, u64 *x) {
   if (isLeaf(p)) {
      *x = p->x;
      tree_release(p);
      return NULL;
   }
   u64 sl = p->l->s;
   node_t *l, *r;
   conc_unpack(p, &l, &r);
   if (i < sl) {
      return conc_balanced(conc_delete(l, i, x), r);
   } else {
      return conc_balanced(l, conc_delete(r, i - sl, x));
   }
}

static void conc_split(node_t *p, u64 i, node_t **out_l, node_t **out_r) {
   if (isLeaf(p)) {
      if (i == 0) {
         *out_l = NULL;
         *out_r = p;
      } else {
         *out_l = p;
         *out_r = NULL;
      }
      return;
   }
   u64 sl = p->l->s;
   node_t *pl, *pr;
   conc_unpack(p, &pl, &pr);
   if (i < sl) {
      node_t *l, *r;
      conc_split(pl, i, &l, &r);
      *out_l = l;
      *out_r = conc_balanced(r, pr);
   } else {
      node_t *l, *r;
      conc_split(pr, i - sl, &l, &r);
      *out_l = conc_balanced(pl, l);
      *out_r = r;
   }
}

// Conc inorder: data is only in leaves.
static void conc_inorder(node_t *p, void (*visit)(u64, void *), void *ctx) {
   if (p == NULL) return;
   if (isLeaf(p)) {
      visit(p->x, ctx);
   } else {
      conc_inorder(p->l, visit, ctx);
      conc_inorder(p->r, visit, ctx);
   }
}

static u64 conc_count(node_t *p) {
   if (isLeaf(p)) return 1;
   return conc_count(p->l) + conc_count(p->r);
}

static void conc_verifyHeights(node_t *p) {
   if (p == NULL) return;
   if (isLeaf(p)) {
      invariant(p->y == 0);
   } else {
      int diff = (int)p->l->y - (int)p->r->y;
      if (diff < 0) diff = -diff;
      invariant(diff <= 1);
   }
   conc_verifyHeights(p->l);
   conc_verifyHeights(p->r);
}

static void conc_verifyLeaves(node_t *p) {
   if (p == NULL) return;
   invariant((p->l == NULL) == (p->r == NULL));
   conc_verifyLeaves(p->l);
   conc_verifyLeaves(p->r);
}

static void conc_verifySizes(node_t *p) {
   if (p == NULL) return;
   invariant(p->s == conc_count(p));
   conc_verifySizes(p->l);
   conc_verifySizes(p->r);
}


// ─────────────────────────────────────────────────────────────────────────────
// List vtable
// ─────────────────────────────────────────────────────────────────────────────

static list_t conc_vt_new(const list_vtable_t *vt) {
   conc_t *t = (conc_t *)calloc(1, sizeof(conc_t));
   return (list_t){.impl = t, .vt = vt};
}

static list_t conc_vt_clone(list_t *self) {
   conc_t *orig = (conc_t *)self->impl;
   conc_t *copy = (conc_t *)malloc(sizeof(conc_t));
   tree_share(orig->tree.root);
   *copy = *orig;
   return (list_t){.impl = copy, .vt = self->vt};
}

static void conc_vt_free(list_t *self) {
   conc_t *t = (conc_t *)self->impl;
   free_nodes(t->tree.root);
   free(t);
   self->impl = NULL;
}

static u64 conc_vt_size(list_t *self) { return ((conc_t *)self->impl)->tree.size; }

static void conc_vt_each(list_t *self, void (*visit)(u64, void *), void *ctx) {
   conc_inorder(((conc_t *)self->impl)->tree.root, visit, ctx);
}

static void conc_vt_verify(list_t *self) {
   conc_t *t = (conc_t *)self->impl;
   conc_verifyLeaves(t->tree.root);
   conc_verifyHeights(t->tree.root);
   conc_verifySizes(t->tree.root);
}

static u64 conc_vt_select(list_t *self, u64 i) {
   conc_t *t = (conc_t *)self->impl;
   assert_bst(i < t->tree.size);
   return conc_lookup(t->tree.root, i)->x;
}

static void conc_vt_update(list_t *self, u64 i, u64 x) {
   conc_t *t = (conc_t *)self->impl;
   assert_bst(i < t->tree.size);
   t->tree.root = conc_update(t->tree.root, i, x);
}

static void conc_vt_insert(list_t *self, u64 i, u64 x) {
   conc_t *t = (conc_t *)self->impl;
   assert_bst(i <= t->tree.size);
   t->tree.size++;
   if (t->tree.root == NULL) {
      t->tree.root = conc_asLeaf(x);
   } else {
      t->tree.root = conc_insert(t->tree.root, i, x);
   }
}

static u64 conc_vt_delete(list_t *self, u64 i) {
   conc_t *t = (conc_t *)self->impl;
   assert_bst(i < t->tree.size);
   u64 x;
   t->tree.size--;
   t->tree.root = conc_delete(t->tree.root, i, &x);
   return x;
}

static void conc_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   conc_t *t = (conc_t *)self->impl;
   assert_bst(i <= t->tree.size);
   node_t *lp = NULL, *rp = NULL;
   if (t->tree.root != NULL) {
      tree_share(t->tree.root);
      conc_split(t->tree.root, i, &lp, &rp);
   }
   conc_t *sl = (conc_t *)calloc(1, sizeof(conc_t)); sl->tree = (tree_t){lp, i};
   conc_t *sr = (conc_t *)calloc(1, sizeof(conc_t)); sr->tree = (tree_t){rp, t->tree.size - i};
   *out_l = (list_t){sl, self->vt};
   *out_r = (list_t){sr, self->vt};
}

static list_t conc_vt_join(list_t *self, list_t *other) {
   conc_t *t = (conc_t *)self->impl;
   conc_t *that = (conc_t *)other->impl;
   conc_t *result = (conc_t *)calloc(1, sizeof(conc_t));
   tree_share(t->tree.root);
   tree_share(that->tree.root);
   result->tree.root = conc_balanced(t->tree.root, that->tree.root);
   result->tree.size = t->tree.size + that->tree.size;
   return (list_t){result, self->vt};
}

const list_vtable_t conc_vtable = {
   .name      = "Conc",
   .new_fn    = conc_vt_new,
   .clone_fn  = conc_vt_clone,
   .free_fn   = conc_vt_free,
   .size_fn   = conc_vt_size,
   .each_fn   = conc_vt_each,
   .verify_fn = conc_vt_verify,
   .select_fn = conc_vt_select,
   .update_fn = conc_vt_update,
   .insert_fn = conc_vt_insert,
   .delete_fn = conc_vt_delete,
   .split_fn  = conc_vt_split,
   .join_fn   = conc_vt_join,
};
