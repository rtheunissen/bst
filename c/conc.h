#ifndef CONC_H
#define CONC_H

#include "bst.h"
#include "list.h"

typedef struct {
   tree_t tree;
} conc_t;

extern const list_vtable_t conc_vtable;

#endif // CONC_H
