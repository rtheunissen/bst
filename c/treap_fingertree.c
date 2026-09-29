// TreapFingerTree — the most complex tree implementation.
//
// A finger tree where each finger is a treap. The root node's `l` points to
// the "head" (left spine) and `r` to the "tail" (right spine). Nodes on the
// left spine have their `l` pointing deeper into the spine and `r` pointing
// to a treap subtree. The right spine is symmetric.
//
#include "treap_fingertree.h"
#include "rank.h"
#include "tree_list_adapter.h"

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static u64 tft_rank(node_t *p) {
   if (p == NULL) {
      return 0;
   }
   return p->y;
}

static node_t *tft_allocate_node(treap_fingertree_t *tree, u64 x) {
   invariant(tree != NULL);
   return tree_allocate((node_t){
       .x = x,
       .y = xoshiro256_uint64(&tree->source),
   });
}

// Number of nodes represented by one spine node and its attached treap.
static u64 tft_spine_span(node_t *p) {
   invariant(p != NULL);
   invariant(p->s < UINT64_MAX);
   return p->s + 1;
}

// Local rank inside a head-spine node's right treap.
static u64 tft_head_attached_rank(u64 head_distance) {
   invariant(head_distance > 0);
   return rank_of_last(head_distance);
}

// Local rank inside a tail-spine node's left treap.
static u64 tft_tail_attached_rank(node_t *p, u64 tail_distance) {
   invariant(p != NULL);
   invariant(tail_distance > 0);
   invariant(tail_distance < tft_spine_span(p));
   return rank_from_end(tft_spine_span(p), tail_distance);
}

// Node-level lookup (doesn't need tree_t)
static u64 node_lookup(node_t *p, u64 i) {
   invariant(p != NULL);
   for (;;) {
      invariant(p != NULL);
      if (i == p->s) {
         return p->x;
      }
      if (i < p->s) {
         invariant(p->l != NULL);
         p = p->l;
      } else {
         invariant(p->r != NULL);
         i = rank_in_right_subtree(i, p->s);
         p = p->r;
      }
   }
}

// Node-level update with persistence
static void node_update(node_t *p, u64 i, u64 x) {
   invariant(p != NULL);
   invariant(!tree_shared(p));
   for (;;) {
      invariant(p != NULL);
      if (i == p->s) {
         p->x = x;
         return;
      }
      if (i < p->s) {
         invariant(p->l != NULL);
         tree_persist_unshared(&p->l);
         p = p->l;
      } else {
         invariant(p->r != NULL);
         tree_persist_unshared(&p->r);
         i = rank_in_right_subtree(i, p->s);
         p = p->r;
      }
   }
}

// ─────────────────────────────────────────────────────────────────────────────
// Reverse operations (convert between finger-tree and treap spine formats)
// ─────────────────────────────────────────────────────────────────────────────

static node_t *tft_reverseL(node_t *p, node_t *g, u64 s) {
   invariant(p != NULL || s == 0);
   tree_debug_require_size(p, s);
   for (;;) {
      if (p == NULL) {
         return g;
      }
      invariant(s > 0);
      invariant(p->s < s);
      tree_persist_unshared(&p);
      u64 sl = p->s;
      p->s = sizeR(p, s);
      node_t *l = p->l;
      p->l = g;
      g = p;
      p = l;
      s = sl;
   }
}

static node_t *tft_reverseR(node_t *p, node_t *g) {
   for (;;) {
      if (p == NULL) {
         return g;
      }
      tree_persist_unshared(&p);
      node_t *r = p->r;
      p->r = g;
      g = p;
      p = r;
   }
}

