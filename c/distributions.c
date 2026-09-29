#include "distributions.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef u64 (*dist_less_than_fn)(distribution_t *self, u64 n);
typedef void (*dist_destroy_fn)(distribution_t *self);

static void dist_free(distribution_t *self) {
   free(self);
}

static void *dist_alloc(size_t size) {
   return bst_calloc(1, size);
}

static void dist_init(distribution_t *dist, dist_less_than_fn less_than,
                      dist_destroy_fn destroy) {
   invariant(dist != NULL);
   invariant(less_than != NULL);
   dist->less_than = less_than;
   dist->destroy = destroy;
}

static distribution_t *dist_new_stateless(dist_less_than_fn less_than) {
   distribution_t *dist = (distribution_t *)dist_alloc(sizeof(distribution_t));
   dist_init(dist, less_than, dist_free);
   return dist;
}

static void dist_require_nonzero(u64 n) {
   invariant(n > 0);
}

typedef struct {
   __uint128_t numerator;
   int shift;
} unit_scale_t;

static unit_scale_t scale_unit_product(double v, u64 n) {
   dist_require_nonzero(n);
   invariant(isfinite(v));
   invariant(v >= 0.0 && v < 1.0);
   invariant(sizeof(double) == sizeof(u64));

   u64 bits;
   memcpy(&bits, &v, sizeof(bits));
   invariant((bits >> 63) == 0);

   u64 exponent = (bits >> 52) & 0x7ffULL;
   u64 fraction = bits & ((1ULL << 52) - 1);
   u64 mantissa;
   int shift;
   // v = mantissa / 2^shift for binary64 values in [0, 1).
   if (exponent == 0) {
      mantissa = fraction;
      shift = 1074;
   } else {
      mantissa = (1ULL << 52) | fraction;
      shift = 1075 - (int)exponent;
   }
   invariant(shift > 0);

   return (unit_scale_t){
       .numerator = (__uint128_t)n * mantissa,
       .shift = shift,
   };
}

static u64 scale_unit_floor(double v, u64 n) {
   unit_scale_t scaled = scale_unit_product(v, n);
   if (scaled.shift >= 128) {
      return 0;
   }

   u64 x = (u64)(scaled.numerator >> scaled.shift);
   return x < n ? x : rank_of_last(n);
}

static u64 scale_unit_ceil(double v, u64 n) {
   dist_require_nonzero(n);
   if (n == 1) {
      return 0;
   }

   u64 maximum = rank_of_last(n);
   unit_scale_t scaled = scale_unit_product(v, maximum);
   if (scaled.numerator == 0) {
      return 0;
   }
   if (scaled.shift >= 128) {
      return 1;
   }

   __uint128_t x = scaled.numerator >> scaled.shift;
   __uint128_t mask = (((__uint128_t)1 << scaled.shift) - 1);
   if ((scaled.numerator & mask) != 0) {
      x++;
   }
   invariant(x <= maximum);
   return x < n ? x : maximum;
}

static u64 scale_ratio_floor(u64 numerator, u64 denominator, u64 n) {
   dist_require_nonzero(denominator);
   dist_require_nonzero(n);
   invariant(numerator <= denominator);

   if (n == 1) {
      return 0;
   }

   __uint128_t scaled = (__uint128_t)numerator * (__uint128_t)rank_of_last(n);
   u64 x = (u64)(scaled / (__uint128_t)denominator);
   invariant(x < n);
   return x;
}

