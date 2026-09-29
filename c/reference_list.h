#ifndef REFERENCE_LIST_H
#define REFERENCE_LIST_H

#include "list.h"

extern const list_vtable_t reference_vtable;

list_t reference_for(list_t *instance);
list_t reference_of_size(u64 size);

#endif // REFERENCE_LIST_H