static node_t *tft_reverseL2(node_t *p, node_t *g) {
   u64 s = 0;
   for (;;) {
      if (p == NULL) {
         return g;
      }
      tree_persist_unshared(&p);
      u64 prefix_size = s;
      s = rank_after_predecessor(prefix_size, p);
      p->s = prefix_size;
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
   invariant(p != NULL);
   invariant(p->r != NULL);
   invariant(!tree_shared(p));
   tree_persist_unshared(&p->r);
   node_t *r = p->r;
   p->r = r->r;
   r->r = p->l;
   p->l = r;
   p->s = rank_after_predecessor(p->s, r);
}

static void tft_rotateParentRightOnLeftSpine(node_t *p) {
   invariant(p != NULL);
   invariant(p->l != NULL);
   invariant(!tree_shared(p));
   tree_persist_unshared(&p->l);
   node_t *l = p->l;
   u64 root_left_size = p->s;
   p->l = l->l;
   l->l = p->r;
   p->r = l;
   p->s = rank_after_predecessor(root_left_size, l);
   l->s = root_left_size;
}

static void tft_rotateRightIntoRoot(treap_fingertree_t *tree, node_t *l) {
   invariant(tree != NULL);
   invariant(tree->tree.root != NULL);
   invariant(l != NULL);
   invariant(l->l == NULL);
   node_t *p = tree->tree.root;
   invariant(!tree_shared(p));
   invariant(!tree_shared(l));
   u64 root_left_size = p->s;
   u64 promoted_left_size = l->s;
   tree_appendR_raw(&p->r, p);
   l->l = p->l;
   p->l = l->r;
   l->r = p->r;
   p->r = NULL;
   l->s = rank_in_right_subtree(root_left_size, promoted_left_size);
   p->s = promoted_left_size;
   tree->tree.root = l;
}

static void tft_rotateLeftIntoRoot(treap_fingertree_t *tree, node_t *r) {
   invariant(tree != NULL);
   invariant(tree->tree.root != NULL);
   invariant(r != NULL);
   invariant(r->r == NULL);
   node_t *p = tree->tree.root;
   invariant(!tree_shared(p));
   invariant(!tree_shared(r));
   u64 root_left_size = p->s;
   u64 promoted_left_size = r->s;
   tree_appendL_raw(&p->l, p);
   r->r = p->r;
   p->r = r->l;
   r->l = p->l;
   p->l = NULL;
   r->s = rank_after_predecessor(root_left_size, r);
   p->s = promoted_left_size;
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
   invariant(p != NULL);
   invariant(!tree_shared(p));
   while (p->r != NULL && p->r->y > p->y) {
      tree_persist_unshared(&p->r);
      node_t *r = p->r;
      u64 root_left_size = p->s;
      u64 promoted_left_size = r->s;
      p->r = r->l;
      r->l = p->l;
      p->l = r;
      r->s = rank_in_right_subtree(root_left_size, promoted_left_size);
      p->s = promoted_left_size;
   }
}

static void tft_rotateDownR(node_t *p) {
   invariant(p != NULL);
   invariant(!tree_shared(p));
   while (p->l != NULL && p->l->y > p->y) {
      tree_persist_unshared(&p->l);
      node_t *l = p->l;
      p->l = l->r;
      l->r = p->r;
      p->r = l;
      p->s = rank_in_right_subtree(p->s, l->s);
   }
}

// ─────────────────────────────────────────────────────────────────────────────
// Treap join (not finger-tree join — joins two treaps by priority)
// ─────────────────────────────────────────────────────────────────────────────

static node_t *tft_treap_join(node_t *l, node_t *r, u64 sl, u64 sr) {
   tree_join_inputs_require(l, r, sl, sr);
#ifdef DEBUG
   tree_debug_require_join_inputs(l, r, sl, sr);
#endif
   node_t *root = NULL;
   node_t **p = &root;
   for (;;) {
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
      if (l->y >= r->y) {
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

// ─────────────────────────────────────────────────────────────────────────────
// Internal treap insert (used when inserting into interior subtrees)
// ─────────────────────────────────────────────────────────────────────────────

static void tft_treap_insert(node_t **p, u64 size, u64 i, node_t *n) {
   invariant(p != NULL);
   invariant(n != NULL);
   invariant(n->l == NULL);
   invariant(n->r == NULL);
   invariant(n->s == 0);
   (void)tree_insert_inputs_require(*p, i, size);
   for (;;) {
      invariant(p != NULL);
      if (*p == NULL) {
         invariant(i == 0);
         *p = n;
         return;
      }
      if ((*p)->y <= n->y) {
         tree_split_result_t split = tree_split_roots_sized(*p, i, size);
         n->l = split.left;
         n->r = split.right;
         n->s = i;
         *p = n;
         return;
      }
      tree_persist_unshared(p);
      node_t *node = *p;
      invariant(node != NULL);
      if (i <= node->s) {
         size = sizeL(node);
         p = insertL(node);
      } else {
         size = sizeR(node, size);
         p = insertR(node, &i);
      }
   }
}

// Internal treap delete
static u64 tft_treap_delete(node_t **p, u64 size, u64 i) {
   invariant(p != NULL);
   (void)tree_delete_inputs_require(*p, i, size);
   for (;;) {
      invariant(*p != NULL);
      if (i == (*p)->s) {
         node_t *old = *p;
         tree_share(old->l);
         tree_share(old->r);
         u64 x = old->x;
         *p = tft_treap_join(old->l, old->r, old->s, sizeR(old, size));
         tree_release(old);
         return x;
      }
      tree_persist_unshared(p);
      node_t *node = *p;
      invariant(node != NULL);
      if (i < node->s) {
         invariant(node->l != NULL);
         size = sizeL(node);
         p = deleteL(node);
      } else {
         invariant(node->r != NULL);
         size = sizeR(node, size);
         p = deleteR(node, &i);
      }
   }
}

// ─────────────────────────────────────────────────────────────────────────────
// Insert operations
// ─────────────────────────────────────────────────────────────────────────────

static void tft_insertAsLast(treap_fingertree_t *tree, u64 x) {
   tree_persist_unshared(&tree->tree.root);
   node_t *p = tft_allocate_node(tree, x);
   p->r = tree->tree.root->r;
   p = tft_rotateUpR(tree, p);
   tree->tree.root->r = p;
   tree_list_sized_inserted(&tree->tree);
}

static void tft_insertAsFirst(treap_fingertree_t *tree, u64 x) {
   tree_persist_unshared(&tree->tree.root);
   invariant(tree->tree.root->s < UINT64_MAX);
   tree->tree.root->s = size_after_inserting_one(tree->tree.root->s);
   node_t *p = tft_allocate_node(tree, x);
   p->l = tree->tree.root->l;
   p = tft_rotateUpL(tree, p);
   tree->tree.root->l = p;
   tree_list_sized_inserted(&tree->tree);
}

static void tft_insertFromHead(treap_fingertree_t *tree, u64 x, u64 i) {
   tree_persist_unshared(&tree->tree.root);
   tree_persist_unshared(&tree->tree.root->l);
   invariant(tree->tree.root->s < UINT64_MAX);
   tree->tree.root->s = size_after_inserting_one(tree->tree.root->s);
   tree_list_sized_inserted(&tree->tree);

   node_t *n = tft_allocate_node(tree, x);
   node_t *p = tree->tree.root->l;
   for (;;) {
      if (i > p->s) {
         tree_persist_unshared(&p->l);
         i = rank_in_right_subtree(i, p->s);
         p = p->l;
         continue;
      }
      if (tft_rank(n) > tft_rank(p)) {
         tree_split_result_t split = tree_split_roots_sized(p->r, i, p->s);
         p->r = split.left;
         n->r = split.right;
         n->s = suffix_size_after_prefix(p->s, i);
         p->s = i;
         n->l = p->l;
         p->l = tft_rotateUpL(tree, n);
         return;
      }
      invariant(p->s < UINT64_MAX);
      u64 attached_size = p->s;
      p->s = size_after_inserting_one(p->s);
      tft_treap_insert(&p->r, attached_size, i, n);
      return;
   }
}

static void tft_insertFromTail(treap_fingertree_t *tree, u64 x, u64 i) {
   tree_persist_unshared(&tree->tree.root);
   tree_persist_unshared(&tree->tree.root->r);
   tree_list_sized_inserted(&tree->tree);

   node_t *n = tft_allocate_node(tree, x);
   node_t *p = tree->tree.root->r;
   for (;;) {
      if (i > p->s) {
         tree_persist_unshared(&p->r);
         i = rank_in_right_subtree(i, p->s);
         p = p->r;
         continue;
      }
      if (tft_rank(n) > tft_rank(p)) {
         u64 split_size = suffix_size_after_prefix(p->s, i);
         tree_split_result_t split =
             tree_split_roots_sized(p->l, split_size, p->s);
         n->l = split.left;
         p->l = split.right;
         n->s = split_size;
         p->s = i;
         n->r = p->r;
         p->r = tft_rotateUpR(tree, n);
         return;
      }
      invariant(p->s < UINT64_MAX);
      u64 attached_size = p->s;
      u64 insert_rank =
          rank_from_end(size_after_inserting_one(attached_size), i);
      p->s = size_after_inserting_one(p->s);
      tft_treap_insert(&p->l, attached_size, insert_rank, n);
      return;
   }
}

static void tft_insert(treap_fingertree_t *tree, u64 i, u64 x) {
   invariant(tree != NULL);
   u64 size = tree->tree.size;
   u64 inserted_size = tree_insert_inputs_require(tree->tree.root, i, size);
   if (tree->tree.root == NULL) {
      tree->tree.root = tft_allocate_node(tree, x);
      tree_list_sized_inserted(&tree->tree);
      invariant(tree->tree.size == inserted_size);
      return;
   }
   if (i <= tree->tree.root->s) {
      if (i == 0) {
         tft_insertAsFirst(tree, x);
      } else {
         tft_insertFromHead(tree, x, i - 1);
      }
   } else {
      if (i == tree->tree.size) {
         tft_insertAsLast(tree, x);
      } else {
         tft_insertFromTail(tree, x, rank_from_end(tree->tree.size, i));
      }
   }
   invariant(tree->tree.size == inserted_size);
}

// ─────────────────────────────────────────────────────────────────────────────
// Delete operations
// ─────────────────────────────────────────────────────────────────────────────

static u64 tft_deleteFirst(treap_fingertree_t *tree) {
   invariant(tree->tree.root->l != NULL);
   invariant(tree->tree.root->s > 0);
   node_t *old = tree->tree.root->l;
   u64 x = old->x;
   tree_persist_unshared(&tree->tree.root);
   tree_share(old->l);
   tree_share(old->r);
   tree->tree.root->l = tft_reverseL(old->r, old->l, old->s);
   tree->tree.root->s = size_after_removing_one(tree->tree.root->s);
   tree_release(old);
   return x;
}

static u64 tft_deleteLast(treap_fingertree_t *tree) {
   invariant(tree->tree.root->r != NULL);
   node_t *old = tree->tree.root->r;
   u64 x = old->x;
   tree_persist_unshared(&tree->tree.root);
   tree_share(old->l);
   tree_share(old->r);
   tree->tree.root->r = tft_reverseR(old->l, old->r);
   tree_release(old);
   return x;
}

static u64 tft_deleteFromHead(treap_fingertree_t *tree, u64 i) {
   if (i == 0) {
      return tft_deleteFirst(tree);
   }
   tree_persist_unshared(&tree->tree.root);
   tree_persist_unshared(&tree->tree.root->l);
   invariant(tree->tree.root->s > 0);
   tree->tree.root->s = size_after_removing_one(tree->tree.root->s);
   node_t *p = tree->tree.root->l;
   for (;;) {
      invariant(p != NULL);
      invariant(p->s < UINT64_MAX);
      u64 span = tft_spine_span(p);
      if (i <= span) {
         break;
      }
      invariant(p->l != NULL);
      tree_persist_unshared(&p->l);
      i = rank_in_right_subtree(i, p->s);
      p = p->l;
   }
   u64 span = tft_spine_span(p);
   if (i < span) {
      invariant(i > 0);
      invariant(p->s > 0);
      invariant(p->r != NULL);
      u64 x = tft_treap_delete(&p->r, p->s, tft_head_attached_rank(i));
      p->s = size_after_removing_one(p->s);
      return x;
   }
   invariant(p->l != NULL);
   node_t *g = p->l;
   tree_share(g->l);
   tree_share(g->r);
   u64 x = g->x;
   p->r = tft_treap_join(p->r, g->r, p->s, g->s);
   p->l = g->l;
   p->s = rank_after_prefix(p->s, g);
   tft_rotateDownL(p);
   tree_release(g);
   return x;
}

static u64 tft_deleteFromTail(treap_fingertree_t *tree, u64 i) {
   if (i == tree->tree.size - 1) {
      return tft_deleteLast(tree);
   }
   i = rank_from_end(tree->tree.size, i);
   tree_persist_unshared(&tree->tree.root);
   tree_persist_unshared(&tree->tree.root->r);
   node_t *p = tree->tree.root->r;
   for (;;) {
      invariant(p != NULL);
      invariant(p->s < UINT64_MAX);
      u64 span = tft_spine_span(p);
      if (i <= span) {
         break;
      }
      invariant(p->r != NULL);
      tree_persist_unshared(&p->r);
      i = rank_in_right_subtree(i, p->s);
      p = p->r;
   }
   u64 span = tft_spine_span(p);
   if (i < span) {
      invariant(i > 0);
      invariant(p->s > 0);
      invariant(p->l != NULL);
      u64 x = tft_treap_delete(&p->l, p->s, tft_tail_attached_rank(p, i));
      p->s = size_after_removing_one(p->s);
      return x;
   }
   invariant(p->r != NULL);
   node_t *g = p->r;
   tree_share(g->l);
   tree_share(g->r);
   u64 x = g->x;
   p->l = tft_treap_join(g->l, p->l, g->s, p->s);
   p->r = g->r;
   p->s = rank_after_prefix(p->s, g);
   tft_rotateDownR(p);
   tree_release(g);
   return x;
}

static u64 tft_deleteRoot(treap_fingertree_t *tree) {
   tree_persist_unshared(&tree->tree.root);
   node_t *old = tree->tree.root;
   u64 x = old->x;

   // Convert to treap
   old->l = tft_reverseL2(old->l, NULL);
   old->r = tft_reverseR(old->r, NULL);

   // Dissolve root
   tree_share(old->l);
   tree_share(old->r);
   tree->tree.root =
       tft_treap_join(old->l, old->r, old->s, sizeR(old, tree->tree.size));
   tree_release(old);

   if (tree->tree.root == NULL) {
      return x;
   }
   tree->tree.root->l =
       tft_reverseL(tree->tree.root->l, NULL, tree->tree.root->s);
   tree->tree.root->r = tft_reverseR(tree->tree.root->r, NULL);
   return x;
}

static u64 tft_delete(treap_fingertree_t *tree, u64 i) {
   invariant(tree != NULL);
   u64 size = tree->tree.size;
   u64 deleted_size = tree_delete_inputs_require(tree->tree.root, i, size);
   u64 x;
   if (i < tree->tree.root->s) {
      x = tft_deleteFromHead(tree, i);
   } else if (i > tree->tree.root->s) {
      x = tft_deleteFromTail(tree, i);
   } else {
      x = tft_deleteRoot(tree);
   }
   tree_list_sized_deleted(&tree->tree);
   invariant(tree->tree.size == deleted_size);
   return x;
}

// ─────────────────────────────────────────────────────────────────────────────
// Select / Update
// ─────────────────────────────────────────────────────────────────────────────

static u64 tft_accessFromHead(treap_fingertree_t *tree, u64 i) {
   node_t *p = tree->tree.root->l;
   for (;;) {
      invariant(p != NULL);
      if (i == 0) {
         return p->x;
      }
      if (i > p->s) {
         invariant(p->l != NULL);
         i = rank_in_right_subtree(i, p->s);
         p = p->l;
      } else {
         invariant(p->r != NULL);
         return node_lookup(p->r, tft_head_attached_rank(i));
      }
   }
}

static u64 tft_accessFromTail(treap_fingertree_t *tree, u64 i) {
   node_t *p = tree->tree.root->r;
   for (;;) {
      invariant(p != NULL);
      if (i == 0) {
         return p->x;
      }
      if (i > p->s) {
         invariant(p->r != NULL);
         i = rank_in_right_subtree(i, p->s);
         p = p->r;
      } else {
         invariant(p->l != NULL);
         return node_lookup(p->l, tft_tail_attached_rank(p, i));
      }
   }
}

static u64 tft_select(treap_fingertree_t *tree, u64 i) {
   invariant(tree != NULL);
   tree_rank_inputs_require(tree->tree.root, i, tree->tree.size);
   if (i < tree->tree.root->s) {
      return tft_accessFromHead(tree, i);
   }
   if (i > tree->tree.root->s) {
      return tft_accessFromTail(tree, rank_from_end(tree->tree.size, i));
   }
   return tree->tree.root->x;
}

static void tft_updateFromHead(treap_fingertree_t *tree, u64 x, u64 i) {
   tree_persist_unshared(&tree->tree.root);
   tree_persist_unshared(&tree->tree.root->l);
   node_t *p = tree->tree.root->l;
   for (;;) {
      invariant(p != NULL);
      if (i == 0) {
         p->x = x;
         return;
      }
      if (i > p->s) {
         tree_persist_unshared(&p->l);
         i = rank_in_right_subtree(i, p->s);
         p = p->l;
      } else {
         tree_persist_unshared(&p->r);
         node_update(p->r, tft_head_attached_rank(i), x);
         return;
      }
   }
}

static void tft_updateFromTail(treap_fingertree_t *tree, u64 x, u64 i) {
   tree_persist_unshared(&tree->tree.root);
   tree_persist_unshared(&tree->tree.root->r);
   node_t *p = tree->tree.root->r;
   for (;;) {
      invariant(p != NULL);
      if (i == 0) {
         p->x = x;
         return;
      }
      if (i > p->s) {
         tree_persist_unshared(&p->r);
         i = rank_in_right_subtree(i, p->s);
         p = p->r;
      } else {
         tree_persist_unshared(&p->l);
         node_update(p->l, tft_tail_attached_rank(p, i), x);
         return;
      }
   }
}

static void tft_update(treap_fingertree_t *tree, u64 i, u64 x) {
   invariant(tree != NULL);
   tree_rank_inputs_require(tree->tree.root, i, tree->tree.size);
   u64 size = tree->tree.size;
   if (i < tree->tree.root->s) {
      tft_updateFromHead(tree, x, i);
   } else if (i > tree->tree.root->s) {
      tft_updateFromTail(tree, x, rank_from_end(tree->tree.size, i));
   } else {
      tree_persist_unshared(&tree->tree.root);
      tree->tree.root->x = x;
   }
   invariant(tree->tree.size == size);
   tree_list_require_sized_tree(&tree->tree);
}

// ─────────────────────────────────────────────────────────────────────────────
// Split
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   tree_t left;
   tree_t right;
} tft_split_result_t;

static tft_split_result_t tft_splitFromHead(treap_fingertree_t *tree, u64 i) {
   invariant(tree != NULL);
   u64 right_size = sequence_split_inputs_require(i, tree->tree.size);
   invariant(i <= tree->tree.root->s);

   tree_persist_unshared(&tree->tree.root);

   node_t *p = tree->tree.root;
   u64 d = i;

   for (;;) {
      invariant(p != NULL);
      invariant(p->l != NULL);
      invariant(p->l->s < UINT64_MAX);
      if (d <= tft_spine_span(p->l)) {
         break;
      }
      d = rank_in_right_subtree(d, p->l->s);
      tree_persist_unshared(&p->l);
      p = p->l;
   }
   tree_persist_unshared(&p->l);
   node_t *g = p->l;
   p->l = NULL;
   p = g;
   g = p->l;

   invariant(d > 0);
   invariant(p->s < UINT64_MAX);
   invariant(d <= tft_spine_span(p));
   u64 sl = rank_of_last(d);
   u64 sr = sequence_split_inputs_require(sl, p->s);

   tree_split_result_t split = tree_split_roots_sized(p->r, sl, p->s);

   tree_t left = {.root = p, .size = i};
   invariant(d <= i);
   left.root->s = i - d;
   left.root->l = tree->tree.root->l;
   left.root->r = tft_reverseR(split.left, NULL);

   tree_t right = {
       .root = tree->tree.root,
       .size = right_size,
   };
   right.root->s = sequence_split_inputs_require(i, tree->tree.root->s);
   right.root->r = tree->tree.root->r;
   right.root->l = tft_reverseL(split.right, g, sr);

   return (tft_split_result_t){.left = left, .right = right};
}

static tft_split_result_t tft_splitFromTail(treap_fingertree_t *tree, u64 i) {
   invariant(tree != NULL);
   invariant(i < tree->tree.size);
   invariant(i > tree->tree.root->s);
   u64 right_size = sequence_split_inputs_require(i, tree->tree.size);

   tree_persist_unshared(&tree->tree.root);

   node_t *p = tree->tree.root;
   u64 d = right_size;

   for (;;) {
      invariant(p != NULL);
      invariant(p->r != NULL);
      invariant(p->r->s < UINT64_MAX);
      if (d <= tft_spine_span(p->r)) {
         break;
      }
      d = rank_in_right_subtree(d, p->r->s);
      tree_persist_unshared(&p->r);
      p = p->r;
   }
   tree_persist_unshared(&p->r);
   node_t *g = p->r;
   p->r = NULL;
   p = g;
   g = p->r;

   invariant(d > 0);
   invariant(p->s < UINT64_MAX);
   invariant(d <= tft_spine_span(p));
   u64 sr = rank_of_last(d);
   u64 sl = sequence_split_inputs_require(sr, p->s);

   tree_split_result_t split = tree_split_roots_sized(p->l, sl, p->s);

   tree_t right = {
       .root = p,
       .size = right_size,
   };
   right.root->s = sr;
   right.root->l = tft_reverseL(split.right, NULL, sr);
   right.root->r = tree->tree.root->r;

   tree_t left = {.root = tree->tree.root, .size = i};
   left.root->l = tree->tree.root->l;
   left.root->r = tft_reverseR(split.left, g);

   return (tft_split_result_t){.left = left, .right = right};
}

static tft_split_result_t tft_split(treap_fingertree_t *tree, u64 i) {
   invariant(tree != NULL);
   u64 right_size =
       tree_split_inputs_require(tree->tree.root, i, tree->tree.size);
   // In Go, split/splitFromHead/splitFromTail use value receivers, so they
   // get a copy of the struct. We must make a local copy to avoid modifying
   // the original tree when persist copies the root during the split.
   treap_fingertree_t local = *tree;
   tree_share(local.tree.root);
   if (i == 0) {
      invariant(right_size == local.tree.size);
      return (tft_split_result_t){.left = {NULL, 0}, .right = local.tree};
   }
   if (i == local.tree.size) {
      invariant(right_size == 0);
      return (tft_split_result_t){.left = local.tree, .right = {NULL, 0}};
   }
   if (i <= local.tree.root->s) {
      return tft_splitFromHead(&local, i);
   }
   return tft_splitFromTail(&local, i);
}

// ─────────────────────────────────────────────────────────────────────────────
// Join (finger-tree join using joinUp)
// ─────────────────────────────────────────────────────────────────────────────

static node_t *tft_joinUp(treap_fingertree_t *tree_l,
                          treap_fingertree_t *tree_r) {
   invariant(tree_l != NULL);
   invariant(tree_r != NULL);
   invariant(tree_l->tree.root != NULL);
   invariant(tree_r->tree.root != NULL);
   tree_persist_unshared(&tree_l->tree.root);
   tree_persist_unshared(&tree_r->tree.root);

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

         tree_appendR_raw(&tree_l->tree.root->r,
                          tft_rotateUpR(tree_l, tree_r->tree.root));
         return tree_l->tree.root;
      }
      if (l == NULL) {
         tree_r->tree.root->l = tree_l->tree.root->l;
         tree_r->tree.root->s =
             rank_after_prefix(tree_l->tree.size, tree_r->tree.root);
         tree_l->tree.root->l = r;
         tree_l->tree.root->r = p;
         tree_l->tree.root->s = s;

         tree_appendL_raw(&tree_r->tree.root->l,
                          tft_rotateUpL(tree_r, tree_l->tree.root));
         return tree_r->tree.root;
      }
      if (l->y < r->y) {
         tree_persist_unshared(&l);
         s = rank_after_predecessor(s, l);
         node_t *g = l->r;
         l->r = p;
         p = l;
         l = g;
      } else {
         tree_persist_unshared(&r);
         u64 prefix_size = s;
         s = rank_after_predecessor(prefix_size, r);
         r->s = prefix_size;
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
   invariant(visit != NULL);
   for (; p != NULL; p = p->l) {
      visit(p->x, ctx);
      inorder(p->r, visit, ctx);
   }
}

static void tft_eachFromTail(node_t *p, void (*visit)(u64, void *), void *ctx) {
   invariant(visit != NULL);
   node_t *inline_data[NODE_STACK_INLINE_CAP];
   node_stack_t stack = {
       .data = inline_data,
       .len = 0,
       .cap = NODE_STACK_INLINE_CAP,
   };

   for (; p != NULL; p = p->r) {
      node_stack_push(&stack, inline_data, p);
   }
   while (stack.len > 0) {
      node_t *node = node_stack_pop(&stack);
      inorder(node->l, visit, ctx);
      visit(node->x, ctx);
   }

   node_stack_free(&stack, inline_data);
}

static void tft_each(treap_fingertree_t *tree, void (*visit)(u64, void *),
                     void *ctx) {
   invariant(tree != NULL);
   invariant(visit != NULL);
   tree_list_require_sized_tree(&tree->tree);
   if (tree->tree.root == NULL) {
      return;
   }
   tft_eachFromHead(tree->tree.root->l, visit, ctx);
   visit(tree->tree.root->x, ctx);
   tft_eachFromTail(tree->tree.root->r, visit, ctx);
}

// ─────────────────────────────────────────────────────────────────────────────
// Verification
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   node_t *node;
   u64 size;
   u64 left_size;
   u64 right_size;
   int state;
   bool is_right_child;
} tft_treap_verify_frame_t;

enum {
   TFT_TREAP_VERIFY_STACK_INLINE_CAP = 128,
};

static void tft_treap_verify_stack_push(tft_treap_verify_frame_t **stack,
                                        tft_treap_verify_frame_t *inline_stack,
                                        u64 *len, u64 *cap, node_t *node,
                                        u64 size, bool is_right_child) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(len != NULL);
   invariant(cap != NULL);
   invariant(*stack != NULL);
   invariant(*cap > 0);
   invariant(*len <= *cap);
   if (node == NULL) {
      invariant(size == 0);
      return;
   }
   invariant(size > 0);

   if (*len == *cap) {
      *stack = (tft_treap_verify_frame_t *)stack_array_grow(
          *stack, inline_stack, *len, cap, sizeof(**stack));
   }

   invariant(*len < *cap);
   u64 index = *len;
   *len = size_after_inserting_one(*len);
   (*stack)[index] = (tft_treap_verify_frame_t){
       .node = node,
       .size = size,
       .left_size = 0,
       .right_size = 0,
       .state = 0,
       .is_right_child = is_right_child,
   };
}

static void
tft_treap_verify_stack_dispose(tft_treap_verify_frame_t *stack,
                               tft_treap_verify_frame_t *inline_stack, u64 len,
                               u64 cap) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(cap > 0);
   invariant(len <= cap);
   invariant(stack == inline_stack || cap > TFT_TREAP_VERIFY_STACK_INLINE_CAP);
   if (stack != inline_stack) {
      free(stack);
   }
}