#ifdef DEBUG
void dist_debug_verify_scaling(void) {
   double min_unit = nextafter(0.0, 1.0);
   invariant(min_unit > 0.0);
   invariant(scale_unit_floor(min_unit, UINT64_MAX) == 0);
   invariant(scale_unit_ceil(min_unit, UINT64_MAX) == 1);

   double max_unit = nextafter(1.0, 0.0);
   invariant(max_unit < 1.0);
   invariant(scale_unit_floor(max_unit, UINT64_MAX) == UINT64_MAX - 2048);
   invariant(scale_unit_ceil(max_unit, UINT64_MAX) == UINT64_MAX - 2048);

   invariant(scale_unit_floor(0.0, 1) == 0);
   invariant(scale_unit_floor(0.0, UINT64_MAX) == 0);
   invariant(scale_unit_floor(0.5, 11) == 5);
   invariant(scale_unit_floor(0.5, UINT64_MAX) == UINT64_MAX / 2);
   invariant(scale_unit_floor(0.75, 9) == 6);
   invariant(scale_unit_floor(nextafter(0.5, 0.0), 3) == 1);
   invariant(scale_unit_floor(nextafter(0.5, 1.0), 3) == 1);
   invariant(scale_unit_floor(nextafter(1.0, 0.0), 3) == 2);
   invariant(scale_unit_ceil(0.0, 1) == 0);
   invariant(scale_unit_ceil(0.0, 11) == 0);
   invariant(scale_unit_ceil(0.5, 11) == 5);
   invariant(scale_unit_ceil(0.5, UINT64_MAX) == UINT64_MAX / 2);
   invariant(scale_unit_ceil(0.75, 9) == 6);
   invariant(scale_unit_ceil(nextafter(0.5, 0.0), 3) == 1);
   invariant(scale_unit_ceil(nextafter(0.5, 1.0), 3) == 2);
   invariant(scale_unit_ceil(nextafter(1.0, 0.0), 3) == 2);
   invariant(scale_ratio_floor(0, 1000000, UINT64_MAX) == 0);
   invariant(scale_ratio_floor(1000000, 1000000, UINT64_MAX) == UINT64_MAX - 1);
   invariant(scale_ratio_floor(500000, 1000000, 11) == 5);
   invariant(scale_ratio_floor(1, 3, 10) == 3);
   invariant(scale_ratio_floor(1, 1, 1) == 0);
   invariant(scale_ratio_floor(1, 2, UINT64_MAX) == UINT64_MAX / 2);
   invariant(scale_ratio_floor(1, UINT64_MAX, UINT64_MAX) == 0);
   invariant(scale_ratio_floor(UINT64_MAX - 1, UINT64_MAX, UINT64_MAX) ==
             UINT64_MAX - 2);
   invariant(scale_ratio_floor(UINT64_MAX, UINT64_MAX, UINT64_MAX) ==
             UINT64_MAX - 1);
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Uniform
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   distribution_t base;
   xoshiro256_t source;
} uniform_dist_t;

static u64 uniform_less_than(distribution_t *self, u64 n) {
   uniform_dist_t *d = (uniform_dist_t *)self;
   dist_require_nonzero(n);
   return random_less_than_src(n, &d->source);
}

distribution_t *uniform_new(u64 seed) {
   uniform_dist_t *d = (uniform_dist_t *)dist_alloc(sizeof(uniform_dist_t));
   dist_init(&d->base, uniform_less_than, dist_free);
   xoshiro256_seed(&d->source, seed);
   return &d->base;
}

// Global variant: uses the global PRNG. Do NOT destroy.
static u64 uniform_global_less_than(distribution_t *self, u64 n) {
   (void)self;
   dist_require_nonzero(n);
   return random_less_than(n);
}

static distribution_t global_uniform_instance = {
    .less_than = uniform_global_less_than,
    .destroy = NULL,
};

distribution_t *uniform_from_global(void) {
   return &global_uniform_instance;
}

// ─────────────────────────────────────────────────────────────────────────────
// Sample source
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   xoshiro256_t rng;
   bool has_normal_spare;
   double normal_spare;
} sample_source_t;

static void sample_source_seed(sample_source_t *source, u64 seed) {
   invariant(source != NULL);
   xoshiro256_seed(&source->rng, seed);
   xoshiro256_require(&source->rng);
   source->has_normal_spare = false;
   source->normal_spare = 0.0;
}

