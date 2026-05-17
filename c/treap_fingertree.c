// TreapFingerTree — the most complex tree implementation.
//
// A finger tree where each finger is a treap. The root node's `l` points to
// the "head" (left spine) and `r` to the "tail" (right spine). Nodes on the
// left spine have their `l` pointing deeper into the spine and `r` pointing
// to a treap subtree. The right spine is symmetric.
//
#include "treap_fingertree.h"

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static u64 tft_rank(node_t *p) {
   if (p == NULL) return 0;
   return p->y;
}

// Node-level lookup (doesn't need tree_t)
static u64 node_lookup(node_t *p, u64 i) {
   for (;;) {
      if (i == p->s) return p->x;
      if (i < p->s) p = p->l;
      else { i = i - p->s - 1; p = p->r; }
   }
}

// Node-level update with persistence
static void node_update(node_t *p, u64 i, u64 x) {
   for (;;) {
      if (i == p->s) { p->x = x; return; }
      if (i < p->s) { tree_persist(&p->l); p = p->l; }
      else { tree_persist(&p->r); i = i - p->s - 1; p = p->r; }
   }
}


// ─────────────────────────────────────────────────────────────────────────────
// Reverse operations (convert between finger-tree and treap spine formats)
// ─────────────────────────────────────────────────────────────────────────────

static node_t *tft_reverseL(node_t *p, node_t *g, u64 s) {
   assert_bst(s == node_size(p));
   for (;;) {
      if (p == NULL) return g;
      tree_persist(&p);
      u64 sl = p->s;
      p->s = s - p->s - 1;
      node_t *l = p->l;
      p->l = g;
      g = p;
      p = l;
      s = sl;
   }
}

static node_t *tft_reverseR(node_t *p, node_t *g) {
   for (;;) {
      if (p == NULL) return g;
      tree_persist(&p);
      node_t *r = p->r;
      p->r = g;
      g = p;
      p = r;
   }
}

static node_t *tft_reverseL2(node_t *p, node_t *g) {
   u64 s = 0;
   for (;;) {
      if (p == NULL) return g;
      tree_persist(&p);
      s = s + p->s + 1;
      p->s = s - p->s - 1;
      node_t *l = p->l;
      p->l = g;
      g = p;
      p = l;
   }
}


// ─────────────────────────────────────────────────────────────────────────────
// Spine rotation operations
// ─────────────────────────────────────────────────────────────────────────────

static void tft_rotateParentLeftOnRightSpine(node_t *p) {
   tree_persist(&p->r);
   node_t *r = p->r;
   p->r = r->r;
   r->r = p->l;
   p->l = r;
   p->s = p->s + r->s + 1;
}

static void tft_rotateParentRightOnLeftSpine(node_t *p) {
   tree_persist(&p->l);
   node_t *l = p->l;
   p->l = l->l;
   l->l = p->r;
   p->r = l;
   p->s = p->s + l->s + 1;
   l->s = p->s - l->s - 1;
}

static void tft_rotateRightIntoRoot(treap_fingertree_t *tree, node_t *l) {
   assert_bst(l->l == NULL);
   node_t *p = tree->tree.root;
   tree_appendR(&p->r, p);
   l->l = p->l;
   p->l = l->r;
   l->r = p->r;
   p->r = NULL;
   l->s = p->s - l->s - 1;
   p->s = p->s - l->s - 1;
   tree->tree.root = l;
}

static void tft_rotateLeftIntoRoot(treap_fingertree_t *tree, node_t *r) {
   assert_bst(r->r == NULL);
   node_t *p = tree->tree.root;
   tree_appendL(&p->l, p);
   r->r = p->r;
   p->r = r->l;
   r->l = p->l;
   p->l = NULL;
   r->s = r->s + p->s + 1;
   p->s = r->s - p->s - 1;
   tree->tree.root = r;
}

static node_t *tft_rotateUpR(treap_fingertree_t *tree, node_t *p) {
   for (;;) {
      if (p->r == NULL) {
         if (p->y > tree->tree.root->y) {
            tft_rotateLeftIntoRoot(tree, p);
            return NULL;
         }
      } else {
         if (p->y > p->r->y) {
            tft_rotateParentLeftOnRightSpine(p);
            continue;
         }
      }
      return p;
   }
}