static void tft_treap_verify_set_child_size(tft_treap_verify_frame_t *parent,
                                            bool is_right_child, u64 size) {
   invariant(parent != NULL);
   invariant(size > 0);
   if (is_right_child) {
      invariant(parent->right_size == 0);
      parent->right_size = size;
   } else {
      invariant(parent->left_size == 0);
      parent->left_size = size;
   }
}

static u64 tft_verify_treap(node_t *root, u64 size, node_seen_t *seen) {
   invariant(seen != NULL);
   tree_sized_root_require(root, size);
   if (root == NULL) {
      return 0;
   }

   tft_treap_verify_frame_t inline_stack[TFT_TREAP_VERIFY_STACK_INLINE_CAP];
   tft_treap_verify_frame_t *stack = inline_stack;
   u64 len = 0;
   u64 cap = TFT_TREAP_VERIFY_STACK_INLINE_CAP;
   u64 root_size = 0;
   u64 visited = 0;

   tft_treap_verify_stack_push(&stack, inline_stack, &len, &cap, root, size,
                               false);
   while (len > 0) {
      tft_treap_verify_frame_t *frame = &stack[rank_of_last(len)];
      node_t *node = frame->node;

      if (frame->state == 0) {
         frame->state = 1;
         node_seen_insert(seen, node);
         visited = size_after_inserting_one(visited);
         invariant(sizeL(node) < frame->size);
         invariant(tft_rank(node) >= tft_rank(node->l));
         invariant(tft_rank(node) >= tft_rank(node->r));
         tft_treap_verify_stack_push(&stack, inline_stack, &len, &cap, node->l,
                                     sizeL(node), false);
         continue;
      }

      if (frame->state == 1) {
         frame->state = 2;
         tft_treap_verify_stack_push(&stack, inline_stack, &len, &cap, node->r,
                                     sizeR(node, frame->size), true);
         continue;
      }

      invariant((node->l == NULL) == (frame->left_size == 0));
      invariant((node->r == NULL) == (frame->right_size == 0));
      invariant(frame->left_size == sizeL(node));
      invariant(frame->right_size == sizeR(node, frame->size));
      invariant(frame->size == tree_size_from_child_sizes(frame->left_size,
                                                          frame->right_size));

      bool is_right_child = frame->is_right_child;
      u64 frame_size = frame->size;
      len = size_after_removing_one(len);
      if (len == 0) {
         root_size = frame_size;
      } else {
         tft_treap_verify_set_child_size(&stack[rank_of_last(len)],
                                         is_right_child, frame_size);
      }
   }

   tft_treap_verify_stack_dispose(stack, inline_stack, len, cap);
   invariant(root_size == size);
   invariant(visited == size);
   return root_size;
}