static void sample_source_require(sample_source_t *source) {
   invariant(source != NULL);
   xoshiro256_require(&source->rng);
   if (source->has_normal_spare) {
      invariant(isfinite(source->normal_spare));
   } else {
      invariant(source->normal_spare == 0.0);
   }
}

static double sample_source_normal_float64(sample_source_t *source) {
   sample_source_require(source);
   if (source->has_normal_spare) {
      source->has_normal_spare = false;
      double spare = source->normal_spare;
      source->normal_spare = 0.0;
      invariant(isfinite(spare));
      return spare;
   }

   double u, v, s;
   do {
      u = 2.0 * xoshiro256_float64(&source->rng) - 1.0;
      v = 2.0 * xoshiro256_float64(&source->rng) - 1.0;
      s = u * u + v * v;
   } while (s >= 1.0 || s == 0.0);

   double factor = sqrt(-2.0 * log(s) / s);
   double first = u * factor;
   double second = v * factor;
   invariant(isfinite(first));
   invariant(isfinite(second));

   source->normal_spare = second;
   source->has_normal_spare = true;
   return first;
}

static double sample_source_unit_open(sample_source_t *source) {
   sample_source_require(source);
   double u;
   do {
      u = xoshiro256_float64(&source->rng);
   } while (u == 0.0);
   invariant(u > 0.0 && u < 1.0);
   return u;
}

static double sample_source_exp_float64(sample_source_t *source) {
   double value = -log(sample_source_unit_open(source));
   invariant(value > 0.0);
   invariant(isfinite(value));
   return value;
}

// ─────────────────────────────────────────────────────────────────────────────
// Normal
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   distribution_t base;
   sample_source_t source;
   double mean;
   double sdev;
} normal_dist_t;

static u64 normal_less_than(distribution_t *self, u64 n) {
   normal_dist_t *d = (normal_dist_t *)self;
   dist_require_nonzero(n);
   if (n == 1) {
      return 0;
   }
   for (;;) {
      double v = sample_source_normal_float64(&d->source) * d->sdev + d->mean;
      if (v >= 0.0 && v < 1.0) {
         return scale_unit_floor(v, n);
      }
   }
}

distribution_t *normal_new(u64 seed) {
   normal_dist_t *d = (normal_dist_t *)dist_alloc(sizeof(normal_dist_t));
   dist_init(&d->base, normal_less_than, dist_free);
   sample_source_seed(&d->source, seed);
   d->mean = 0.50;
   d->sdev = 0.15;
   return &d->base;
}

// ─────────────────────────────────────────────────────────────────────────────
// Gamma (helper for Skewed/Beta)
//
// Marsaglia, George, and Wai Wan Tsang. "A simple method for generating
// gamma variables." ACM TOMS 26.3 (2000): 363-372.
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   sample_source_t source;
   double alpha;
} gamma_state_t;

static void gamma_init(gamma_state_t *g, u64 seed, double alpha) {
   invariant(g != NULL);
   invariant(alpha > 0.0);
   invariant(isfinite(alpha));
   sample_source_seed(&g->source, seed);
   g->alpha = alpha;
}

static double gamma_float64(gamma_state_t *g) {
   invariant(g != NULL);
   invariant(g->alpha > 0.0);
   invariant(isfinite(g->alpha));

   if (g->alpha == 1.0) {
      return sample_source_exp_float64(&g->source);
   }
   double d = g->alpha - 1.0 / 3.0;
   double m = 1.0;
   if (g->alpha < 1.0) {
      d += 1.0;
      m = pow(sample_source_unit_open(&g->source), 1.0 / g->alpha);
   }
   double c = 1.0 / (3.0 * sqrt(d));
   for (;;) {
      double x = sample_source_normal_float64(&g->source);
      double v = 1.0 + x * c;
      if (v <= 0.0) {
         continue;
      }
      v = v * v * v;
      double u = sample_source_unit_open(&g->source);
      if (u < 1.0 - 0.0331 * (x * x) * (x * x)) {
         double value = m * d * v;
         invariant(value >= 0.0);
         invariant(isfinite(value));
         return value;
      }
      if (log(u) < 0.5 * x * x + d * (1.0 - v + log(v))) {
         double value = m * d * v;
         invariant(value >= 0.0);
         invariant(isfinite(value));
         return value;
      }
   }
}

