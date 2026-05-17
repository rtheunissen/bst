#include "random.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


// ─────────────────────────────────────────────────────────────────────────────
// SplitMix64
// ─────────────────────────────────────────────────────────────────────────────

void splitmix64_seed(splitmix64_t *s64, u64 seed) {
   s64->state = seed;
}

u64 splitmix64_uint64(splitmix64_t *s64) {
   s64->state += 0x9E3779B97F4A7C15ULL;

   u64 z = s64->state;

   z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
   z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
   z = (z ^ (z >> 31));

   return z;
}


// ─────────────────────────────────────────────────────────────────────────────
// xoshiro256**
// ─────────────────────────────────────────────────────────────────────────────

static inline u64 rotl64(u64 x, int k) {
   return (x << k) | (x >> (64 - k));
}

void xoshiro256_seed(xoshiro256_t *src, u64 seed) {
   splitmix64_t s64;
   splitmix64_seed(&s64, seed);
   src->s[0] = splitmix64_uint64(&s64);
   src->s[1] = splitmix64_uint64(&s64);
   src->s[2] = splitmix64_uint64(&s64);
   src->s[3] = splitmix64_uint64(&s64);
}

u64 xoshiro256_uint64(xoshiro256_t *src) {
   u64 result = rotl64(src->s[1] * 5, 7) * 9;

   u64 t = src->s[1] << 17;

   src->s[2] ^= src->s[0];
   src->s[3] ^= src->s[1];
   src->s[1] ^= src->s[2];
   src->s[0] ^= src->s[3];

   src->s[2] ^= t;
   src->s[3] = rotl64(src->s[3], 45);

   return result;
}


// ─────────────────────────────────────────────────────────────────────────────
// Global PRNG
// ─────────────────────────────────────────────────────────────────────────────

static xoshiro256_t global_source;

void random_seed(u64 seed) {
   xoshiro256_seed(&global_source, seed);
}

u64 random_uint64(void) {
   return xoshiro256_uint64(&global_source);
}

xoshiro256_t *random_global_source(void) {
   return &global_source;
}


// ─────────────────────────────────────────────────────────────────────────────
// LessThan — Lemire's nearly-divisionless method
//
//    - https://news.ycombinator.com/item?id=28396077
//    - https://www.pcg-random.org/posts/bounded-rands.html
//    - https://arxiv.org/pdf/1805.10941.pdf
//    - https://github.com/apple/swift/pull/39143/files
// ─────────────────────────────────────────────────────────────────────────────

u64 random_less_than_src(u64 n, xoshiro256_t *src) {
   __uint128_t m = (__uint128_t)n * xoshiro256_uint64(src);
   u64 a = (u64)(m >> 64);
   u64 f = (u64)m;

   // Optional fast path
   if (f <= (u64)(0 - n)) return a;

   u64 b = (u64)(((__uint128_t)n * xoshiro256_uint64(src)) >> 64);
   u64 c = ((f + b) < f) ? 1 : 0;
   return a + c;
}

u64 random_less_than(u64 n) {
   return random_less_than_src(n, &global_source);
}


// ─────────────────────────────────────────────────────────────────────────────
// Float helpers
// ─────────────────────────────────────────────────────────────────────────────

// Uniform double in [0, 1) with 53 bits of precision.
double xoshiro256_float64(xoshiro256_t *src) {
   return (double)(xoshiro256_uint64(src) >> 11) * 0x1.0p-53;
}

// Standard normal distribution via Marsaglia polar method.
double xoshiro256_norm_float64(xoshiro256_t *src) {
   double u, v, s;
   do {
      u = 2.0 * xoshiro256_float64(src) - 1.0;
      v = 2.0 * xoshiro256_float64(src) - 1.0;
      s = u * u + v * v;
   } while (s >= 1.0 || s == 0.0);
   return u * sqrt(-2.0 * log(s) / s);
}

// Standard exponential distribution.
double xoshiro256_exp_float64(xoshiro256_t *src) {
   double u;
   do {
      u = xoshiro256_float64(src);
   } while (u == 0.0);
   return -log(u);
}