static u64 tft_verify_left_spine(node_t *node, node_seen_t *seen) {
   invariant(seen != NULL);
   u64 count = 0;

   for (; node != NULL; node = node->l) {
      node_seen_insert(seen, node);
      count = size_after_inserting_one(count);
      u64 attached = tft_verify_treap(node->r, node->s, seen);
      invariant(attached == node->s);
      count = size_sum(count, attached);
      invariant(tft_rank(node) >= tft_rank(node->r));
      invariant(tft_rank(node) <= tft_rank(node->l) || node->l == NULL);
   }

   return count;
}

static u64 tft_verify_right_spine(node_t *node, node_seen_t *seen) {
   invariant(seen != NULL);
   u64 count = 0;

   for (; node != NULL; node = node->r) {
      node_seen_insert(seen, node);
      u64 attached = tft_verify_treap(node->l, node->s, seen);
      invariant(attached == node->s);
      count = size_sum(count, attached);
      count = size_after_inserting_one(count);
      invariant(tft_rank(node) >= tft_rank(node->l));
      invariant(tft_rank(node) <= tft_rank(node->r) || node->r == NULL);
   }

   return count;
}

static void tft_verify(treap_fingertree_t *tree) {
   invariant(tree != NULL);
   tree_sized_handle_require(tree->tree);
   if (tree->tree.root == NULL) {
      return;
   }

   node_seen_t seen = {0};
   node_t *root = tree->tree.root;
   node_seen_insert(&seen, root);
   invariant(root->s < tree->tree.size);
   invariant(tft_rank(root) >= tft_rank(root->l));
   invariant(tft_rank(root) >= tft_rank(root->r));
   u64 left_size = tft_verify_left_spine(root->l, &seen);
   u64 right_size = tft_verify_right_spine(root->r, &seen);

   invariant(root->s == left_size);
   u64 total = tree_size_from_child_sizes(left_size, right_size);
   invariant(total == tree->tree.size);
   node_seen_dispose(&seen);
}