// ─────────────────────────────────────────────────────────────────────────────
// Skewed (Beta(100, 50) distribution)
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   distribution_t base;
   gamma_state_t gx;
   gamma_state_t gy;
} skewed_dist_t;

static double beta_float64(gamma_state_t *gx, gamma_state_t *gy) {
   invariant(gx != NULL);
   invariant(gy != NULL);

   for (;;) {
      double x = gamma_float64(gx);
      double y = gamma_float64(gy);
      invariant(x >= 0.0);
      invariant(y >= 0.0);
      invariant(isfinite(x));
      invariant(isfinite(y));

      double sum = x + y;
      invariant(isfinite(sum));
      if (sum == 0.0) {
         continue;
      }

      double v = x / sum;
      invariant(v >= 0.0 && v <= 1.0);
      invariant(isfinite(v));
      if (v < 1.0) {
         return v;
      }
   }
}

static u64 skewed_less_than(distribution_t *self, u64 n) {
   skewed_dist_t *d = (skewed_dist_t *)self;
   dist_require_nonzero(n);
   if (n == 1) {
      return 0;
   }
   double v = beta_float64(&d->gx, &d->gy);
   return scale_unit_ceil(v, n);
}

distribution_t *skewed_new(u64 seed) {
   skewed_dist_t *d = (skewed_dist_t *)dist_alloc(sizeof(skewed_dist_t));
   dist_init(&d->base, skewed_less_than, dist_free);

   splitmix64_t seeds;
   splitmix64_seed(&seeds, seed);
   gamma_init(&d->gx, splitmix64_uint64(&seeds), 100.0);
   gamma_init(&d->gy, splitmix64_uint64(&seeds), 50.0);
   return &d->base;
}

// ─────────────────────────────────────────────────────────────────────────────
// Zipf
//
// Based on W. Hörmann and G. Derflinger, "Rejection-Inversion to Generate
// Variates from Monotone Discrete Distributions", ACM TOMS, 22(4), 1996.
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   distribution_t base;
   xoshiro256_t source;
   double imax;
   double v;
   double q;
   double oneminusQ;
   double oneminusQinv;
   double hxm;
   double hx0MinusHxm;
   double s;
} zipf_dist_t;

static double zipf_h(zipf_dist_t *z, double x) {
   invariant(z != NULL);
   invariant(z->v + x > 0.0);
   double value = exp(z->oneminusQ * log(z->v + x)) * z->oneminusQinv;
   invariant(isfinite(value));
   return value;
}

static double zipf_hinv(zipf_dist_t *z, double x) {
   invariant(z != NULL);
   invariant(z->oneminusQ * x > 0.0);
   double value = exp(z->oneminusQinv * log(z->oneminusQ * x)) - z->v;
   invariant(isfinite(value));
   return value;
}

static u64 zipf_uint64(zipf_dist_t *z) {
   invariant(z != NULL);
   invariant(z->imax >= 1.0);
   xoshiro256_require(&z->source);

   double k;
   for (;;) {
      double r = xoshiro256_float64(&z->source);
      double ur = z->hxm + r * z->hx0MinusHxm;
      double x = zipf_hinv(z, ur);
      k = floor(x + 0.5);
      if (k - x <= z->s)
         break;
      if (ur >= zipf_h(z, k + 0.5) - exp(-log(k + z->v) * z->q))
         break;
   }
   invariant(k >= 0.0);
   invariant(k <= z->imax);
   return (u64)k;
}

