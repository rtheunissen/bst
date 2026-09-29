#include "random.h"
#include <math.h>

// ─────────────────────────────────────────────────────────────────────────────
// SplitMix64
// ─────────────────────────────────────────────────────────────────────────────

void splitmix64_seed(splitmix64_t *s64, u64 seed) {
   invariant(s64 != NULL);
   s64->state = seed;
}

u64 splitmix64_uint64(splitmix64_t *s64) {
   invariant(s64 != NULL);
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
   invariant(k > 0);
   invariant(k < 64);
   return (x << k) | (x >> (64 - k));
}

void xoshiro256_require(xoshiro256_t *src) {
   invariant(src != NULL);
   invariant(src->s[0] != 0 || src->s[1] != 0 || src->s[2] != 0 ||
             src->s[3] != 0);
}

void xoshiro256_seed(xoshiro256_t *src, u64 seed) {
   invariant(src != NULL);
   splitmix64_t s64;
   splitmix64_seed(&s64, seed);
   src->s[0] = splitmix64_uint64(&s64);
   src->s[1] = splitmix64_uint64(&s64);
   src->s[2] = splitmix64_uint64(&s64);
   src->s[3] = splitmix64_uint64(&s64);
   xoshiro256_require(src);
}

u64 xoshiro256_uint64(xoshiro256_t *src) {
   xoshiro256_require(src);
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

static xoshiro256_t global_source = {{
    0x910A2DEC89025CC1ULL,
    0xBEEB8DA1658EEC67ULL,
    0xF893A2EEFB32555EULL,
    0x71C18690EE42C90BULL,
}};

void random_seed(u64 seed) {
   xoshiro256_seed(&global_source, seed);
}

u64 random_uint64(void) {
   return xoshiro256_uint64(&global_source);
}

static u64 random_derive_byte(u64 hash, unsigned char byte) {
   hash ^= (u64)byte;
   return hash * 1099511628211ULL;
}

static u64 random_derive_begin(u64 seed) {
   return 1469598103934665603ULL ^ seed;
}

u64 random_derive_seed(u64 seed, const char *label) {
   invariant(label != NULL);
   invariant(label[0] != '\0');

   u64 hash = random_derive_begin(seed);
   for (const unsigned char *p = (const unsigned char *)label; *p != '\0';
        p++) {
      hash = random_derive_byte(hash, *p);
   }
   return hash;
}

u64 random_derive_u64(u64 seed, u64 value) {
   u64 hash = random_derive_begin(seed);
   for (int i = 0; i < 8; i++) {
      hash = random_derive_byte(hash, (unsigned char)(value & 0xff));
      value >>= 8;
   }
   return hash;
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

static u64 random_less_than_threshold(u64 n) {
   invariant(n > 0);
   return (u64)(-n) % n;
}

static __uint128_t random_less_than_scaled_product(u64 n, u64 x) {
   invariant(n > 0);
   return (__uint128_t)n * (__uint128_t)x;
}

u64 random_less_than_src(u64 n, xoshiro256_t *src) {
   invariant(n > 0);
   xoshiro256_require(src);
   // Singleton ranges have only one valid result and intentionally do not
   // advance the source.
   if (n == 1) {
      return 0;
   }

   u64 threshold = random_less_than_threshold(n);
   invariant(threshold < n);
   for (;;) {
      __uint128_t m =
          random_less_than_scaled_product(n, xoshiro256_uint64(src));
      if ((u64)m >= threshold) {
         return (u64)(m >> 64);
      }
   }
}

u64 random_less_than(u64 n) {
   return random_less_than_src(n, &global_source);
}

// ─────────────────────────────────────────────────────────────────────────────
// Float helpers
// ─────────────────────────────────────────────────────────────────────────────

// Uniform double in [0, 1) with 53 bits of precision.
double xoshiro256_float64(xoshiro256_t *src) {
   xoshiro256_require(src);
   double value = (double)(xoshiro256_uint64(src) >> 11) * 0x1.0p-53;
   invariant(value >= 0.0);
   invariant(value < 1.0);
   return value;
}

// Standard normal distribution via Marsaglia polar method.
double xoshiro256_norm_float64(xoshiro256_t *src) {
   xoshiro256_require(src);
   double u, v, s;
   do {
      u = 2.0 * xoshiro256_float64(src) - 1.0;
      v = 2.0 * xoshiro256_float64(src) - 1.0;
      s = u * u + v * v;
   } while (s >= 1.0 || s == 0.0);
   double value = u * sqrt(-2.0 * log(s) / s);
   invariant(isfinite(value));
   return value;
}

// Standard exponential distribution.
double xoshiro256_exp_float64(xoshiro256_t *src) {
   xoshiro256_require(src);
   double u;
   do {
      u = xoshiro256_float64(src);
   } while (u == 0.0);
   double value = -log(u);
   invariant(value > 0.0);
   invariant(isfinite(value));
   return value;
}