#ifdef DEBUG
static void tft_debug_require_join_inputs(treap_fingertree_t *left,
                                          treap_fingertree_t *right) {
   invariant(left != NULL);
   invariant(right != NULL);
   tft_verify(left);
   tft_verify(right);
   assert_bst(!tree_roots_overlap(&left->tree, &right->tree));
}
#endif

static list_t tft_make_result(const list_vtable_t *vt, xoshiro256_t source,
                              node_t *root, u64 size) {
   invariant(vt != NULL);
   tree_root_size_require(root, size);
   treap_fingertree_t *res =
       TREE_LIST_ALLOC_UNCHECKED(treap_fingertree_t, root, size);
   res->source = source;
#ifdef DEBUG
   tft_verify(res);
#endif
   return tree_list_make(res, vt);
}

typedef struct {
   node_t *src;
   node_t **out;
   node_t *clone;
   u64 size;
   u64 left_size;
   u64 right_size;
   int state;
   bool is_right_child;
} tft_clone_frame_t;

enum {
   TFT_CLONE_STACK_INLINE_CAP = 128,
};

static void tft_clone_stack_push(tft_clone_frame_t **stack,
                                 tft_clone_frame_t *inline_stack, u64 *len,
                                 u64 *cap, node_t *src, u64 size,
                                 bool is_right_child, node_t **out) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(len != NULL);
   invariant(cap != NULL);
   invariant(out != NULL);
   invariant(*stack != NULL);
   invariant(*cap > 0);
   invariant(*len <= *cap);
   if (src == NULL) {
      invariant(size == 0);
      *out = NULL;
      return;
   }
   invariant(size > 0);

   if (*len == *cap) {
      *stack = (tft_clone_frame_t *)stack_array_grow(*stack, inline_stack, *len,
                                                     cap, sizeof(**stack));
   }

   invariant(*len < *cap);
   u64 index = *len;
   *len = size_after_inserting_one(*len);
   (*stack)[index] = (tft_clone_frame_t){
       .src = src,
       .out = out,
       .clone = NULL,
       .size = size,
       .left_size = 0,
       .right_size = 0,
       .state = 0,
       .is_right_child = is_right_child,
   };
}

