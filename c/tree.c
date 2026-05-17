#include "bst.h"
#include "rank.h"
#include <math.h>


// ─────────────────────────────────────────────────────────────────────────────
// Arena allocator
// ─────────────────────────────────────────────────────────────────────────────

arena_t *global_arena = NULL;

#ifdef MEASUREMENTS
u64 measurement_rotations = 0;
u64 measurement_allocations = 0;
u64 measurement_partition_count = 0;
u64 measurement_partition_depth = 0;
#endif

void arena_init(arena_t *a, u64 capacity) {
   a->first.buffer = (node_t *)malloc(capacity * sizeof(node_t));
   a->first.capacity = capacity;
   a->first.offset = 0;
   a->first.next = NULL;
   a->current = &a->first;
}

void arena_grow(arena_t *a) {
   // Allocate a new chunk the same size as the first
   u64 cap = a->first.capacity;
   arena_chunk_t *chunk = (arena_chunk_t *)malloc(sizeof(arena_chunk_t));
   chunk->buffer = (node_t *)malloc(cap * sizeof(node_t));
   chunk->capacity = cap;
   chunk->offset = 0;
   chunk->next = NULL;
   a->current->next = chunk;
   a->current = chunk;
}

void arena_reset(arena_t *a) {
   // Free overflow chunks, keep the first
   arena_chunk_t *c = a->first.next;
   while (c) {
      arena_chunk_t *next = c->next;
      free(c->buffer);
      free(c);
      c = next;
   }
   a->first.offset = 0;
   a->first.next = NULL;
   a->current = &a->first;
}

void arena_destroy(arena_t *a) {
   arena_reset(a);
   free(a->first.buffer);
   a->first.buffer = NULL;
   a->first.capacity = 0;
}

static bool arena_contains(arena_t *a, node_t *p) {
   if (a == NULL || p == NULL) {
      return false;
   }
   uintptr_t addr = (uintptr_t)p;
   for (arena_chunk_t *c = &a->first; c != NULL; c = c->next) {
      uintptr_t start = (uintptr_t)c->buffer;
      uintptr_t end = start + c->capacity * sizeof(node_t);
      if (addr >= start && addr < end) {
         return true;
      }
   }
   return false;
}


// ─────────────────────────────────────────────────────────────────────────────
// Clone
// ─────────────────────────────────────────────────────────────────────────────

// Creates a shallow copy of the tree and shares its root with the copy.
tree_t tree_clone(tree_t *tree) {
   tree_share(tree->root);
   return *tree;
}


// ─────────────────────────────────────────────────────────────────────────────
// Inorder traversal
// ─────────────────────────────────────────────────────────────────────────────

void inorder(node_t *p, void (*visit)(u64, void *), void *ctx) {
   if (p == NULL) {
      return;
   }
   inorder(p->l, visit, ctx);
   visit(p->x, ctx);
   inorder(p->r, visit, ctx);
}


// ─────────────────────────────────────────────────────────────────────────────
// Height and size (O(n), for verification only)
// ─────────────────────────────────────────────────────────────────────────────

int height(node_t *p) {
   if (p == NULL) {
      return -1;
   }
   return 1 + MAX(height(p->l), height(p->r));
}

// Average path length (O(n))
static void averagePathLengthHelper(node_t *p, u64 depth, u64 *totalDepth, u64 *totalNodes) {
   if (p == NULL) {
      return;
   }
   *totalNodes = *totalNodes + 1;
   *totalDepth = *totalDepth + depth;

   averagePathLengthHelper(p->l, depth + 1, totalDepth, totalNodes);
   averagePathLengthHelper(p->r, depth + 1, totalDepth, totalNodes);
}

double nodeAveragePathLength(node_t *p) {
   if (p == NULL) {
      return 0;
   }
   u64 totalDepth = 0;
   u64 totalNodes = 0;
   averagePathLengthHelper(p, 0, &totalDepth, &totalNodes);
   return (double)totalDepth / (double)totalNodes;
}

// Counts the number of nodes reachable from p*, including itself.
u64 node_size(node_t *p) {
   if (p == NULL) {
      return 0;
   } else {
      return 1 + node_size(p->l) + node_size(p->r);
   }
}