static u64 zipf_less_than(distribution_t *self, u64 n) {
   zipf_dist_t *z = (zipf_dist_t *)self;
   dist_require_nonzero(n);
   if (n == 1) {
      return 0;
   }
   u64 val = zipf_uint64(z);
   return scale_ratio_floor(val, (u64)z->imax, n);
}

distribution_t *zipf_new(u64 seed) {
   zipf_dist_t *z = (zipf_dist_t *)dist_alloc(sizeof(zipf_dist_t));
   dist_init(&z->base, zipf_less_than, dist_free);
   xoshiro256_seed(&z->source, seed);

   double s = 1.25;
   double v = 1.0;

   z->imax = 1000000.0;
   z->v = v;
   z->q = s;
   z->oneminusQ = 1.0 - s;
   z->oneminusQinv = 1.0 / (1.0 - s);
   invariant(z->imax >= 1.0);
   invariant(z->v > 0.0);
   invariant(z->q > 1.0);
   invariant(z->oneminusQ < 0.0);
   invariant(z->oneminusQinv < 0.0);
   z->hxm = zipf_h(z, z->imax + 0.5);
   z->hx0MinusHxm = zipf_h(z, 0.5) - exp(log(z->v) * (-z->q)) - z->hxm;
   z->s = 1.0 - zipf_hinv(z, zipf_h(z, 1.5) - exp(-z->q * log(z->v + 1.0)));
   invariant(isfinite(z->hxm));
   invariant(isfinite(z->hx0MinusHxm));
   invariant(isfinite(z->s));
   invariant(z->hxm < 0.0);
   invariant(z->hx0MinusHxm < 0.0);
   invariant(z->hxm + z->hx0MinusHxm < 0.0);

   return &z->base;
}

// ─────────────────────────────────────────────────────────────────────────────
// Maximum: always returns n-1
// ─────────────────────────────────────────────────────────────────────────────

static u64 maximum_less_than(distribution_t *self, u64 n) {
   (void)self;
   dist_require_nonzero(n);
   return rank_of_last(n);
}

distribution_t *maximum_new(u64 seed) {
   (void)seed;
   return dist_new_stateless(maximum_less_than);
}

// ─────────────────────────────────────────────────────────────────────────────
// Minimum: always returns 0
// ─────────────────────────────────────────────────────────────────────────────

static u64 minimum_less_than(distribution_t *self, u64 n) {
   (void)self;
   dist_require_nonzero(n);
   return 0;
}

distribution_t *minimum_new(u64 seed) {
   (void)seed;
   return dist_new_stateless(minimum_less_than);
}

// ─────────────────────────────────────────────────────────────────────────────
// Ascending: 0, 1, 2, ..., wrapping at n
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   distribution_t base;
   u64 i;
} ascending_dist_t;

static u64 ascending_less_than(distribution_t *self, u64 n) {
   ascending_dist_t *d = (ascending_dist_t *)self;
   dist_require_nonzero(n);
   if (n == 1) {
      return 0;
   }
   if (d->i >= n) {
      d->i = 0;
   }
   u64 i = d->i;
   d->i = i == rank_of_last(n) ? 0 : size_after_inserting_one(i);
   return i;
}

distribution_t *ascending_new(u64 seed) {
   (void)seed;
   ascending_dist_t *d =
       (ascending_dist_t *)dist_alloc(sizeof(ascending_dist_t));
   dist_init(&d->base, ascending_less_than, dist_free);
   d->i = 0;
   return &d->base;
}

// ─────────────────────────────────────────────────────────────────────────────
// Descending: n-1, n-2, ..., wrapping at 0
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   distribution_t base;
   u64 i;
} descending_dist_t;

static u64 descending_less_than(distribution_t *self, u64 n) {
   descending_dist_t *d = (descending_dist_t *)self;
   dist_require_nonzero(n);
   if (n == 1) {
      return 0;
   }
   if (d->i > n || d->i == 0) {
      d->i = n;
   }
   d->i = size_after_removing_one(d->i);
   return d->i;
}