static void tft_clone_stack_dispose(tft_clone_frame_t *stack,
                                    tft_clone_frame_t *inline_stack, u64 len,
                                    u64 cap) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(cap > 0);
   invariant(len <= cap);
   invariant(stack == inline_stack || cap > TFT_CLONE_STACK_INLINE_CAP);
   if (stack != inline_stack) {
      free(stack);
   }
}

static node_t tft_clone_node_value(node_t *src) {
   invariant(src != NULL);
   node_t value = *src;
   value.l = NULL;
   value.r = NULL;
#ifdef PERSISTENT
   value.refs = 0;
#endif
   return value;
}

static void tft_clone_set_child_size(tft_clone_frame_t *parent,
                                     bool is_right_child, u64 size) {
   invariant(parent != NULL);
   invariant(size > 0);
   if (is_right_child) {
      invariant(parent->right_size == 0);
      parent->right_size = size;
   } else {
      invariant(parent->left_size == 0);
      parent->left_size = size;
   }
}

static node_t *tft_clone_treap(node_t *root, u64 size, node_seen_t *seen) {
   invariant(seen != NULL);
   tree_sized_root_require(root, size);
   node_t *copy_root = NULL;
   tft_clone_frame_t inline_stack[TFT_CLONE_STACK_INLINE_CAP];
   tft_clone_frame_t *stack = inline_stack;
   u64 len = 0;
   u64 cap = TFT_CLONE_STACK_INLINE_CAP;
   u64 cloned = 0;

   tft_clone_stack_push(&stack, inline_stack, &len, &cap, root, size, false,
                        &copy_root);
   while (len > 0) {
      tft_clone_frame_t *frame = &stack[rank_of_last(len)];
      node_t *node = frame->src;

      if (frame->state == 0) {
         frame->state = 1;
         node_seen_insert(seen, node);
         cloned = size_after_inserting_one(cloned);
         invariant(sizeL(node) < frame->size);
         invariant(tft_rank(node) >= tft_rank(node->l));
         invariant(tft_rank(node) >= tft_rank(node->r));
         frame->clone = tree_allocate(tft_clone_node_value(node));
         *frame->out = frame->clone;
         tft_clone_stack_push(&stack, inline_stack, &len, &cap, node->l,
                              sizeL(node), false, &frame->clone->l);
         continue;
      }

      if (frame->state == 1) {
         frame->state = 2;
         tft_clone_stack_push(&stack, inline_stack, &len, &cap, node->r,
                              sizeR(node, frame->size), true, &frame->clone->r);
         continue;
      }

      invariant(frame->clone != NULL);
      invariant((node->l == NULL) == (frame->left_size == 0));
      invariant((node->r == NULL) == (frame->right_size == 0));
      invariant(frame->left_size == sizeL(node));
      invariant(frame->right_size == sizeR(node, frame->size));
      invariant(frame->size == tree_size_from_child_sizes(frame->left_size,
                                                          frame->right_size));

      bool is_right_child = frame->is_right_child;
      u64 frame_size = frame->size;
      len = size_after_removing_one(len);
      if (len > 0) {
         tft_clone_set_child_size(&stack[rank_of_last(len)], is_right_child,
                                  frame_size);
      }
   }

   tft_clone_stack_dispose(stack, inline_stack, len, cap);
   invariant(cloned == size);
   return copy_root;
}

