#ifndef LBST_BOTTOMUP_H
#define LBST_BOTTOMUP_H
#include "bst.h"
#include "list.h"
#include "weight.h"
typedef struct { tree_t tree; weight_balance_t wb; } lbst_bottomup_t;
extern const list_vtable_t lbst_bottomup_vtable;
#endif
