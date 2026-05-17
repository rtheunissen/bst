#ifndef ZIP_H
#define ZIP_H

#include "bst.h"
#include "list.h"
#include "random.h"

typedef struct {
   tree_t tree;
   xoshiro256_t source;
} zip_t;

extern const list_vtable_t zip_vtable;

#endif // ZIP_H