static node_t *tft_clone_left_spine(node_t *node, node_seen_t *seen,
                                    u64 *size) {
   invariant(seen != NULL);
   invariant(size != NULL);
   node_t *root = NULL;
   node_t **out = &root;
   *size = 0;

   for (; node != NULL; node = node->l) {
      node_seen_insert(seen, node);
      *size = size_after_inserting_one(*size);

      node_t *clone = tree_allocate(tft_clone_node_value(node));
      *out = clone;
      clone->r = tft_clone_treap(node->r, node->s, seen);
      *size = size_sum(*size, node->s);
      invariant(tft_rank(node) >= tft_rank(node->r));
      invariant(tft_rank(node) <= tft_rank(node->l) || node->l == NULL);
      out = &clone->l;
   }

   return root;
}

static node_t *tft_clone_right_spine(node_t *node, node_seen_t *seen,
                                     u64 *size) {
   invariant(seen != NULL);
   invariant(size != NULL);
   node_t *root = NULL;
   node_t **out = &root;
   *size = 0;

   for (; node != NULL; node = node->r) {
      node_seen_insert(seen, node);

      node_t *clone = tree_allocate(tft_clone_node_value(node));
      *out = clone;
      clone->l = tft_clone_treap(node->l, node->s, seen);
      *size = size_sum(*size, node->s);
      *size = size_after_inserting_one(*size);
      invariant(tft_rank(node) >= tft_rank(node->l));
      invariant(tft_rank(node) <= tft_rank(node->r) || node->r == NULL);
      out = &clone->r;
   }

   return root;
}

static node_t *tft_clone_nodes(node_t *root, u64 size) {
   tree_root_size_require(root, size);
   if (root == NULL) {
      return NULL;
   }

   node_seen_t seen = {0};
   node_seen_insert(&seen, root);
   invariant(tft_rank(root) >= tft_rank(root->l));
   invariant(tft_rank(root) >= tft_rank(root->r));

   node_t *clone = tree_allocate(tft_clone_node_value(root));
   u64 left_size = 0;
   u64 right_size = 0;
   clone->l = tft_clone_left_spine(root->l, &seen, &left_size);
   clone->r = tft_clone_right_spine(root->r, &seen, &right_size);

   invariant(root->s == left_size);
   u64 total = tree_size_from_child_sizes(left_size, right_size);
   invariant(total == size);
   node_seen_dispose(&seen);
   return clone;
}

