#ifndef RANK_H
#define RANK_H

#include "bst.h"
#include <limits.h>

// ─────────────────────────────────────────────────────────────────────────────
// Rank-balanced helpers
//
// Translated from trees/rank_balanced.go
// ─────────────────────────────────────────────────────────────────────────────

static inline int rank_of(node_t *p) {
   if (p == NULL) {
      return -1;
   }
   invariant(p->y <= (u64)INT_MAX);
   return (int)p->y;
}

static inline u64 rank_ordinal(int rank) {
   invariant(rank >= -1);
   return rank < 0 ? 0 : size_after_inserting_one((u64)rank);
}

static inline int rankDifference(node_t *parent, node_t *child) {
   invariant(parent != NULL);
   int parent_rank = rank_of(parent);
   int child_rank = rank_of(child);
   invariant(parent_rank >= child_rank);
   u64 parent_ordinal = rank_ordinal(parent_rank);
   u64 child_ordinal = rank_ordinal(child_rank);
   invariant(parent_ordinal >= child_ordinal);
   u64 diff = parent_ordinal - child_ordinal;
   invariant(diff <= (u64)INT_MAX);
   return (int)diff;
}

static inline u64 rank_above(int rank) {
   invariant(rank >= -1);
   invariant(rank < INT_MAX);
   return rank_ordinal(rank);
}

static inline u64 rank_distance(int a, int b) {
   invariant(a >= -1);
   invariant(b >= -1);
   u64 oa = rank_ordinal(a);
   u64 ob = rank_ordinal(b);
   return oa >= ob ? oa - ob : ob - oa;
}

static inline bool rank_at_most_one_above(int rank, int base) {
   invariant(rank >= -1);
   invariant(base >= -1);
   u64 limit = size_after_inserting_one(rank_ordinal(base));
   return rank_ordinal(rank) <= limit;
}

static inline int rank_twice(int rank) {
   invariant(rank >= 0);
   invariant(rank <= INT_MAX / 2);
   u64 twice = size_sum((u64)rank, (u64)rank);
   invariant(twice <= (u64)INT_MAX);
   return (int)twice;
}

static inline int rank_twice_plus_one(int rank) {
   invariant(rank >= 0);
   invariant(rank <= (INT_MAX - 1) / 2);
   u64 twice = size_sum((u64)rank, (u64)rank);
   u64 result = size_after_inserting_one(twice);
   invariant(result <= (u64)INT_MAX);
   return (int)result;
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
   invariant(p != NULL);
   return isZeroChild(p, p->l) || isZeroChild(p, p->r);
}

static inline void promote(node_t *p) {
   invariant(p != NULL);
   invariant(p->y < (u64)INT_MAX);
   p->y = size_after_inserting_one(p->y);
}

static inline void demote(node_t *p) {
   invariant(p != NULL);
   invariant(p->y > 0);
   p->y = size_after_removing_one(p->y);
}

static inline bool isZeroZero(node_t *p) {
   invariant(p != NULL);
   return isZeroChild(p, p->l) && isZeroChild(p, p->r);
}

static inline bool isOneOne(node_t *p) {
   invariant(p != NULL);
   return isOneChild(p, p->l) && isOneChild(p, p->r);
}

static inline bool isTwoTwo(node_t *p) {
   invariant(p != NULL);
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

typedef u64 (*node_rank_fn)(node_t *p);

// Verify rank invariants.
void verifyMaxRankHeap(node_t *p, node_rank_fn rank);
void verifyMaxRankHeapSized(node_t *p, u64 size, node_rank_fn rank);
void verifyRedBlackRanks(node_t *p);
void verifyRedBlackRanksSized(node_t *p, u64 size);
int verifyRedBlackHeight(node_t *p, u64 s);
int verifyAVLRanks(node_t *p, u64 s);
void verifyAVLWeakRanks(node_t *p);
void verifyAVLWeakRanksSized(node_t *p, u64 size);
void verifyAVLWeakHeight(node_t *root, u64 size);
void verifyAVLRelaxedRanks(node_t *p);
void verifyAVLRelaxedRanksSized(node_t *p, u64 size);
void verifyRedBlackRelaxedRanks(node_t *p);
void verifyRedBlackRelaxedRanksSized(node_t *p, u64 size);
void verifyRedBlackRelaxedHeight(node_t *p);
void verifyRedBlackRelaxedHeightSized(node_t *p, u64 size);
void verifyRedBlackRelaxed(node_t *p);
void verifyRedBlackRelaxedSized(node_t *p, u64 size);

#endif // RANK_H
