#ifndef WEIGHT_H
#define WEIGHT_H

#include "bst.h"

// ─────────────────────────────────────────────────────────────────────────────
// Weight balance criteria
//
// Translated from trees/wbst.go and trees/lbst.go
// ─────────────────────────────────────────────────────────────────────────────

static inline __uint128_t weight_u128_plus_one(u64 x) {
   return (__uint128_t)x + 1;
}

static inline u64 weight_u64_plus_one(u64 x) {
   invariant(x < UINT64_MAX);
   return x + 1;
}

static inline int weight_u128_log2(__uint128_t x) {
   invariant(x > 0);
   u64 high = (u64)(x >> 64);
   if (high != 0) {
      return 64 + (int)util_log2(high);
   }
   return (int)util_log2((u64)x);
}

static inline bool weight_gte_msb_u128(__uint128_t x, __uint128_t y) {
   return y == 0 || (x > 0 && weight_u128_log2(x) >= weight_u128_log2(y));
}

// ThreeTwo (BB[1/4], Δ=3, Γ=2)
static inline bool three_two_isBalanced(u64 x, u64 y) {
   return 3 * weight_u128_plus_one(x) >= weight_u128_plus_one(y);
}
static inline bool three_two_singleRotation(u64 x, u64 y) {
   return 2 * weight_u128_plus_one(x) > weight_u128_plus_one(y);
}
int three_two_maxHeight(u64 s);

static inline int log_balance_maxHeight(u64 s) {
   invariant(s > 0);
   u64 log = util_log2(s);
   invariant(log <= (u64)(INT_MAX / 2));
   u64 bound = size_sum(log, log);
   invariant(bound <= (u64)INT_MAX);
   return (int)bound;
}

// LogSize
static inline bool log_size_isBalanced(u64 x, u64 y) {
   return util_gte_msb(x, y >> 1);
}
static inline bool log_size_singleRotation(u64 x, u64 y) {
   return util_gte_msb(x, y);
}
static inline int log_size_maxHeight(u64 s) {
   return log_balance_maxHeight(s);
}

// LogWeight
static inline bool log_weight_isBalanced(u64 x, u64 y) {
   return weight_gte_msb_u128(weight_u128_plus_one(x),
                              weight_u128_plus_one(y) >> 1);
}
static inline bool log_weight_singleRotation(u64 x, u64 y) {
   return weight_gte_msb_u128(weight_u128_plus_one(x), weight_u128_plus_one(y));
}
static inline int log_weight_maxHeight(u64 s) {
   return log_balance_maxHeight(s);
}

// Function pointer types for weight balance operations
typedef bool (*wb_balanced_fn)(u64, u64);
typedef bool (*wb_single_fn)(u64, u64);
typedef int (*wb_maxheight_fn)(u64);

typedef struct {
   wb_balanced_fn isBalanced;
   wb_single_fn singleRotation;
   wb_maxheight_fn maxHeight;
} weight_balance_t;

static inline void weight_balance_require(weight_balance_t wb) {
   invariant(wb.isBalanced != NULL);
   invariant(wb.singleRotation != NULL);
   invariant(wb.maxHeight != NULL);
}

static inline bool weight_balance_equal(weight_balance_t left,
                                        weight_balance_t right) {
   weight_balance_require(left);
   weight_balance_require(right);
   return left.isBalanced == right.isBalanced &&
          left.singleRotation == right.singleRotation &&
          left.maxHeight == right.maxHeight;
}

static inline void weight_balance_require_equal(weight_balance_t left,
                                                weight_balance_t right) {
   invariant(weight_balance_equal(left, right));
}

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