// ─────────────────────────────────────────────────────────────────────────────
// Free nodes recursively (ref-count aware)
// ─────────────────────────────────────────────────────────────────────────────

#ifdef PERSISTENT
void tree_release(node_t *p) {
   if (p == NULL) {
      return;
   }
   if (p->refs > 0) {
      p->refs--;
      return;
   }

   node_t *l = p->l;
   node_t *r = p->r;
   tree_release(l);
   tree_release(r);

   if (!arena_contains(global_arena, p)) {
      free(p);
   }
}

void free_nodes(node_t *p) {
   tree_release(p);
}
#endif


// ─────────────────────────────────────────────────────────────────────────────
// Verify sizes
// ─────────────────────────────────────────────────────────────────────────────

u64 tree_verify_size(node_t *p, u64 s) {
   if (p == NULL) {
      return 0;
   }
   u64 sl = tree_verify_size(p->l, sizeL(p));
   u64 sr = tree_verify_size(p->r, sizeR(p, s));

   invariant(s == sl + sr + 1);
   return s;
}


// ─────────────────────────────────────────────────────────────────────────────
// Delete min / max
// ─────────────────────────────────────────────────────────────────────────────

static void tree_deleteMin2(node_t *r, node_t **out_root, node_t **out_min) {
   node_t n = {0};
   node_t *l = &n;
   for (;;) {
      tree_persist(&r);
      if (r->l == NULL) {
         l->l = r->r;
         r->r = NULL;
         *out_root = n.l;
         *out_min = r;
         return;
      }
      r->s = r->s - 1;
      l->l = r;
      l = l->l;
      r = r->l;
   }
}

node_t *tree_deleteMin(node_t **p) {
   node_t *root, *min;
   tree_deleteMin2(*p, &root, &min);
   *p = root;
   return min;
}

static void tree_deleteMax2(node_t *p, node_t **out_root, node_t **out_max) {
   node_t n = {0};
   node_t *r = &n;
   for (;;) {
      tree_persist(&p);
      if (p->r == NULL) {
         r->r = p->l;
         p->l = NULL;
         *out_root = n.r;
         *out_max = p;
         return;
      }
      r->r = p;
      r = r->r;
      p = p->r;
   }
}

node_t *tree_deleteMax(node_t **p) {
   node_t *root, *max;
   tree_deleteMax2(*p, &root, &max);
   *p = root;
   return max;
}


// ─────────────────────────────────────────────────────────────────────────────
// Dissolve
// ─────────────────────────────────────────────────────────────────────────────

node_t *tree_dissolve(node_t *p, u64 s) {
   // dissolvePreferLargerSubtree
   if (p->l == NULL) {
      return p->r;
   }
   if (p->r == NULL) {
      return p->l;
   }
   if (sizeL(p) < sizeR(p, s)) {
      node_t *r = tree_deleteMin(&p->r);
      r->l = p->l;
      r->r = p->r;
      r->s = p->s;
      r->y = p->y;
      return r;
   } else {
      node_t *l = tree_deleteMax(&p->l);
      l->r = p->r;
      l->l = p->l;
      l->s = p->s - 1;
      l->y = p->y;
      return l;
   }
}


// ─────────────────────────────────────────────────────────────────────────────
// Lookup
// ─────────────────────────────────────────────────────────────────────────────

u64 tree_lookup(tree_t *tree, u64 i) {
   node_t *p = tree->root;
   for (;;) {
      if (i == p->s) {
         return p->x;
      }
      if (i < p->s) {
         p = p->l;
      } else {
         i = i - p->s - 1;
         p = p->r;
      }
   }
}


// ─────────────────────────────────────────────────────────────────────────────
// Update
// ─────────────────────────────────────────────────────────────────────────────

static void tree_update_internal(node_t *p, u64 i, u64 x) {
   for (;;) {
      if (i == p->s) {
         p->x = x;
         return;
      }
      if (i < p->s) {
         tree_persist(&p->l);
         p = p->l;
      } else {
         tree_persist(&p->r);
         i = i - p->s - 1;
         p = p->r;
      }
   }
}

void tree_update_base(tree_t *tree, u64 i, u64 x) {
   assert_bst(i < tree->size);
   tree_persist(&tree->root);
   tree_update_internal(tree->root, i, x);
}


