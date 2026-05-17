#ifndef LLRB_H
#define LLRB_H

#include "bst.h"
#include "list.h"

typedef struct {
   tree_t tree;
} llrb_t;

extern const list_vtable_t llrb_vtable;

#endif // LLRB_H
