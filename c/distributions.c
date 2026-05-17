#include "distributions.h"
#include <math.h>
#include <stdlib.h>

static void dist_free(distribution_t *self) {
   free(self);
}


// ─────────────────────────────────────────────────────────────────────────────
// Uniform
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   distribution_t base;
   xoshiro256_t source;
} uniform_dist_t;

static u64 uniform_less_than(distribution_t *self, u64 n) {
   uniform_dist_t *d = (uniform_dist_t *)self;
   if (n == 0) abort();
   return random_less_than_src(n, &d->source);
}

distribution_t *uniform_new(u64 seed) {
   uniform_dist_t *d = (uniform_dist_t *)calloc(1, sizeof(uniform_dist_t));
   d->base.less_than = uniform_less_than;
   d->base.destroy = dist_free;
   xoshiro256_seed(&d->source, seed);
   return &d->base;
}

// Global variant: uses the global PRNG. Do NOT destroy.
static u64 uniform_global_less_than(distribution_t *self, u64 n) {
   (void)self;
   if (n == 0) abort();
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
// Normal
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   distribution_t base;
   xoshiro256_t source;
   double mean;
   double sdev;
} normal_dist_t;

static u64 normal_less_than(distribution_t *self, u64 n) {
   normal_dist_t *d = (normal_dist_t *)self;
   if (n == 0) abort();
   for (;;) {
      double v = xoshiro256_norm_float64(&d->source) * d->sdev + d->mean;
      if (v >= 0.0 && v < 1.0) {
         return (u64)(v * (double)n);
      }
   }
}

distribution_t *normal_new(u64 seed) {
   normal_dist_t *d = (normal_dist_t *)calloc(1, sizeof(normal_dist_t));
   d->base.less_than = normal_less_than;
   d->base.destroy = dist_free;
   xoshiro256_seed(&d->source, seed);
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
   xoshiro256_t source;
   double alpha;
} gamma_state_t;

static void gamma_init(gamma_state_t *g, u64 seed, double alpha) {
   xoshiro256_seed(&g->source, seed);
   g->alpha = alpha;
}

