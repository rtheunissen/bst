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
typedef void   (*balancer_verify_fn)(tree_t tree);

typedef struct {
   const char          *name;
   balancer_restore_fn  restore;
   balancer_verify_fn   verify;
} balancer_t;


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
// Randomize: randomizes the shape of a tree using a distribution
// ─────────────────────────────────────────────────────────────────────────────

tree_t tree_randomize(tree_t tree, distribution_t *dist);


#endif // BALANCERS_H
