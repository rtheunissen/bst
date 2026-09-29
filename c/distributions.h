#ifndef DISTRIBUTIONS_H
#define DISTRIBUTIONS_H

#include "bst.h"
#include "random.h"

#include <stdio.h>

// ─────────────────────────────────────────────────────────────────────────────
// Distribution interface
// ─────────────────────────────────────────────────────────────────────────────

typedef struct distribution distribution_t;

struct distribution {
   u64 (*less_than)(distribution_t *self, u64 n);
   void (*destroy)(distribution_t *self);
};

// Factory function type: creates a new distribution instance from a seed.
typedef distribution_t *(*dist_factory_fn)(u64 seed);

typedef struct {
   const char *name;
   dist_factory_fn factory;
} dist_spec_t;

static inline bool dist_name_is_data_token(const char *name) {
   return bst_name_is_printable_token(name);
}

static inline void dist_spec_require(const dist_spec_t *spec) {
   invariant(spec != NULL);
   invariant(spec->name != NULL);
   invariant(spec->name[0] != '\0');
   invariant(dist_name_is_data_token(spec->name));
   invariant(spec->factory != NULL);
}

static inline void dist_require(distribution_t *d) {
   invariant(d != NULL);
   invariant(d->less_than != NULL);
}

static inline distribution_t *dist_create(const dist_spec_t *spec, u64 seed) {
   invariant(spec != NULL);
   dist_spec_require(spec);
   dist_factory_fn factory = spec->factory;
   invariant(factory != NULL);
   distribution_t *d = factory(seed);
   dist_require(d);
   return d;
}

// Convenience dispatcher.
static inline u64 dist_less_than(distribution_t *d, u64 n) {
   dist_require(d);
   invariant(n > 0);
   // Match random_less_than_src: singleton bounds are deterministic and do not
   // advance distribution state.
   if (n == 1) {
      return 0;
   }
   u64 x = d->less_than(d, n);
   invariant(x < n);
   return x;
}

static inline void dist_destroy(distribution_t *d) {
   if (d == NULL) {
      return;
   }
   dist_require(d);
   if (d->destroy) {
      d->destroy(d);
   }
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

// Returns a process-lifetime Uniform distribution backed by the global PRNG.
// Calling dist_destroy on it is a no-op.
distribution_t *uniform_from_global(void);

extern const dist_spec_t *const dist_all[];
extern const int dist_count;

extern const dist_spec_t *const dist_benchmark[];
extern const int dist_benchmark_count;

extern const dist_spec_t *const dist_uniform[];
extern const int dist_uniform_count;

extern const dist_spec_t *const dist_animation[];
extern const int dist_animation_count;

typedef struct {
   const dist_spec_t *selected[1];
   const dist_spec_t *const *items;
   int count;
} dist_selection_t;

const dist_spec_t *dist_find(const char *name);
const dist_spec_t *dist_find_or_exit(const char *name);
void dist_benchmark_selection_init(dist_selection_t *selection,
                                   const char *name);
void dist_fprint_names(FILE *file, const char *label,
                       const dist_spec_t *const *specs, int count);

#ifdef DEBUG
void dist_debug_verify_scaling(void);
#endif

#endif // DISTRIBUTIONS_H
