#ifndef RANDOM_H
#define RANDOM_H

#include "bst.h"

// ─────────────────────────────────────────────────────────────────────────────
// xoshiro256** PRNG
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   u64 s[4];
} xoshiro256_t;

void xoshiro256_seed(xoshiro256_t *src, u64 seed);
u64  xoshiro256_uint64(xoshiro256_t *src);


// ─────────────────────────────────────────────────────────────────────────────
// SplitMix64 (used for seeding xoshiro256)
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   u64 state;
} splitmix64_t;

void splitmix64_seed(splitmix64_t *s64, u64 seed);
u64  splitmix64_uint64(splitmix64_t *s64);


// ─────────────────────────────────────────────────────────────────────────────
// Global PRNG
// ─────────────────────────────────────────────────────────────────────────────

void random_seed(u64 seed);
u64  random_uint64(void);

// Returns a pointer to the global PRNG source.
xoshiro256_t *random_global_source(void);


// ─────────────────────────────────────────────────────────────────────────────
// Bounded random: unbiased random in [0, n)
// Uses Lemire's nearly-divisionless method.
//
//    - https://arxiv.org/pdf/1805.10941.pdf
//    - https://www.pcg-random.org/posts/bounded-rands.html
// ─────────────────────────────────────────────────────────────────────────────

u64 random_less_than_src(u64 n, xoshiro256_t *src);
u64 random_less_than(u64 n);


// ─────────────────────────────────────────────────────────────────────────────
// Float helpers (for distributions)
// ─────────────────────────────────────────────────────────────────────────────

// Uniform double in [0, 1) with 53 bits of precision.
double xoshiro256_float64(xoshiro256_t *src);

// Standard normal distribution (Box-Muller).
double xoshiro256_norm_float64(xoshiro256_t *src);

// Standard exponential distribution.
double xoshiro256_exp_float64(xoshiro256_t *src);

#endif // RANDOM_H