distribution_t *descending_new(u64 seed) {
   (void)seed;
   descending_dist_t *d =
       (descending_dist_t *)dist_alloc(sizeof(descending_dist_t));
   dist_init(&d->base, descending_less_than, dist_free);
   d->i = 0;
   return &d->base;
}

// ─────────────────────────────────────────────────────────────────────────────
// Queue: alternates between 0 and n-1
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   distribution_t base;
   bool tail;
} queue_dist_t;

static u64 queue_less_than(distribution_t *self, u64 n) {
   queue_dist_t *d = (queue_dist_t *)self;
   dist_require_nonzero(n);
   if (n == 1) {
      return 0;
   }
   u64 value = d->tail ? rank_of_last(n) : 0;
   d->tail = !d->tail;
   return value;
}

distribution_t *queue_new(u64 seed) {
   (void)seed;
   queue_dist_t *d = (queue_dist_t *)dist_alloc(sizeof(queue_dist_t));
   dist_init(&d->base, queue_less_than, dist_free);
   d->tail = false;
   return &d->base;
}

// ─────────────────────────────────────────────────────────────────────────────
// Median: always returns n/2
// ─────────────────────────────────────────────────────────────────────────────

static u64 median_less_than(distribution_t *self, u64 n) {
   (void)self;
   dist_require_nonzero(n);
   return n / 2;
}

