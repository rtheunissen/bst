#ifndef WEIGHT_H
#define WEIGHT_H

#include "bst.h"
#include <math.h>

// ─────────────────────────────────────────────────────────────────────────────
// Weight balance criteria
//
// Translated from trees/wbst.go and trees/lbst.go
// ─────────────────────────────────────────────────────────────────────────────

// ThreeTwo (BB[1/4], Δ=3, Γ=2)
static inline bool three_two_isBalanced(u64 x, u64 y) {
   return 3 * (x + 1) >= (y + 1);
}
static inline bool three_two_singleRotation(u64 x, u64 y) {
   return 2 * (x + 1) > (y + 1);
}
static inline int three_two_maxHeight(u64 s) {
   return (int)(log((double)s + 1) / log(4.0 / 3.0));
}

// LogSize
static inline bool log_size_isBalanced(u64 x, u64 y) {
   return util_gte_msb(x, y >> 1);
}
static inline bool log_size_singleRotation(u64 x, u64 y) {
   return util_gte_msb(x, y);
}
static inline int log_size_maxHeight(u64 s) {
   return (int)(2 * util_log2(s));
}

// LogWeight
static inline bool log_weight_isBalanced(u64 x, u64 y) {
   return util_gte_msb(x + 1, (y + 1) >> 1);
}
static inline bool log_weight_singleRotation(u64 x, u64 y) {
   return util_gte_msb(x + 1, y + 1);
}
static inline int log_weight_maxHeight(u64 s) {
   return (int)(2 * util_log2(s));
}

// Function pointer types for weight balance operations
typedef bool (*wb_balanced_fn)(u64, u64);
typedef bool (*wb_single_fn)(u64, u64);
typedef int  (*wb_maxheight_fn)(u64);

typedef struct {
   wb_balanced_fn  isBalanced;
   wb_single_fn    singleRotation;
   wb_maxheight_fn maxHeight;
} weight_balance_t;

static const weight_balance_t wb_three_two = {
   three_two_isBalanced,
   three_two_singleRotation,
   three_two_maxHeight,
};

static const weight_balance_t wb_log_size = {
   log_size_isBalanced,
   log_size_singleRotation,
   log_size_maxHeight,
};

static const weight_balance_t wb_log_weight = {
   log_weight_isBalanced,
   log_weight_singleRotation,
   log_weight_maxHeight,
};

#endif // WEIGHT_H