static treap_fingertree_t *tft_clone_physical(treap_fingertree_t *orig) {
   invariant(orig != NULL);
   tree_list_require_sized_tree(&orig->tree);
   treap_fingertree_t *copy = (treap_fingertree_t *)bst_malloc(sizeof(*copy));
   *copy = *orig;
   copy->tree.root = tft_clone_nodes(orig->tree.root, orig->tree.size);
   return copy;
}

// ─────────────────────────────────────────────────────────────────────────────
// List vtable
// ─────────────────────────────────────────────────────────────────────────────

static list_t tft_vt_new(const list_vtable_t *vt) {
   list_vtable_require(vt);
   treap_fingertree_t *t =
       (treap_fingertree_t *)bst_calloc(1, sizeof(treap_fingertree_t));
   xoshiro256_seed(&t->source, random_uint64());
   return tree_list_make(t, vt);
}

static list_t tft_vt_clone(list_t *self) {
   list_require(self);
   treap_fingertree_t *orig = (treap_fingertree_t *)self->impl;
#ifndef PERSISTENT
   treap_fingertree_t *copy = tft_clone_physical(orig);
   return tree_list_make(copy, self->vt);
#else
#ifdef DEBUG
   tft_verify(orig);
#endif

   treap_fingertree_t *copy = (treap_fingertree_t *)bst_malloc(sizeof(*copy));
   *copy = *orig;
   tree_share(copy->tree.root);
#ifdef DEBUG
   tft_verify(copy);
#endif
   return tree_list_make(copy, self->vt);
#endif
}

static void tft_vt_free(list_t *self) {
   list_require(self);
   treap_fingertree_t *t = (treap_fingertree_t *)self->impl;
   tree_list_require_sized_tree(&t->tree);
   tree_list_free_impl(self, t, sizeof(*t), &t->tree);
}

static u64 tft_vt_size(list_t *self) {
   list_require(self);
   treap_fingertree_t *t = (treap_fingertree_t *)self->impl;
   return tree_list_require_sized_tree(&t->tree)->size;
}

static void tft_vt_each(list_t *self, void (*v)(u64, void *), void *c) {
   list_require(self);
   treap_fingertree_t *t = (treap_fingertree_t *)self->impl;
   tft_each(t, v, c);
}

static void tft_vt_verify(list_t *self) {
   list_require(self);
   treap_fingertree_t *t = (treap_fingertree_t *)self->impl;
   tft_verify(t);
}

static u64 tft_vt_select(list_t *self, u64 i) {
   list_require(self);
   treap_fingertree_t *t = (treap_fingertree_t *)self->impl;
   return tft_select(t, i);
}

static void tft_vt_update(list_t *self, u64 i, u64 x) {
   list_require(self);
   treap_fingertree_t *t = (treap_fingertree_t *)self->impl;
   tft_update(t, i, x);
}

static void tft_vt_insert(list_t *self, u64 i, u64 x) {
   list_require(self);
   treap_fingertree_t *t = (treap_fingertree_t *)self->impl;
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   tft_insert(t, i, x);
   invariant(t->tree.size == inserted_size);
   tree_list_require_sized_tree(&t->tree);
}

static u64 tft_vt_delete(list_t *self, u64 i) {
   list_require(self);
   treap_fingertree_t *t = (treap_fingertree_t *)self->impl;
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   u64 x = tft_delete(t, i);
   invariant(t->tree.size == deleted_size);
   tree_list_require_sized_tree(&t->tree);
   return x;
}

TREE_LIST_DEFINE_EVENT_APPLIERS(tft)

static void tft_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   tree_list_require_split(self, i, out_l, out_r);
   treap_fingertree_t *t = (treap_fingertree_t *)self->impl;
   u64 size = t->tree.size;
   tft_split_result_t split = tft_split(t, i);
   invariant(split.left.size == i);
   u64 right_size = tree_list_split_right_size(size, i);
   invariant(split.right.size == right_size);
   tree_list_require_prepared_split_result(self, i, size, i, right_size, out_l,
                                           out_r);
   *out_l =
       tft_make_result(self->vt, t->source, split.left.root, split.left.size);
   *out_r =
       tft_make_result(self->vt, t->source, split.right.root, split.right.size);
#ifndef PERSISTENT
   t->tree = (tree_t){0};
#endif
}

static list_t tft_vt_join(list_t *self, list_t *other) {
   tree_list_require_join(self, other);
   treap_fingertree_t *t = (treap_fingertree_t *)self->impl;
   treap_fingertree_t *that = (treap_fingertree_t *)other->impl;
   u64 size = tree_list_join_size(t->tree.size, that->tree.size);
   tree_list_require_join_result(self, other, size);

#ifdef PERSISTENT
   if (t->tree.size == 0) {
      return tft_vt_clone(other);
   }
   if (that->tree.size == 0) {
      return tft_vt_clone(self);
   }
#else
   if (t->tree.size == 0) {
      list_t result = tft_make_result(self->vt, that->source, that->tree.root,
                                      that->tree.size);
      t->tree = (tree_t){0};
      that->tree = (tree_t){0};
      return result;
   }
   if (that->tree.size == 0) {
      list_t result =
          tft_make_result(self->vt, t->source, t->tree.root, t->tree.size);
      t->tree = (tree_t){0};
      that->tree = (tree_t){0};
      return result;
   }
#endif

#ifdef PERSISTENT
   treap_fingertree_t *lft = tft_clone_physical(t);
   treap_fingertree_t *rft = tft_clone_physical(that);

#ifdef DEBUG
   tft_debug_require_join_inputs(lft, rft);
#endif
   node_t *root = tft_joinUp(lft, rft);

   free(lft);
   free(rft);
#else
#ifdef DEBUG
   tft_debug_require_join_inputs(t, that);
#endif
   node_t *root = tft_joinUp(t, that);
   t->tree = (tree_t){0};
   that->tree = (tree_t){0};
#endif

   return tft_make_result(self->vt, t->source, root, size);
}

TREE_LIST_DEFINE_SIZED_TREE_ACCESSOR(treap_fingertree_t, tft)
const list_vtable_t treap_fingertree_vtable =
    TREE_LIST_VTABLE("TreapFingerTree", tft);
