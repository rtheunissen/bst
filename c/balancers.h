#ifndef BALANCERS_H
#define BALANCERS_H

#include "bst.h"
#include "distributions.h"

// ─────────────────────────────────────────────────────────────────────────────
// Balancer interface
//
// Each balancer provides:
//   restore: rebalances a tree according to its invariant, returns the new tree
//   verify:  checks that the tree satisfies the balancer's invariant
// ─────────────────────────────────────────────────────────────────────────────

typedef tree_t (*balancer_restore_fn)(tree_t tree);
typedef void (*balancer_verify_fn)(tree_t tree);

typedef struct {
   const char *name;
   balancer_restore_fn restore;
   balancer_verify_fn verify;
} balancer_t;

static inline bool balancer_name_is_component(const char *name) {
   return bst_name_is_path_component(name);
}

static inline void balancer_require(const balancer_t *balancer) {
   invariant(balancer != NULL);
   invariant(balancer_name_is_component(balancer->name));
   invariant(balancer->restore != NULL);
   invariant(balancer->verify != NULL);
}

static inline void balancer_verify_tree(const balancer_t *balancer,
                                        tree_t tree) {
   balancer_require(balancer);
   tree_sized_handle_require(tree);
   balancer->verify(tree);
   tree_sized_handle_require(tree);
}

// ─────────────────────────────────────────────────────────────────────────────
// Balancer instances
// ─────────────────────────────────────────────────────────────────────────────

extern const balancer_t balancer_median;
extern const balancer_t balancer_height;
extern const balancer_t balancer_weight;
extern const balancer_t balancer_log;
extern const balancer_t balancer_cost;
extern const balancer_t balancer_dsw;

// ─────────────────────────────────────────────────────────────────────────────
// Randomize: randomizes the shape of a tree using a distribution.
// Mutates and returns the input tree; persistent builds require exclusive,
// unshared node ownership.
// ─────────────────────────────────────────────────────────────────────────────

tree_t tree_randomize(tree_t tree, distribution_t *dist);

#endif // BALANCERS_H
