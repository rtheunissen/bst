#ifndef LBST_TOPDOWN_H
#define LBST_TOPDOWN_H

#include "bst.h"
#include "list.h"
#include "weight.h"

typedef struct {
   tree_t tree;
   weight_balance_t wb;
} lbst_topdown_t;

extern const list_vtable_t lbst_topdown_vtable;

#endif // LBST_TOPDOWN_H