static node_t *tft_rotateUpL(treap_fingertree_t *tree, node_t *p) {
   for (;;) {
      if (p->l == NULL) {
         if (p->y > tree->tree.root->y) {
            tft_rotateRightIntoRoot(tree, p);
            return NULL;
         }
      } else {
         if (p->y > p->l->y) {
            tft_rotateParentRightOnLeftSpine(p);
            continue;
         }
      }
      return p;
   }
}

static void tft_rotateDownL(node_t *p) {
   while (p->r != NULL && p->r->y > p->y) {
      tree_persist(&p->r);
      node_t *r = p->r;
      p->r = r->l;
      r->l = p->l;
      p->l = r;
      r->s = p->s - r->s - 1;
      p->s = p->s - r->s - 1;
   }
}

static void tft_rotateDownR(node_t *p) {
   while (p->l != NULL && p->l->y > p->y) {
      tree_persist(&p->l);
      node_t *l = p->l;
      p->l = l->r;
      l->r = p->r;
      p->r = l;
      p->s = p->s - l->s - 1;
   }
}


// ─────────────────────────────────────────────────────────────────────────────
// Treap join (not finger-tree join — joins two treaps by priority)
// ─────────────────────────────────────────────────────────────────────────────

static node_t *tft_treap_join(node_t *l, node_t *r, u64 sl) {
   assert_bst(sl == node_size(l));
   node_t *root = NULL;
   node_t **p = &root;
   for (;;) {
      if (l == NULL) { *p = r; return root; }
      if (r == NULL) { *p = l; return root; }
      if (l->y >= r->y) {
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


// ─────────────────────────────────────────────────────────────────────────────
// Internal treap insert (used when inserting into interior subtrees)
// ─────────────────────────────────────────────────────────────────────────────

static void tft_treap_insert(node_t **p, u64 i, node_t *n) {
   for (;;) {
      if (*p == NULL) { *p = n; return; }
      if ((*p)->y <= n->y) {
         tree_split_internal(*p, i, &n->l, &n->r);
         n->s = i;
         *p = n;
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

// Internal treap delete
static void tft_treap_delete(node_t **p, u64 i, u64 *x) {
   for (;;) {
      if (i == (*p)->s) {
         node_t *old = *p;
         tree_share(old->l);
         tree_share(old->r);
         *x = old->x;
         *p = tft_treap_join(old->l, old->r, old->s);
         tree_release(old);
         return;
      }
      tree_persist(p);
      if (i < (*p)->s) {
         p = deleteL(*p);
      } else {
         p = deleteR(*p, &i);
      }
   }
}


// ─────────────────────────────────────────────────────────────────────────────
// Insert operations
// ─────────────────────────────────────────────────────────────────────────────

static void tft_insertAsLast(treap_fingertree_t *tree, u64 x) {
   tree_persist(&tree->tree.root);
   node_t *p = tree_allocate((node_t){.x = x, .y = xoshiro256_uint64(&tree->source)});
   p->r = tree->tree.root->r;
   p = tft_rotateUpR(tree, p);
   tree->tree.root->r = p;
   tree->tree.size++;
}

static void tft_insertAsFirst(treap_fingertree_t *tree, u64 x) {
   tree_persist(&tree->tree.root);
   node_t *p = tree_allocate((node_t){.x = x, .y = xoshiro256_uint64(&tree->source)});
   p->l = tree->tree.root->l;
   p = tft_rotateUpL(tree, p);
   tree->tree.root->l = p;
   tree->tree.root->s++;
   tree->tree.size++;
}

static void tft_insertFromHead(treap_fingertree_t *tree, u64 x, u64 i) {
   tree_persist(&tree->tree.root);
   tree_persist(&tree->tree.root->l);
   tree->tree.root->s++;
   tree->tree.size++;

   node_t *n = tree_allocate((node_t){.x = x, .y = xoshiro256_uint64(&tree->source)});
   node_t *p = tree->tree.root->l;
   for (;;) {
      if (i > p->s) {
         tree_persist(&p->l);
         i = i - p->s - 1;
         p = p->l;
         continue;
      }
      if (tft_rank(n) > tft_rank(p)) {
         tree_split_internal(p->r, i, &p->r, &n->r);
         n->s = p->s - i;
         p->s = i;
         n->l = p->l;
         p->l = tft_rotateUpL(tree, n);
         return;
      }
      p->s++;
      tft_treap_insert(&p->r, i, n);
      return;
   }
}

static void tft_insertFromTail(treap_fingertree_t *tree, u64 x, u64 i) {
   tree_persist(&tree->tree.root);
   tree_persist(&tree->tree.root->r);
   tree->tree.size++;

   node_t *n = tree_allocate((node_t){.x = x, .y = xoshiro256_uint64(&tree->source)});
   node_t *p = tree->tree.root->r;
   for (;;) {
      if (i > p->s) {
         tree_persist(&p->r);
         i = i - p->s - 1;
         p = p->r;
         continue;
      }
      if (tft_rank(n) > tft_rank(p)) {
         tree_split_internal(p->l, p->s - i, &n->l, &p->l);
         n->s = p->s - i;
         p->s = i;
         n->r = p->r;
         p->r = tft_rotateUpR(tree, n);
         return;
      }
      p->s++;
      tft_treap_insert(&p->l, p->s - i - 1, n);
      return;
   }
}

static void tft_insert(treap_fingertree_t *tree, u64 i, u64 x) {
   assert_bst(i <= tree->tree.size);
   if (tree->tree.root == NULL) {
      tree->tree.root = tree_allocate((node_t){.x = x, .y = xoshiro256_uint64(&tree->source)});
      tree->tree.size = 1;
      return;
   }
   if (i <= tree->tree.root->s) {
      if (i == 0) tft_insertAsFirst(tree, x);
      else tft_insertFromHead(tree, x, i - 1);
   } else {
      if (i == tree->tree.size) tft_insertAsLast(tree, x);
      else tft_insertFromTail(tree, x, tree->tree.size - i - 1);
   }
}


// ─────────────────────────────────────────────────────────────────────────────
// Delete operations
// ─────────────────────────────────────────────────────────────────────────────

static void tft_deleteFirst(treap_fingertree_t *tree, u64 *x) {
   node_t *old = tree->tree.root->l;
   *x = old->x;
   tree_persist(&tree->tree.root);
   tree_share(old->l);
   tree_share(old->r);
   tree->tree.root->l = tft_reverseL(old->r, old->l, old->s);
   tree->tree.root->s--;
   tree_release(old);
}

static void tft_deleteLast(treap_fingertree_t *tree, u64 *x) {
   node_t *old = tree->tree.root->r;
   *x = old->x;
   tree_persist(&tree->tree.root);
   tree_share(old->l);
   tree_share(old->r);
   tree->tree.root->r = tft_reverseR(old->l, old->r);
   tree_release(old);
}

static void tft_deleteFromHead(treap_fingertree_t *tree, u64 i, u64 *x) {
   if (i == 0) { tft_deleteFirst(tree, x); return; }
   tree_persist(&tree->tree.root);
   tree_persist(&tree->tree.root->l);
   tree->tree.root->s--;
   node_t *p = tree->tree.root->l;
   while (i > p->s + 1) {
      tree_persist(&p->l);
      i = i - p->s - 1;
      p = p->l;
   }
   if (i < p->s + 1) {
      tft_treap_delete(&p->r, i - 1, x);
      p->s--;
      return;
   }
   node_t *g = p->l;
   tree_share(g->l);
   tree_share(g->r);
   *x = g->x;
   p->r = tft_treap_join(p->r, g->r, p->s);
   p->l = g->l;
   p->s = p->s + g->s;
   tft_rotateDownL(p);
   tree_release(g);
}

static void tft_deleteFromTail(treap_fingertree_t *tree, u64 i, u64 *x) {
   if (i == tree->tree.size - 1) { tft_deleteLast(tree, x); return; }
   i = tree->tree.size - i - 1;
   tree_persist(&tree->tree.root);
   tree_persist(&tree->tree.root->r);
   node_t *p = tree->tree.root->r;
   while (i > p->s + 1) {
      tree_persist(&p->r);
      i = i - p->s - 1;
      p = p->r;
   }
   if (i < p->s + 1) {
      tft_treap_delete(&p->l, p->s - i, x);
      p->s--;
      return;
   }
   node_t *g = p->r;
   tree_share(g->l);
   tree_share(g->r);
   *x = g->x;
   p->l = tft_treap_join(g->l, p->l, g->s);
   p->r = g->r;
   p->s = p->s + g->s;
   tft_rotateDownR(p);
   tree_release(g);
}

static void tft_deleteRoot(treap_fingertree_t *tree, u64 *x) {
   tree_persist(&tree->tree.root);
   node_t *old = tree->tree.root;
   *x = old->x;

   // Convert to treap
   old->l = tft_reverseL2(old->l, NULL);
   old->r = tft_reverseR(old->r, NULL);

   // Dissolve root
   tree_share(old->l);
   tree_share(old->r);
   tree->tree.root = tft_treap_join(old->l, old->r, old->s);
   tree_release(old);

   if (tree->tree.root == NULL) return;
   tree->tree.root->l = tft_reverseL(tree->tree.root->l, NULL, tree->tree.root->s);
   tree->tree.root->r = tft_reverseR(tree->tree.root->r, NULL);
}

static u64 tft_delete(treap_fingertree_t *tree, u64 i) {
   assert_bst(i < tree->tree.size);
   u64 x;
   if (i < tree->tree.root->s) {
      tft_deleteFromHead(tree, i, &x);
   } else if (i > tree->tree.root->s) {
      tft_deleteFromTail(tree, i, &x);
   } else {
      tft_deleteRoot(tree, &x);
   }
   tree->tree.size--;
   return x;
}


// ─────────────────────────────────────────────────────────────────────────────
// Select / Update
// ─────────────────────────────────────────────────────────────────────────────

static u64 tft_accessFromHead(treap_fingertree_t *tree, u64 i) {
   node_t *p = tree->tree.root->l;
   for (;;) {
      if (i == 0) return p->x;
      if (i > p->s) { i = i - p->s - 1; p = p->l; }
      else { return node_lookup(p->r, i - 1); }
   }
}

static u64 tft_accessFromTail(treap_fingertree_t *tree, u64 i) {
   node_t *p = tree->tree.root->r;
   for (;;) {
      if (i == 0) return p->x;
      if (i > p->s) { i = i - p->s - 1; p = p->r; }
      else { return node_lookup(p->l, p->s - i); }
   }
}

static u64 tft_select(treap_fingertree_t *tree, u64 i) {
   assert_bst(i < tree->tree.size);
   if (i < tree->tree.root->s) return tft_accessFromHead(tree, i);
   if (i > tree->tree.root->s) return tft_accessFromTail(tree, tree->tree.size - i - 1);
   return tree->tree.root->x;
}

static void tft_updateFromHead(treap_fingertree_t *tree, u64 x, u64 i) {
   tree_persist(&tree->tree.root);
   tree_persist(&tree->tree.root->l);
   node_t *p = tree->tree.root->l;
   for (;;) {
      if (i == 0) { p->x = x; return; }
      if (i > p->s) { tree_persist(&p->l); i = i - p->s - 1; p = p->l; }
      else { tree_persist(&p->r); node_update(p->r, i - 1, x); return; }
   }
}

static void tft_updateFromTail(treap_fingertree_t *tree, u64 x, u64 i) {
   tree_persist(&tree->tree.root);
   tree_persist(&tree->tree.root->r);
   node_t *p = tree->tree.root->r;
   for (;;) {
      if (i == 0) { p->x = x; return; }
      if (i > p->s) { tree_persist(&p->r); i = i - p->s - 1; p = p->r; }
      else { tree_persist(&p->l); node_update(p->l, p->s - i, x); return; }
   }
}

static void tft_update(treap_fingertree_t *tree, u64 i, u64 x) {
   assert_bst(i < tree->tree.size);
   if (i < tree->tree.root->s) tft_updateFromHead(tree, x, i);
   else if (i > tree->tree.root->s) tft_updateFromTail(tree, x, tree->tree.size - i - 1);
   else { tree_persist(&tree->tree.root); tree->tree.root->x = x; }
}


// ─────────────────────────────────────────────────────────────────────────────
// Split
// ─────────────────────────────────────────────────────────────────────────────

static void tft_splitFromHead(treap_fingertree_t *tree, u64 i, tree_t *out_l, tree_t *out_r) {
   assert_bst(i <= tree->tree.size);
   assert_bst(i <= tree->tree.root->s);

   tree_persist(&tree->tree.root);

   node_t *p = tree->tree.root;
   u64 d = i;
   while (d > (p->l->s + 1)) {
      d = d - (p->l->s + 1);
      tree_persist(&p->l);
      p = p->l;
   }
   tree_persist(&p->l);
   node_t *g = p->l;
   p->l = NULL;
   p = g;
   g = p->l;

   u64 sl = d - 1;
   u64 sr = p->s - sl;

   node_t *l, *r;
   tree_split_internal(p->r, sl, &l, &r);

   out_l->root = p;
   out_l->size = i;
   out_l->root->s = i - d;
   out_l->root->l = tree->tree.root->l;
   out_l->root->r = tft_reverseR(l, NULL);

   out_r->root = tree->tree.root;
   out_r->size = tree->tree.size - i;
   out_r->root->s = tree->tree.root->s - i;
   out_r->root->r = tree->tree.root->r;
   out_r->root->l = tft_reverseL(r, g, sr);
}

static void tft_splitFromTail(treap_fingertree_t *tree, u64 i, tree_t *out_l, tree_t *out_r) {
   assert_bst(i < tree->tree.size);
   assert_bst(i > tree->tree.root->s);

   tree_persist(&tree->tree.root);

   node_t *p = tree->tree.root;
   u64 d = tree->tree.size - i;

   while (d > (p->r->s + 1)) {
      d = d - (p->r->s + 1);
      tree_persist(&p->r);
      p = p->r;
   }
   tree_persist(&p->r);
   node_t *g = p->r;
   p->r = NULL;
   p = g;
   g = p->r;

   u64 sr = d - 1;
   u64 sl = p->s - sr;

   node_t *l, *r;
   tree_split_internal(p->l, sl, &l, &r);

   out_r->root = p;
   out_r->size = tree->tree.size - i;
   out_r->root->s = sr;
   out_r->root->l = tft_reverseL(r, NULL, sr);
   out_r->root->r = tree->tree.root->r;

   out_l->root = tree->tree.root;
   out_l->size = i;
   out_l->root->l = tree->tree.root->l;
   out_l->root->r = tft_reverseR(l, g);
}

static void tft_split(treap_fingertree_t *tree, u64 i, tree_t *out_l, tree_t *out_r) {
   assert_bst(i <= tree->tree.size);
   // In Go, split/splitFromHead/splitFromTail use value receivers, so they
   // get a copy of the struct. We must make a local copy to avoid modifying
   // the original tree when persist copies the root during the split.
   treap_fingertree_t local = *tree;
   tree_share(local.tree.root);
   if (i == 0) {
      *out_l = (tree_t){NULL, 0};
      *out_r = local.tree;
      return;
   }
   if (i == local.tree.size) {
      *out_l = local.tree;
      *out_r = (tree_t){NULL, 0};
      return;
   }
   if (i <= local.tree.root->s) {
      tft_splitFromHead(&local, i, out_l, out_r);
   } else {
      tft_splitFromTail(&local, i, out_l, out_r);
   }
}


// ─────────────────────────────────────────────────────────────────────────────
// Join (finger-tree join using joinUp)
// ─────────────────────────────────────────────────────────────────────────────

static node_t *tft_joinUp(treap_fingertree_t *tree_l, treap_fingertree_t *tree_r) {
   tree_persist(&tree_l->tree.root);
   tree_persist(&tree_r->tree.root);

   node_t *l = tree_l->tree.root->r;
   node_t *r = tree_r->tree.root->l;
   u64 s = 0;

   node_t *p = NULL;
   for (;;) {
      if (r == NULL) {
         tree_l->tree.root->r = tree_r->tree.root->r;
         tree_r->tree.root->l = p;
         tree_r->tree.root->r = l;
         tree_r->tree.root->s = s;

         tree_appendR(&tree_l->tree.root->r, tft_rotateUpR(tree_l, tree_r->tree.root));
         return tree_l->tree.root;
      }
      if (l == NULL) {
         tree_r->tree.root->l = tree_l->tree.root->l;
         tree_r->tree.root->s = tree_l->tree.size + tree_r->tree.root->s;
         tree_l->tree.root->l = r;
         tree_l->tree.root->r = p;
         tree_l->tree.root->s = s;

         tree_appendL(&tree_r->tree.root->l, tft_rotateUpL(tree_l, tree_l->tree.root));
         return tree_r->tree.root;
      }
      if (l->y < r->y) {
         tree_persist(&l);
         s = s + l->s + 1;
         node_t *g = l->r;
         l->r = p;
         p = l;
         l = g;
      } else {
         tree_persist(&r);
         s = s + r->s + 1;
         r->s = s - r->s - 1;
         node_t *g = r->l;
         r->l = p;
         p = r;
         r = g;
      }
   }
}


// ─────────────────────────────────────────────────────────────────────────────
// Traversal (finger-tree order: head spine → root → tail spine)
// ─────────────────────────────────────────────────────────────────────────────

static void tft_eachFromHead(node_t *p, void (*visit)(u64, void *), void *ctx) {
   if (p == NULL) return;
   visit(p->x, ctx);
   inorder(p->r, visit, ctx);
   tft_eachFromHead(p->l, visit, ctx);
}

static void tft_eachFromTail(node_t *p, void (*visit)(u64, void *), void *ctx) {
   if (p == NULL) return;
   tft_eachFromTail(p->r, visit, ctx);
   inorder(p->l, visit, ctx);
   visit(p->x, ctx);
}

static void tft_each(treap_fingertree_t *tree, void (*visit)(u64, void *), void *ctx) {
   if (tree->tree.root == NULL) return;
   tft_eachFromHead(tree->tree.root->l, visit, ctx);
   visit(tree->tree.root->x, ctx);
   tft_eachFromTail(tree->tree.root->r, visit, ctx);
}


// ─────────────────────────────────────────────────────────────────────────────
// Verification
// ─────────────────────────────────────────────────────────────────────────────

static void tft_verifyMaxRankHeap(node_t *p) {
   if (p == NULL) return;
   invariant(tft_rank(p) >= tft_rank(p->l));
   invariant(tft_rank(p) >= tft_rank(p->r));
   tft_verifyMaxRankHeap(p->l);
   tft_verifyMaxRankHeap(p->r);
}

static void tft_verify(treap_fingertree_t *tree) {
   invariant(tree->tree.size == node_size(tree->tree.root));
   if (tree->tree.root == NULL) return;

   // The root's size must be equal to the size of the left subtree.
   invariant(tree->tree.root->s == node_size(tree->tree.root->l));

   // Verify internal positions along the spines.
   for (node_t *l = tree->tree.root->l; l != NULL; l = l->l) {
      tree_verify_size(l->r, l->s);
   }
   for (node_t *r = tree->tree.root->r; r != NULL; r = r->r) {
      tree_verify_size(r->l, r->s);
   }

   // Verify ranks along spines.
   for (node_t *l = tree->tree.root->l; l != NULL; l = l->l) {
      tft_verifyMaxRankHeap(l->r);
      invariant(tft_rank(l) >= tft_rank(l->r));
      invariant(tft_rank(l) <= tft_rank(l->l) || l->l == NULL);
   }
   for (node_t *r = tree->tree.root->r; r != NULL; r = r->r) {
      tft_verifyMaxRankHeap(r->l);
      invariant(tft_rank(r) >= tft_rank(r->l));
      invariant(tft_rank(r) <= tft_rank(r->r) || r->r == NULL);
   }
}


// ─────────────────────────────────────────────────────────────────────────────
// List vtable
// ─────────────────────────────────────────────────────────────────────────────

static list_t tft_vt_new(const list_vtable_t *vt) {
   treap_fingertree_t *t = (treap_fingertree_t *)calloc(1, sizeof(treap_fingertree_t));
   xoshiro256_seed(&t->source, random_uint64());
   return (list_t){.impl = t, .vt = vt};
}

static list_t tft_vt_clone(list_t *self) {
   treap_fingertree_t *orig = (treap_fingertree_t *)self->impl;
   treap_fingertree_t *copy = (treap_fingertree_t *)malloc(sizeof(treap_fingertree_t));
   tree_share(orig->tree.root);
   *copy = *orig;
   return (list_t){.impl = copy, .vt = self->vt};
}

static void tft_vt_free(list_t *self) {
   treap_fingertree_t *t = (treap_fingertree_t *)self->impl;
   free_nodes(t->tree.root);
   free(t);
   self->impl = NULL;
}

static u64 tft_vt_size(list_t *self) { return ((treap_fingertree_t *)self->impl)->tree.size; }
static void tft_vt_each(list_t *self, void (*v)(u64, void *), void *c) { tft_each((treap_fingertree_t *)self->impl, v, c); }
static void tft_vt_verify(list_t *self) { tft_verify((treap_fingertree_t *)self->impl); }
static u64  tft_vt_select(list_t *self, u64 i) { return tft_select((treap_fingertree_t *)self->impl, i); }
static void tft_vt_update(list_t *self, u64 i, u64 x) { tft_update((treap_fingertree_t *)self->impl, i, x); }
static void tft_vt_insert(list_t *self, u64 i, u64 x) { tft_insert((treap_fingertree_t *)self->impl, i, x); }
static u64  tft_vt_delete(list_t *self, u64 i) { return tft_delete((treap_fingertree_t *)self->impl, i); }

static void tft_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   treap_fingertree_t *t = (treap_fingertree_t *)self->impl;
   tree_t lt, rt;
   tft_split(t, i, &lt, &rt);
   treap_fingertree_t *sl = (treap_fingertree_t *)calloc(1, sizeof(treap_fingertree_t));
   sl->tree = lt; sl->source = t->source;
   treap_fingertree_t *sr = (treap_fingertree_t *)calloc(1, sizeof(treap_fingertree_t));
   sr->tree = rt; sr->source = t->source;
   *out_l = (list_t){.impl = sl, .vt = self->vt};
   *out_r = (list_t){.impl = sr, .vt = self->vt};
}

static list_t tft_vt_join(list_t *self, list_t *other) {
   treap_fingertree_t *t = (treap_fingertree_t *)self->impl;
   treap_fingertree_t *that = (treap_fingertree_t *)other->impl;

   if (t->tree.size == 0) return tft_vt_clone(other);
   if (that->tree.size == 0) return tft_vt_clone(self);

   // Clone both (joinUp mutates both trees)
   list_t lc = tft_vt_clone(self);
   list_t rc = tft_vt_clone(other);
   treap_fingertree_t *lft = (treap_fingertree_t *)lc.impl;
   treap_fingertree_t *rft = (treap_fingertree_t *)rc.impl;

   treap_fingertree_t *result = (treap_fingertree_t *)calloc(1, sizeof(treap_fingertree_t));
   result->tree.root = tft_joinUp(lft, rft);
   result->tree.size = lft->tree.size + rft->tree.size;
   result->source = t->source;

   free(lft);
   free(rft);

   return (list_t){.impl = result, .vt = self->vt};
}

const list_vtable_t treap_fingertree_vtable = {
   .name      = "TreapFingerTree",
   .new_fn    = tft_vt_new,
   .clone_fn  = tft_vt_clone,
   .free_fn   = tft_vt_free,
   .size_fn   = tft_vt_size,
   .each_fn   = tft_vt_each,
   .verify_fn = tft_vt_verify,
   .select_fn = tft_vt_select,
   .update_fn = tft_vt_update,
   .insert_fn = tft_vt_insert,
   .delete_fn = tft_vt_delete,
   .split_fn  = tft_vt_split,
   .join_fn   = tft_vt_join,
};
