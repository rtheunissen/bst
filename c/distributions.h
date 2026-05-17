#ifndef DISTRIBUTIONS_H
#define DISTRIBUTIONS_H

#include "bst.h"
#include "random.h"

// ─────────────────────────────────────────────────────────────────────────────
// Distribution interface
// ─────────────────────────────────────────────────────────────────────────────

typedef struct distribution distribution_t;

struct distribution {
   u64  (*less_than)(distribution_t *self, u64 n);
   void (*destroy)(distribution_t *self);
};

// Factory function type: creates a new distribution instance from a seed.
typedef distribution_t *(*dist_factory_fn)(u64 seed);

// Convenience dispatcher.
static inline u64 dist_less_than(distribution_t *d, u64 n) {
   return d->less_than(d, n);
}

static inline void dist_destroy(distribution_t *d) {
   if (d && d->destroy) d->destroy(d);
}

// ─────────────────────────────────────────────────────────────────────────────
// Distribution factories
// ─────────────────────────────────────────────────────────────────────────────

distribution_t *uniform_new(u64 seed);
distribution_t *normal_new(u64 seed);
distribution_t *skewed_new(u64 seed);
distribution_t *zipf_new(u64 seed);
distribution_t *maximum_new(u64 seed);
distribution_t *minimum_new(u64 seed);
distribution_t *ascending_new(u64 seed);
distribution_t *descending_new(u64 seed);
distribution_t *queue_new(u64 seed);
distribution_t *median_new(u64 seed);

// Returns a Uniform distribution backed by the global PRNG (do NOT destroy).
distribution_t *uniform_from_global(void);

// Distribution name table (parallel to factory array)
extern const char *dist_names[];
extern const dist_factory_fn dist_factories[];
extern const int dist_count;

#endif // DISTRIBUTIONS_H