static double gamma_float64(gamma_state_t *g) {
   if (g->alpha == 1.0) {
      return xoshiro256_exp_float64(&g->source);
   }
   double d = g->alpha - 1.0 / 3.0;
   double m = 1.0;
   if (g->alpha < 1.0) {
      d += 1.0;
      m = pow(xoshiro256_float64(&g->source), 1.0 / g->alpha);
   }
   double c = 1.0 / (3.0 * sqrt(d));
   for (;;) {
      double x = xoshiro256_norm_float64(&g->source);
      double v = 1.0 + x * c;
      if (v <= 0.0) {
         continue;
      }
      v = v * v * v;
      double u = xoshiro256_float64(&g->source);
      if (u < 1.0 - 0.0331 * (x * x) * (x * x)) {
         return m * d * v;
      }
      if (log(u) < 0.5 * x * x + d * (1.0 - v + log(v))) {
         return m * d * v;
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
   double x = gamma_float64(gx);
   double y = gamma_float64(gy);
   return x / (x + y);
}

static u64 skewed_less_than(distribution_t *self, u64 n) {
   skewed_dist_t *d = (skewed_dist_t *)self;
   if (n == 0) abort();
   double v = beta_float64(&d->gx, &d->gy);
   return (u64)(ceil(v * (double)(n - 1)));
}

distribution_t *skewed_new(u64 seed) {
   skewed_dist_t *d = (skewed_dist_t *)calloc(1, sizeof(skewed_dist_t));
   d->base.less_than = skewed_less_than;
   d->base.destroy = dist_free;
   gamma_init(&d->gx, seed, 100.0);
   gamma_init(&d->gy, seed, 50.0);
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
   return exp(z->oneminusQ * log(z->v + x)) * z->oneminusQinv;
}

static double zipf_hinv(zipf_dist_t *z, double x) {
   return exp(z->oneminusQinv * log(z->oneminusQ * x)) - z->v;
}

static u64 zipf_uint64(zipf_dist_t *z) {
   double k;
   for (;;) {
      double r = xoshiro256_float64(&z->source);
      double ur = z->hxm + r * z->hx0MinusHxm;
      double x = zipf_hinv(z, ur);
      k = floor(x + 0.5);
      if (k - x <= z->s) break;
      if (ur >= zipf_h(z, k + 0.5) - exp(-log(k + z->v) * z->q)) break;
   }
   return (u64)k;
}

static u64 zipf_less_than(distribution_t *self, u64 n) {
   zipf_dist_t *z = (zipf_dist_t *)self;
   if (n == 0) abort();
   u64 val = zipf_uint64(z);
   return (u64)((double)val / 1000000.0 * (double)(n - 1));
}

distribution_t *zipf_new(u64 seed) {
   zipf_dist_t *z = (zipf_dist_t *)calloc(1, sizeof(zipf_dist_t));
   z->base.less_than = zipf_less_than;
   z->base.destroy = dist_free;
   xoshiro256_seed(&z->source, seed);

   double s = 1.25;
   double v = 1.0;

   z->imax = 1000000.0;
   z->v = v;
   z->q = s;
   z->oneminusQ = 1.0 - s;
   z->oneminusQinv = 1.0 / (1.0 - s);
   z->hxm = zipf_h(z, z->imax + 0.5);
   z->hx0MinusHxm = zipf_h(z, 0.5) - exp(log(z->v) * (-z->q)) - z->hxm;
   z->s = 1.0 - zipf_hinv(z, zipf_h(z, 1.5) - exp(-z->q * log(z->v + 1.0)));

   return &z->base;
}


// ─────────────────────────────────────────────────────────────────────────────
// Maximum: always returns n-1
// ─────────────────────────────────────────────────────────────────────────────

static u64 maximum_less_than(distribution_t *self, u64 n) {
   (void)self;
   if (n == 0) return 0;
   return n - 1;
}

distribution_t *maximum_new(u64 seed) {
   (void)seed;
   distribution_t *d = (distribution_t *)calloc(1, sizeof(distribution_t));
   d->less_than = maximum_less_than;
   d->destroy = dist_free;
   return d;
}


// ─────────────────────────────────────────────────────────────────────────────
// Minimum: always returns 0
// ─────────────────────────────────────────────────────────────────────────────

static u64 minimum_less_than(distribution_t *self, u64 n) {
   (void)self;
   (void)n;
   return 0;
}

distribution_t *minimum_new(u64 seed) {
   (void)seed;
   distribution_t *d = (distribution_t *)calloc(1, sizeof(distribution_t));
   d->less_than = minimum_less_than;
   d->destroy = dist_free;
   return d;
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
   if (d->i >= n) {
      d->i = 0;
      return 0;
   }
   u64 i = d->i;
   d->i++;
   return i;
}

distribution_t *ascending_new(u64 seed) {
   (void)seed;
   ascending_dist_t *d = (ascending_dist_t *)calloc(1, sizeof(ascending_dist_t));
   d->base.less_than = ascending_less_than;
   d->base.destroy = dist_free;
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
   if (d->i > n || d->i == 0) {
      d->i = n;
   }
   d->i--;
   return d->i;
}

distribution_t *descending_new(u64 seed) {
   descending_dist_t *d = (descending_dist_t *)calloc(1, sizeof(descending_dist_t));
   d->base.less_than = descending_less_than;
   d->base.destroy = dist_free;
   d->i = seed;
   return &d->base;
}


// ─────────────────────────────────────────────────────────────────────────────
// Queue: alternates between 0 and n-1
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   distribution_t base;
   xoshiro256_t source;
} queue_dist_t;

static u64 queue_less_than(distribution_t *self, u64 n) {
   queue_dist_t *d = (queue_dist_t *)self;
   if (n == 0) abort();
   return (n - 1) * (xoshiro256_uint64(&d->source) & 1);
}

distribution_t *queue_new(u64 seed) {
   queue_dist_t *d = (queue_dist_t *)calloc(1, sizeof(queue_dist_t));
   d->base.less_than = queue_less_than;
   d->base.destroy = dist_free;
   xoshiro256_seed(&d->source, seed);
   return &d->base;
}


// ─────────────────────────────────────────────────────────────────────────────
// Median: always returns n/2
// ─────────────────────────────────────────────────────────────────────────────

static u64 median_less_than(distribution_t *self, u64 n) {
   (void)self;
   return n / 2;
}

distribution_t *median_new(u64 seed) {
   (void)seed;
   distribution_t *d = (distribution_t *)calloc(1, sizeof(distribution_t));
   d->less_than = median_less_than;
   d->destroy = dist_free;
   return d;
}


// ─────────────────────────────────────────────────────────────────────────────
// Tables for iteration
// ─────────────────────────────────────────────────────────────────────────────

const char *dist_names[] = {
   "Uniform",
   "Normal",
   "Skewed",
   "Zipf",
   "Maximum",
   "Minimum",
   "Median",
   "Ascending",
   "Descending",
   "Queue",
};

const dist_factory_fn dist_factories[] = {
   uniform_new,
   normal_new,
   skewed_new,
   zipf_new,
   maximum_new,
   minimum_new,
   median_new,
   ascending_new,
   descending_new,
   queue_new,
};

const int dist_count = sizeof(dist_factories) / sizeof(dist_factories[0]);