// ─────────────────────────────────────────────────────────────────────────────
// Base insert (unbalanced)
// ─────────────────────────────────────────────────────────────────────────────

void tree_insert_internal(node_t **p, u64 s, u64 i, u64 x) {
   for (;;) {
      if (*p == NULL) {
         *p = tree_allocate((node_t){.x = x});
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

void tree_insert_base(tree_t *tree, u64 i, u64 x) {
   tree_insert_internal(&tree->root, tree->size, i, x);
   tree->size++;
}


// ─────────────────────────────────────────────────────────────────────────────
// Base delete (unbalanced, uses dissolve)
// ─────────────────────────────────────────────────────────────────────────────

u64 tree_delete_internal(node_t **p, u64 s, u64 i) {
   for (;;) {
      tree_persist(p);
      if (i == (*p)->s) {
         node_t *old = *p;
         u64 x = old->x;
         node_t *root = tree_dissolve(old, s);
         tree_share(old->l);
         tree_share(old->r);
         tree_release(old);
         *p = root;
         return x;
      }
      if (i < (*p)->s) {
         s = (*p)->s;
    (*p)->s = (*p)->s - 1;
         p = &(*p)->l;
      } else {
         i = i - (*p)->s - 1;
         s = s - (*p)->s - 1;
         p = &(*p)->r;
      }
   }
}

u64 tree_delete_base(tree_t *tree, u64 i) {
   assert_bst(i < tree->size);
   u64 x = tree_delete_internal(&tree->root, tree->size, i);
   tree->size--;
   return x;
}


// ─────────────────────────────────────────────────────────────────────────────
// Split / Join (base implementations)
// ─────────────────────────────────────────────────────────────────────────────

void tree_split_internal(node_t *p, u64 i, node_t **out_l, node_t **out_r) {
   node_t n = {0};
   node_t *l = &n;
   node_t *r = &n;
   while (p != NULL) {
      tree_persist(&p);
      if (i <= p->s) {
       p->s = p->s - i;
       r->l = p;
         r = r->l;
         p = p->l;
      } else {
         i = i - p->s - 1;
       l->r = p;
         l = l->r;
         p = p->r;
      }
   }
   l->r = NULL;
   r->l = NULL;
   *out_l = n.r;
   *out_r = n.l;
}

void tree_split_base(tree_t *tree, u64 i, tree_t *out_l, tree_t *out_r) {
   assert_bst(i <= tree->size);
   tree_share(tree->root);
   node_t *l, *r;
   tree_split_internal(tree->root, i, &l, &r);
   *out_l = (tree_t){.root = l, .size = i};
   *out_r = (tree_t){.root = r, .size = tree->size - i};
}

node_t *tree_join_internal(node_t *l, node_t *r, u64 sl, u64 sr) {
   if (l == NULL) {
      return r;
   }
   if (r == NULL) {
      return l;
   }
   if (sl <= sr) {
      tree_persist(&l);
      tree_persist(&r);
      node_t *p = tree_deleteMin(&r);
      p->l = l;
      p->r = r;
      p->s = sl;
      return p;
   } else {
      tree_persist(&l);
      tree_persist(&r);
      node_t *p = tree_deleteMax(&l);
      p->r = r;
      p->l = l;
      p->s = sl - 1;
      return p;
   }
}

tree_t tree_join_base(tree_t *tree, tree_t *other) {
   tree_share(tree->root);
   tree_share(other->root);
   return (tree_t){
      .root = tree_join_internal(tree->root, other->root, tree->size, other->size),
      .size = tree->size + other->size,
   };
}


// ─────────────────────────────────────────────────────────────────────────────
// Partition
// ─────────────────────────────────────────────────────────────────────────────

node_t *tree_partition(node_t *p, u64 i) {
   assert_bst(i < node_size(p));

   node_t n = {.s = i};
   node_t *l = &n;
   node_t *r = &n;
   while (i != p->s) {
      tree_persist(&p);
      if (i < p->s) {
       p->s = p->s - i - 1;
       r->l = p;
         r = r->l;
         p = p->l;
      } else {
         i = i - p->s - 1;
       l->r = p;
         l = l->r;
         p = p->r;
      }
   }
   tree_persist(&p);
   l->r = p->l;
   r->l = p->r;
   p->l = n.r;
   p->r = n.l;
   p->s = n.s;
   return p;
}


// ─────────────────────────────────────────────────────────────────────────────
// Tree rotations with persistence
// ─────────────────────────────────────────────────────────────────────────────

void treeRotateL(node_t **p) {
   tree_persist(&(*p)->r);
   *p = rotateL(*p);
}

void treeRotateR(node_t **p) {
   tree_persist(&(*p)->l);
   *p = rotateR(*p);
}

void treeRotateRL(node_t **p) {
   tree_persist(&(*p)->r);
   tree_persist(&(*p)->r->l);
   *p = rotateRL(*p);
}

void treeRotateLR(node_t **p) {
   tree_persist(&(*p)->l);
   tree_persist(&(*p)->l->r);
   *p = rotateLR(*p);
}


// ─────────────────────────────────────────────────────────────────────────────
// Append to rightmost / leftmost
// ─────────────────────────────────────────────────────────────────────────────

void tree_appendR(node_t **p, node_t *n) {
   while (*p != NULL) {
      tree_persist(p);
      p = &(*p)->r;
   }
   *p = n;
}

void tree_appendL(node_t **p, node_t *n) {
   while (*p != NULL) {
      tree_persist(p);
      p = &(*p)->l;
   }
   *p = n;
}


// ─────────────────────────────────────────────────────────────────────────────
// Red-black verification (from redblack.go)
// ─────────────────────────────────────────────────────────────────────────────

void verifyRedBlackRanks(node_t *p) {
   if (p == NULL) {
      return;
   }
   if (isLeaf(p)) {
      invariant(rank_of(p) == 0);
   }
   invariant(rank_of(p) >= rank_of(p->l));
   invariant(rank_of(p) >= rank_of(p->r));

   // All rank differences are 0 or 1.
   invariant(isZeroChild(p, p->l) || isOneChild(p, p->l));
   invariant(isZeroChild(p, p->r) || isOneChild(p, p->r));

   // No parent of a 0-child is a 0-child.
   invariant(!isZeroChild(p, p->l) || !isZeroChild(p->l, p->l->l));
   invariant(!isZeroChild(p, p->l) || !isZeroChild(p->l, p->l->r));
   invariant(!isZeroChild(p, p->r) || !isZeroChild(p->r, p->r->l));
   invariant(!isZeroChild(p, p->r) || !isZeroChild(p->r, p->r->r));

   verifyRedBlackRanks(p->l);
   verifyRedBlackRanks(p->r);
}

// ─────────────────────────────────────────────────────────────────────────────
// replacedBy helpers
// ─────────────────────────────────────────────────────────────────────────────

node_t *tree_replacedByRightSubtree(node_t **p) {
   tree_persist(p);
   node_t *r = *p;
   *p = (*p)->r;
   r->r = NULL;
   return r;
}

node_t *tree_replacedByLeftSubtree(node_t **p) {
   tree_persist(p);
   node_t *l = *p;
   *p = (*p)->l;
   l->l = NULL;
   return l;
}


// ─────────────────────────────────────────────────────────────────────────────
// pathLeft / pathRight (triple-pointer traversal)
// ─────────────────────────────────────────────────────────────────────────────

void tree_pathLeft(node_t ***p) {
   assert_bst((**p)->l != NULL);
   tree_persist(&(**p)->l);
   *p = insertL(**p);
}

void tree_pathRight(node_t ***p, u64 *i) {
   assert_bst((**p)->r != NULL);
   tree_persist(&(**p)->r);
   *p = insertR(**p, i);
}


// ─────────────────────────────────────────────────────────────────────────────
// attach helpers
// ─────────────────────────────────────────────────────────────────────────────

void tree_attach(node_t **p, u64 x) {
   *p = tree_allocate((node_t){.x = x});
}

void tree_attachL(node_t *p, u64 x) {
   p->s++;
   p->l = tree_allocate((node_t){.x = x});
}

void tree_attachR(node_t *p, u64 x) {
   p->r = tree_allocate((node_t){.x = x});
}

void tree_attachLL(node_t *p, u64 x) {
   tree_persist(&p->l);
   p->s++;
   p->l->s++;
   p->l->l = tree_allocate((node_t){.x = x});
}

void tree_attachRR(node_t *p, u64 x) {
   tree_persist(&p->r);
   p->r->r = tree_allocate((node_t){.x = x});
}

void tree_attachLR(node_t *p, u64 x) {
   tree_persist(&p->l);
   p->s++;
   p->l->r = tree_allocate((node_t){.x = x});
}

void tree_attachRL(node_t *p, u64 x) {
   tree_persist(&p->r);
   p->r->s++;
   p->r->l = tree_allocate((node_t){.x = x});
}


int verifyRedBlackHeight(node_t *p, u64 s) {
   if (p == NULL) {
      return -1;
   }
   int l = verifyRedBlackHeight(p->l, sizeL(p));
   int r = verifyRedBlackHeight(p->r, sizeR(p, s));
   int h = 1 + MAX(l, r);

   invariant(h <= 2 * rank_of(p) + 1);
   invariant(h <= (int)(2.0 * log2((double)s)));
   return h;
}


// ─────────────────────────────────────────────────────────────────────────────
// AVL verification
// ─────────────────────────────────────────────────────────────────────────────

int verifyAVLRanks(node_t *p, u64 s) {
   if (p == NULL) {
      return -1;
   }
   // AVL rule: Every node is 1,1 or 1,2
   invariant(isOneChild(p, p->l) || isTwoChild(p, p->l));
   invariant(isOneChild(p, p->r) || isTwoChild(p, p->r));
   invariant(isOneChild(p, p->l) || isOneChild(p, p->r));
   invariant(isOneChild(p, p->r) || isOneChild(p, p->l));

   invariant(rank_of(p->l) == verifyAVLRanks(p->l, sizeL(p)));
   invariant(rank_of(p->r) == verifyAVLRanks(p->r, sizeR(p, s)));

   int h = 1 + MAX(rank_of(p->l), rank_of(p->r));

   invariant(rank_of(p) == h);
   invariant(rank_of(p) <= (int)(1.44 * log2((double)(s + 2)) - 0.328));

   return h;
}

void verifyAVLWeakRanks(node_t *p) {
   if (p == NULL) return;
   if (isLeaf(p)) {
      invariant(rank_of(p) == 0);
   }
   invariant(rank_of(p) > rank_of(p->l));
   invariant(rank_of(p) > rank_of(p->r));
   invariant(isOneChild(p, p->l) || isTwoChild(p, p->l));
   invariant(isOneChild(p, p->r) || isTwoChild(p, p->r));
   verifyAVLWeakRanks(p->l);
   verifyAVLWeakRanks(p->r);
}

void verifyAVLWeakHeight(node_t *root, u64 size) {
   if (root == NULL) return;
   int h = height(root);
   invariant(rank_of(root) >= h || h == 0);
   invariant(rank_of(root) <= 2 * h);
   invariant(rank_of(root) <= (int)(2 * util_log2(size)));
}

void verifyAVLRelaxedRanks(node_t *p) {
   if (p == NULL) return;
   invariant(rank_of(p) >= height(p));
   invariant(rank_of(p) > rank_of(p->l));
   invariant(rank_of(p) > rank_of(p->r));
   verifyAVLRelaxedRanks(p->l);
   verifyAVLRelaxedRanks(p->r);
}

void verifyRedBlackRelaxedRanks(node_t *p) {
   if (p == NULL) return;
   invariant(rank_of(p) >= rank_of(p->l));
   invariant(rank_of(p) >= rank_of(p->r));
   invariant(!isZeroChild(p, p->l) || !isZeroChild(p->l, p->l->l));
   invariant(!isZeroChild(p, p->l) || !isZeroChild(p->l, p->l->r));
   invariant(!isZeroChild(p, p->r) || !isZeroChild(p->r, p->r->l));
   invariant(!isZeroChild(p, p->r) || !isZeroChild(p->r, p->r->r));
   verifyRedBlackRelaxedRanks(p->l);
   verifyRedBlackRelaxedRanks(p->r);
}

void verifyRedBlackRelaxedHeight(node_t *p) {
   invariant(height(p) <= 2 * rank_of(p) + 1);
}
