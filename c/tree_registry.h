#ifndef TREE_REGISTRY_H
#define TREE_REGISTRY_H

#include "list.h"

#include <stdio.h>

extern const list_vtable_t *const tree_registry_all[];
extern const int tree_registry_count;

typedef struct {
   const list_vtable_t *selected[1];
   const list_vtable_t *const *items;
   int count;
} tree_registry_selection_t;

const list_vtable_t *tree_registry_find(const char *name);
const list_vtable_t *tree_registry_find_or_exit(const char *name);
void tree_registry_selection_init(tree_registry_selection_t *selection,
                                  const char *name);
void tree_registry_fprint_names(FILE *file, const char *label);

#endif // TREE_REGISTRY_H
