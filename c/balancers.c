#include "balancers.h"
#include <math.h>


// ─────────────────────────────────────────────────────────────────────────────
// Partition (balancer-specific, without persistence)
//
// Moves the node at position i to the root using split/reassemble.
// Unlike tree_partition in tree.c, this does NOT call tree_persist.
// ─────────────────────────────────────────────────────────────────────────────

static node_t *balancer_partition(node_t *p, u64 i) {
   // assert_bst(i < node_size(p));
   measurement(&measurement_partition_count, 1);

   node_t n = {.s = i};
   node_t *l = &n;
   node_t *r = &n;
   while (i != p->s) {
      measurement(&measurement_partition_depth, 1);
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
   l->r = p->l;
   r->l = p->r;
   p->l = n.r;
   p->r = n.l;
   p->s = n.s;
   return p;
}


// ─────────────────────────────────────────────────────────────────────────────
// Randomize: shuffles tree shape using random partitions
// ─────────────────────────────────────────────────────────────────────────────

static node_t *tree_randomize_internal(node_t *p, u64 s, distribution_t *dist) {
   // assert_bst(node_size(p) == s);
   if (p == NULL) {
      return NULL;
   }
   p = tree_partition(p, dist_less_than(dist, s));
   p->l = tree_randomize_internal(p->l, sizeL(p), dist);
   p->r = tree_randomize_internal(p->r, sizeR(p, s), dist);
   return p;
}

tree_t tree_randomize(tree_t tree, distribution_t *dist) {
   tree.root = tree_randomize_internal(tree.root, tree.size, dist);
   return tree;
}


// ─────────────────────────────────────────────────────────────────────────────
// Median
//
// -1 <= L - R <= 1
// ─────────────────────────────────────────────────────────────────────────────

static bool median_isBalanced(node_t *p, u64 s) {
   u64 sl = sizeL(p);
   u64 sr = sizeR(p, s);
   return sl + 1 >= sr &&
          sr + 1 >= sl;
}

static node_t *median_balance(node_t *p, u64 s) {
   if (s <= 2) {
      return p;
   }
   if (!median_isBalanced(p, s)) {
      p = balancer_partition(p, s >> 1);
   }
   p->l = median_balance(p->l, sizeL(p));
   p->r = median_balance(p->r, sizeR(p, s));
   return p;
}

static tree_t median_restore(tree_t tree) {
   tree.root = median_balance(tree.root, tree.size);
   return tree;
}

static void median_verify_internal(node_t *p, u64 s) {
   if (p == NULL) {
      return;
   }
   int sl = (int)sizeL(p);
   int sr = (int)sizeR(p, s);
   invariant(abs(sl - sr) <= 1);

   median_verify_internal(p->l, sizeL(p));
   median_verify_internal(p->r, sizeR(p, s));
}

static void median_verify(tree_t tree) {
   median_verify_internal(tree.root, tree.size);
}

const balancer_t balancer_median = {
   .name    = "Median",
   .restore = median_restore,
   .verify  = median_verify,
};


// ─────────────────────────────────────────────────────────────────────────────
// Height
//
// A node is height-balanced when the difference between the height of its
// subtrees is no greater than 1, and both subtrees are also height-balanced.
//
// invariant(height(p) <= FloorLog2(s))
// ─────────────────────────────────────────────────────────────────────────────

static bool height_isBalanced(node_t *p, u64 s) {
   u64 sl = sizeL(p);
   u64 sr = sizeR(p, s);
   return util_gte_msb(sl + 1, sr) &&
          util_gte_msb(sr + 1, sl);
}

static node_t *height_balance(node_t *p, u64 s) {
   if (s <= 2) {
      return p;
   }
   if (!height_isBalanced(p, s)) {
      p = balancer_partition(p, s >> 1);
   }
   p->l = height_balance(p->l, sizeL(p));
   p->r = height_balance(p->r, sizeR(p, s));
   return p;
}

static tree_t height_restore(tree_t tree) {
   tree.root = height_balance(tree.root, tree.size);
   return tree;
}

static int height_verify_internal(node_t *p, u64 s) {
   if (p == NULL) {
      return 0;
   }
   int heightL = height_verify_internal(p->l, sizeL(p));
   int heightR = height_verify_internal(p->r, sizeR(p, s));

   invariant(abs(heightL - heightR) <= 1);

   return 1 + MAX(heightL, heightR);
}

static void height_verify(tree_t tree) {
   height_verify_internal(tree.root, tree.size);
}

const balancer_t balancer_height = {
   .name    = "Height",
   .restore = height_restore,
   .verify  = height_verify,
};


// ─────────────────────────────────────────────────────────────────────────────
// Log
//
// -1 <= ⌊log₂(L)⌋ - ⌊log₂(R)⌋ <= 1
// ─────────────────────────────────────────────────────────────────────────────

static bool log_isBalanced(node_t *p, u64 s) {
   u64 sl = sizeL(p);
   u64 sr = sizeR(p, s);
   return util_gte_msb(sl + 1, (sr + 1) >> 1) &&
          util_gte_msb(sr + 1, (sl + 1) >> 1);
}

static node_t *log_balance(node_t *p, u64 s) {
   if (s <= 3) {
      return p;
   }
   if (!log_isBalanced(p, s)) {
      p = balancer_partition(p, s >> 1);
   }
   p->l = log_balance(p->l, sizeL(p));
   p->r = log_balance(p->r, sizeR(p, s));
   return p;
}

static tree_t log_restore(tree_t tree) {
   tree.root = log_balance(tree.root, tree.size);
   return tree;
}

static void log_verify_internal(node_t *p, u64 s) {
   if (p == NULL) {
      return;
   }
   u64 sl = sizeL(p);
   u64 sr = sizeR(p, s);

   int logL = (int)util_log2(sl + 1);
   int logR = (int)util_log2(sr + 1);
   invariant(abs(logL - logR) <= 1);

   log_verify_internal(p->l, sl);
   log_verify_internal(p->r, sr);
}

static void log_verify(tree_t tree) {
   log_verify_internal(tree.root, tree.size);
}

const balancer_t balancer_log = {
   .name    = "Log",
   .restore = log_restore,
   .verify  = log_verify,
};


// ─────────────────────────────────────────────────────────────────────────────
// Weight
//
// (x + 1) >= (y + 1) >> 1  in both directions
// ─────────────────────────────────────────────────────────────────────────────

static bool weight_isBalanced(u64 x, u64 y) {
   return (x + 1) >= (y + 1) >> 1 &&
          (y + 1) >= (x + 1) >> 1;
}

static node_t *weight_balance(node_t *p, u64 s) {
   if (s <= 3) {
      return p;
   }
   if (!weight_isBalanced(sizeL(p), sizeR(p, s))) {
      p = balancer_partition(p, s >> 1);
   }
   p->l = weight_balance(p->l, sizeL(p));
   p->r = weight_balance(p->r, sizeR(p, s));
   return p;
}

static tree_t weight_restore(tree_t tree) {
   tree.root = weight_balance(tree.root, tree.size);
   return tree;
}

static void weight_verify_internal(node_t *p, u64 s) {
   if (p == NULL) {
      return;
   }
   u64 sl = sizeL(p);
   u64 sr = sizeR(p, s);

   invariant((sl + 1) >= (sr + 1) / 2);
   invariant((sr + 1) >= (sl + 1) / 2);

   weight_verify_internal(p->l, sl);
   weight_verify_internal(p->r, sr);
}

static void weight_verify(tree_t tree) {
   weight_verify_internal(tree.root, tree.size);
}

const balancer_t balancer_weight = {
   .name    = "Weight",
   .restore = weight_restore,
   .verify  = weight_verify,
};


// ─────────────────────────────────────────────────────────────────────────────
// Cost
// ─────────────────────────────────────────────────────────────────────────────

static bool cost_isBalanced(node_t *p, u64 s) {
   if (sizeL(p) >= sizeR(p, s)) {
     return sizeR(p, s) >= sizeL(p->l) && sizeR(p, s) >= sizeR(p->l, sizeL(p));
   } else {
     return sizeL(p) >= sizeR(p->r, sizeR(p, s)) && sizeL(p) >= sizeL(p->r);
   }
}

static node_t *cost_balance(node_t *p, u64 s) {
   if (s <= 2) {
      return p;
   }
   if (!cost_isBalanced(p, s)) {
      p = balancer_partition(p, s >> 1);
   }
   p->l = cost_balance(p->l, sizeL(p));
   p->r = cost_balance(p->r, sizeR(p, s));
   return p;
}

static tree_t cost_restore(tree_t tree) {
   tree.root = cost_balance(tree.root, tree.size);
   return tree;
}

static int cost_verify_internal(node_t *p, u64 s) {
   if (p == NULL) {
      return 0;
   }
   invariant(p->l == NULL || sizeR(p, s) >= sizeL(p->l));
   invariant(p->l == NULL || sizeR(p, s) >= sizeR(p->l, sizeL(p)));

   invariant(p->r == NULL || sizeL(p) >= sizeL(p->r));
   invariant(p->r == NULL || sizeL(p) >= sizeR(p->r, sizeR(p, s)));

   int heightL = cost_verify_internal(p->l, sizeL(p));
   int heightR = cost_verify_internal(p->r, sizeR(p, s));

   int h = 1 + MAX(heightL, heightR);

   invariant(h <= (int)(1.44 * log2((double)s + 2) - 0.328)); // Knuth?

   return h;
}

static void cost_verify(tree_t tree) {
   cost_verify_internal(tree.root, tree.size);
}

const balancer_t balancer_cost = {
   .name    = "Cost",
   .restore = cost_restore,
   .verify  = cost_verify,
};


// ─────────────────────────────────────────────────────────────────────────────
// DSW (Day-Stout-Warren)
// ─────────────────────────────────────────────────────────────────────────────

static node_t *dsw_toVine(node_t *p) {
   node_t n = {0};
   node_t *l = &n;
   while (p != NULL) {
      while (p->l != NULL) {
         p = rotateR(p);
      }
      l->r = p;
      l = l->r;
      p = p->r;
   }
   return n.r;
}

static node_t *dsw_compress(node_t *p, u64 k) {
   node_t n = {0};
   node_t *l = &n;
   n.r = p;
   for (; k > 0; k--) {
      l->r = rotateL(p);
      l = l->r;
      p = l->r;
   }
   return n.r;
}

static node_t *dsw_toTree(node_t *vine, u64 size) {
   u64 m = (1ULL << util_log2(size + 1)) - 1;
   node_t *p = dsw_compress(vine, size - m);
   while (m > 1) {
       m = m >> 1;
       p = dsw_compress(p, m);
   }
   return p;
}

static tree_t dsw_restore(tree_t tree) {
   tree.root = dsw_toTree(dsw_toVine(tree.root), tree.size);
   return tree;
}

static void dsw_verify(tree_t tree) {
   if (tree.root == NULL) {
      invariant(tree.size == 0);
      return;
   }
   invariant(height(tree.root) == (int)util_log2(tree.size));
}

const balancer_t balancer_dsw = {
   .name    = "DSW",
   .restore = dsw_restore,
   .verify  = dsw_verify,
};
