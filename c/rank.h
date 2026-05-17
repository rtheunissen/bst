#ifndef RANK_H
#define RANK_H

#include "bst.h"

// ─────────────────────────────────────────────────────────────────────────────
// Rank-balanced helpers
//
// Translated from trees/rank_balanced.go
// ─────────────────────────────────────────────────────────────────────────────

static inline int rank_of(node_t *p) {
   if (p == NULL) {
      return -1;
   } else {
      return (int)p->y;
   }
}

static inline int rankDifference(node_t *parent, node_t *child) {
   assert_bst(rank_of(parent) >= rank_of(child));
   return rank_of(parent) - rank_of(child);
}

static inline bool hasZeroChild(node_t *p);

static inline bool isZeroChild(node_t *parent, node_t *child) {
   return rankDifference(parent, child) == 0;
}

static inline bool isOneChild(node_t *parent, node_t *child) {
   return rankDifference(parent, child) == 1;
}

static inline bool isTwoChild(node_t *parent, node_t *child) {
   return rankDifference(parent, child) == 2;
}

static inline bool isThreeChild(node_t *parent, node_t *child) {
   return rankDifference(parent, child) == 3;
}

static inline bool hasZeroChild(node_t *p) {
   return isZeroChild(p, p->l) || isZeroChild(p, p->r);
}

static inline void promote(node_t *p) {
   p->y++;
}

static inline void demote(node_t *p) {
   p->y--;
}

static inline bool isZeroZero(node_t *p) {
   return isZeroChild(p, p->l) && isZeroChild(p, p->r);
}

static inline bool isOneOne(node_t *p) {
   return isOneChild(p, p->l) && isOneChild(p, p->r);
}

static inline bool isTwoTwo(node_t *p) {
   return isTwoChild(p, p->l) && isTwoChild(p, p->r);
}


// ─────────────────────────────────────────────────────────────────────────────
// Red-black helpers (from trees/redblack.go)
// ─────────────────────────────────────────────────────────────────────────────

static inline bool isRed(node_t *parent, node_t *child) {
   return isZeroChild(parent, child);
}

static inline bool isRedRed(node_t *parent) {
   return isZeroZero(parent);
}

static inline bool isBlack(node_t *parent, node_t *child) {
   return isOneChild(parent, child);
}

// Verify rank invariants (recursive).
void verifyRedBlackRanks(node_t *p);
int  verifyRedBlackHeight(node_t *p, u64 s);
int  verifyAVLRanks(node_t *p, u64 s);
void verifyAVLWeakRanks(node_t *p);
void verifyAVLWeakHeight(node_t *root, u64 size);
void verifyAVLRelaxedRanks(node_t *p);
void verifyRedBlackRelaxedRanks(node_t *p);
void verifyRedBlackRelaxedHeight(node_t *p);

#endif // RANK_H