distribution_t *median_new(u64 seed) {
   (void)seed;
   return dist_new_stateless(median_less_than);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tables for iteration
// ─────────────────────────────────────────────────────────────────────────────

static const dist_spec_t dist_spec_uniform = {"Uniform", uniform_new};
static const dist_spec_t dist_spec_normal = {"Normal", normal_new};
static const dist_spec_t dist_spec_skewed = {"Skewed", skewed_new};
static const dist_spec_t dist_spec_zipf = {"Zipf", zipf_new};
static const dist_spec_t dist_spec_maximum = {"Maximum", maximum_new};
static const dist_spec_t dist_spec_minimum = {"Minimum", minimum_new};
static const dist_spec_t dist_spec_median = {"Median", median_new};
static const dist_spec_t dist_spec_ascending = {"Ascending", ascending_new};
static const dist_spec_t dist_spec_descending = {"Descending", descending_new};
static const dist_spec_t dist_spec_queue = {"Queue", queue_new};

const dist_spec_t *const dist_all[] = {
    &dist_spec_uniform, &dist_spec_normal,    &dist_spec_skewed,
    &dist_spec_zipf,    &dist_spec_maximum,   &dist_spec_minimum,
    &dist_spec_median,  &dist_spec_ascending, &dist_spec_descending,
    &dist_spec_queue,
};

const int dist_count = sizeof(dist_all) / sizeof(dist_all[0]);

const dist_spec_t *const dist_benchmark[] = {
    &dist_spec_uniform, &dist_spec_normal,  &dist_spec_skewed,
    &dist_spec_zipf,    &dist_spec_maximum,
};

const int dist_benchmark_count =
    sizeof(dist_benchmark) / sizeof(dist_benchmark[0]);

const dist_spec_t *const dist_uniform[] = {
    &dist_spec_uniform,
};

const int dist_uniform_count = sizeof(dist_uniform) / sizeof(dist_uniform[0]);

const dist_spec_t *const dist_animation[] = {
    &dist_spec_uniform,    &dist_spec_normal,  &dist_spec_skewed,
    &dist_spec_zipf,       &dist_spec_minimum, &dist_spec_maximum,
    &dist_spec_median,     &dist_spec_queue,   &dist_spec_ascending,
    &dist_spec_descending,
};

const int dist_animation_count =
    sizeof(dist_animation) / sizeof(dist_animation[0]);

static void dist_specs_require_slice(const dist_spec_t *const *specs,
                                     int count) {
   invariant(count >= 0);
   invariant(count == 0 || specs != NULL);
   for (int i = 0; i < count; i++) {
      const dist_spec_t *spec = specs[i];
      dist_spec_require(spec);
      for (int j = i + 1; j < count; j++) {
         const dist_spec_t *other = specs[j];
         dist_spec_require(other);
         invariant(spec != other);
         invariant(strcmp(spec->name, other->name) != 0);
      }
   }
}

static bool dist_all_contains_spec(const dist_spec_t *target) {
   dist_spec_require(target);
   for (int i = 0; i < dist_count; i++) {
      const dist_spec_t *spec = dist_all[i];
      dist_spec_require(spec);
      if (spec == target) {
         return true;
      }
   }
   return false;
}

static void dist_specs_require_registered_slice(const dist_spec_t *const *specs,
                                                int count) {
   dist_specs_require_slice(specs, count);
   for (int i = 0; i < count; i++) {
      invariant(dist_all_contains_spec(specs[i]));
   }
}

static void dist_registry_require(void) {
   invariant(dist_count > 0);
   dist_specs_require_slice(dist_all, dist_count);
   dist_specs_require_registered_slice(dist_benchmark, dist_benchmark_count);
   dist_specs_require_registered_slice(dist_uniform, dist_uniform_count);
   dist_specs_require_registered_slice(dist_animation, dist_animation_count);
}

static bool dist_name_is_lookupable(const char *name) {
   return name != NULL && name[0] != '\0' && dist_name_is_data_token(name);
}

static const dist_spec_t *dist_find_lookupable(const char *name) {
   invariant(dist_name_is_lookupable(name));
   dist_registry_require();
   for (int i = 0; i < dist_count; i++) {
      const dist_spec_t *spec = dist_all[i];
      dist_spec_require(spec);
      if (strcmp(name, spec->name) == 0) {
         return spec;
      }
   }
   return NULL;
}

const dist_spec_t *dist_find(const char *name) {
   if (!dist_name_is_lookupable(name)) {
      return NULL;
   }
   return dist_find_lookupable(name);
}

const dist_spec_t *dist_find_or_exit(const char *name) {
   if (name == NULL) {
      fprintf(stderr, "distribution name is null\n");
      dist_fprint_names(stderr, "distributions", dist_all, dist_count);
      exit(2);
   }
   if (name[0] == '\0') {
      fprintf(stderr, "distribution name is empty\n");
      dist_fprint_names(stderr, "distributions", dist_all, dist_count);
      exit(2);
   }
   if (!dist_name_is_data_token(name)) {
      fprintf(stderr, "distribution name is not a printable token\n");
      dist_fprint_names(stderr, "distributions", dist_all, dist_count);
      exit(2);
   }

   const dist_spec_t *spec = dist_find_lookupable(name);
   if (spec == NULL) {
      fprintf(stderr, "unknown distribution: %s\n", name);
      dist_fprint_names(stderr, "distributions", dist_all, dist_count);
      exit(2);
   }
   return spec;
}

void dist_benchmark_selection_init(dist_selection_t *selection,
                                   const char *name) {
   invariant(selection != NULL);
   *selection = (dist_selection_t){0};

   if (name == NULL) {
      dist_specs_require_registered_slice(dist_benchmark, dist_benchmark_count);
      selection->items = dist_benchmark;
      selection->count = dist_benchmark_count;
      invariant(selection->count > 0);
      return;
   }

   selection->selected[0] = dist_find_or_exit(name);
   selection->items = selection->selected;
   selection->count = 1;
}

void dist_fprint_names(FILE *file, const char *label,
                       const dist_spec_t *const *specs, int count) {
   invariant(file != NULL);
   invariant(label != NULL);
   dist_specs_require_slice(specs, count);

   fprintf(file, "%s:", label);
   for (int i = 0; i < count; i++) {
      const dist_spec_t *spec = specs[i];
      dist_spec_require(spec);
      fprintf(file, " %s", spec->name);
   }
   fprintf(file, "\n");
}
