#ifndef TREE_PERSISTENCE_H
#define TREE_PERSISTENCE_H

// Included by bst.h after node_t and tree_allocate are defined.

// Removes one owning reference from the given node. If no other tree owns it,
// the node and its children are released recursively in persistent builds. In
// non-persistent builds this releases only the node; use free_nodes for whole
// subtrees.
void tree_release(node_t *p);

// Frees nodes recursively, respecting reference counts and live arena ownership.
void free_nodes(node_t *p);

// Adds a reference to the node to share it with another tree.
static inline void tree_share(node_t *p) {
#ifdef PERSISTENT
   if (p != NULL) {
      invariant(p->refs < UINT64_MAX);
      p->refs = size_after_inserting_one(p->refs);
   }
#else
   (void)p;
#endif
}

// Determines whether a node is shared with at least one other tree.
static inline bool tree_shared(node_t *p) {
#ifdef PERSISTENT
   return p != NULL && p->refs > 0;
#else
   (void)p;
   return false;
#endif
}

// Replaces the given node with a copy only if the node is shared with other
// trees. This is copy-on-write: only copy when about to modify a shared node.
static inline void tree_persist(node_t **p) {
   invariant(p != NULL);
   invariant(*p != NULL);
#ifdef PERSISTENT
   invariant((*p)->l == NULL || (*p)->r == NULL || (*p)->l != (*p)->r);
   if (!tree_shared(*p)) {
      return;
   }

   tree_share((*p)->l);
   tree_share((*p)->r);

   node_t *old = *p;

   *p = tree_allocate((node_t){
       .l = old->l,
       .r = old->r,
       .s = old->s,
       .x = old->x,
       .y = old->y,
   });
   invariant(!tree_shared(*p));

   tree_release(old);
#else
   (void)p;
#endif
   invariant(*p != NULL);
}

// Copy-on-write preflight for code that is about to mutate the referenced node.
static inline void tree_persist_unshared(node_t **p) {
   invariant(p != NULL);
   invariant(*p != NULL);
   tree_persist(p);
   invariant(!tree_shared(*p));
}

#endif // TREE_PERSISTENCE_H
