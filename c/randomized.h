#ifndef RANDOMIZED_H
#define RANDOMIZED_H

#include "bst.h"
#include "list.h"
#include "random.h"

typedef struct {
   tree_t tree;
   xoshiro256_t source;
} randomized_t;

extern const list_vtable_t randomized_vtable;

#endif // RANDOMIZED_H
