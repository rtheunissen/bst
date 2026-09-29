#ifndef LBST_RELAXED_H
#define LBST_RELAXED_H

#include "bst.h"
#include "list.h"
#include "weight.h"

typedef struct {
   tree_t tree;
   weight_balance_t wb;
} lbst_relaxed_t;

extern const list_vtable_t lbst_relaxed_vtable;

#endif // LBST_RELAXED_H
