#ifndef SPLAY_H
#define SPLAY_H

#include "bst.h"
#include "list.h"

typedef struct {
   tree_t tree;
} splay_t;

// Splay tree operations
void splay_init(splay_t *t);
splay_t splay_clone(splay_t *t);
void splay_free(splay_t *t);
u64 splay_size(splay_t *t);
void splay_each(splay_t *t, void (*visit)(u64, void *), void *ctx);
void splay_verify(splay_t *t);
u64 splay_select(splay_t *t, u64 i);
void splay_update(splay_t *t, u64 i, u64 x);
void splay_insert(splay_t *t, u64 i, u64 x);
u64 splay_delete(splay_t *t, u64 i);

// The vtable for the list interface.
extern const list_vtable_t splay_vtable;

#endif // SPLAY_H
