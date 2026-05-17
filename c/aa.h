#ifndef AA_H
#define AA_H

#include "bst.h"
#include "list.h"

typedef struct {
   tree_t tree;
} aa_t;

extern const list_vtable_t aa_vtable;

#endif // AA_H
