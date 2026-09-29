#include "splay.h"
#include "tree_list_adapter.h"

// ─────────────────────────────────────────────────────────────────────────────
// Internal splay operations
// ─────────────────────────────────────────────────────────────────────────────

static node_t *splay_splay(node_t *p, u64 size, u64 i) {
   u64 original_size = size;
   invariant(p != NULL);
   tree_rank_inputs_require(p, i, size);
   tree_persist_unshared(&p);
   node_t n = {.s = i};
   node_t *l = &n;
   node_t *r = &n;
   for (;;) {
      invariant(p != NULL);
      tree_rank_inputs_require(p, i, size);
      if (i == p->s) {
         break;
      }
      if (i < p->s) {
         u64 left_size = sizeL(p);
         tree_nonempty_root_shape_require(p->l, left_size);
         if (i < p->l->s) {
            //
            // ROTATE RIGHT, LINK LEFT
            //
            u64 next_size = sizeL(p->l);
            tree_nonempty_root_shape_require(p->l->l, next_size);
            tree_persist_unshared(&p->l);
            invariant(p->l->l != NULL);
            tree_persist_unshared(&p->l->l);
            p = rotateR(p);
            invariant(i < p->s);
            p->s = rank_in_right_subtree(p->s, i);
            r->l = p;
            r = r->l;
            p = p->l;
            size = next_size;
         } else if (i > p->l->s) {
            //
            // LINK LEFT, LINK RIGHT
            //
            u64 next_size = sizeR(p->l, left_size);
            tree_nonempty_root_shape_require(p->l->r, next_size);
            tree_persist_unshared(&p->l);
            invariant(p->l->r != NULL);
            tree_persist_unshared(&p->l->r);
            invariant(i < p->s);
            p->s = rank_in_right_subtree(p->s, i);
            r->l = p;
            r = r->l;
            p = p->l;
            invariant(i > p->s);
            i = rank_in_right_subtree(i, p->s);
            l->r = p;
            l = l->r;
            p = p->r;
            size = next_size;
         } else {
            //
            // LINK LEFT, BREAK
            //
            tree_persist_unshared(&p->l);
            invariant(i < p->s);
            p->s = rank_in_right_subtree(p->s, i);
            r->l = p;
            r = r->l;
            p = p->l;
            break;
         }
      } else {
         u64 right_size = sizeR(p, size);
         tree_nonempty_root_shape_require(p->r, right_size);
         u64 right_root = rank_after_predecessor(p->s, p->r);
         if (i > right_root) {
            //
            // ROTATE LEFT, LINK RIGHT
            //
            u64 next_size = sizeR(p->r, right_size);
            tree_nonempty_root_shape_require(p->r->r, next_size);
            tree_persist_unshared(&p->r);
            invariant(p->r->r != NULL);
            tree_persist_unshared(&p->r->r);
            p = rotateL(p);
            invariant(i > p->s);
            i = rank_in_right_subtree(i, p->s);
            l->r = p;
            l = l->r;
            p = p->r;
            size = next_size;
         } else if (i < right_root) {
            //
            // LINK RIGHT, LINK LEFT
            //
            u64 next_size = sizeL(p->r);
            tree_nonempty_root_shape_require(p->r->l, next_size);
            tree_persist_unshared(&p->r);
            invariant(p->r->l != NULL);
            tree_persist_unshared(&p->r->l);
            invariant(i > p->s);
            i = rank_in_right_subtree(i, p->s);
            l->r = p;
            l = l->r;
            p = p->r;
            invariant(i < p->s);
            p->s = rank_in_right_subtree(p->s, i);
            r->l = p;
            r = r->l;
            p = p->l;
            size = next_size;
         } else {
            //
            // LINK RIGHT, BREAK
            //
            tree_persist_unshared(&p->r);
            invariant(i > p->s);
            invariant(rank_in_right_subtree(i, p->s) == p->r->s);
            l->r = p;
            l = l->r;
            p = p->r;
            break;
         }
      }
   }
   invariant(!tree_shared(p));
   invariant(l == &n || !tree_shared(l));
   invariant(r == &n || !tree_shared(r));
   l->r = p->l;
   r->l = p->r;
   p->r = n.l;
   p->l = n.r;
   p->s = n.s;
   invariant(sizeL(p) == n.s);
   tree_nonempty_root_shape_require(p, original_size);
   return p;
}

static node_t *splay_splayMax(node_t *l, u64 size) {
   tree_sized_root_shape_require(l, size);
   if (l == NULL) {
      invariant(size == 0);
      return NULL;
   }
   tree_nonempty_root_shape_require(l, size);
   tree_persist_unshared(&l);
   while (l->r != NULL) {
      tree_nonempty_root_shape_require(l, size);
      u64 right_size = sizeR(l, size);
      tree_nonempty_root_shape_require(l->r, right_size);
      if (l->r->r != NULL) {
         tree_nonempty_root_shape_require(l->r->r, sizeR(l->r, right_size));
         tree_persist_unshared(&l->r);
         tree_persist_unshared(&l->r->r);
         l->r = rotateL(l->r);
         l = rotateL(l);
      } else {
         tree_persist_unshared(&l->r);
         l = rotateL(l);
      }
   }
   invariant(l->r == NULL);
   return l;
}

// 1. Splay the right-most node of l*, which wouldn't have a right subtree.
// 2. Set the right subtree of the splayed node to r*.
// 3. Return the splayed node.
static node_t *splay_join(node_t *l, node_t *r, u64 sl, u64 sr) {
   tree_join_inputs_require(l, r, sl, sr);
#ifdef DEBUG
   tree_debug_require_join_inputs(l, r, sl, sr);
#endif
   if (l == NULL) {
      return r;
   }
   l = splay_splayMax(l, sl);
   invariant(!tree_shared(l));
   l->r = r;
#ifdef DEBUG
   tree_debug_require_size(l, size_sum(sl, sr));
#endif
   return l;
}

// 1. Splay the node at `split_size`.
// 2. Cut the left subtree of the root as the split's left side.
// 3. The remaining root and right subtree are the split's right side.
static tree_split_result_t splay_split(node_t *root, u64 size, u64 split_size) {
   invariant(root != NULL);
   invariant(split_size < size);
   tree_debug_require_size(root, size);
   root = splay_splay(root, size, split_size);
   invariant(!tree_shared(root));
   invariant(root->s == split_size);
   tree_split_result_t split = {
       .left = root->l,
       .right = root,
   };
   root->l = NULL;
   root->s = 0;
   return split;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public Splay tree operations
// ─────────────────────────────────────────────────────────────────────────────

void splay_init(splay_t *t) {
   invariant(t != NULL);
   t->tree.root = NULL;
   t->tree.size = 0;
}

splay_t splay_clone(splay_t *t) {
   invariant(t != NULL);
   splay_t clone = *t;
   clone.tree = tree_clone(&t->tree);
   return clone;
}

void splay_free(splay_t *t) {
   invariant(t != NULL);
   tree_list_require_sized_tree(&t->tree);
   free_nodes(t->tree.root);
   t->tree.root = NULL;
   t->tree.size = 0;
}

u64 splay_size(splay_t *t) {
   invariant(t != NULL);
   return tree_list_require_sized_tree(&t->tree)->size;
}

void splay_each(splay_t *t, void (*visit)(u64, void *), void *ctx) {
   invariant(t != NULL);
   invariant(visit != NULL);
   inorder(tree_list_require_sized_tree(&t->tree)->root, visit, ctx);
}

void splay_verify(splay_t *t) {
   invariant(t != NULL);
   tree_list_verify_tree(t->tree);
}

// 1. Splay the node at `i`
// 2. Return the root.
u64 splay_select(splay_t *t, u64 i) {
   invariant(t != NULL);
   tree_rank_inputs_require(t->tree.root, i, t->tree.size);
   t->tree.root = splay_splay(t->tree.root, t->tree.size, i);
   invariant(t->tree.root->s == i);
   invariant(!tree_shared(t->tree.root));
   tree_list_require_sized_tree(&t->tree);
   return t->tree.root->x;
}

// 1. Splay the node to be updated, at position `i`.
// 2. Update the root's data.
void splay_update(splay_t *t, u64 i, u64 x) {
   invariant(t != NULL);
   tree_rank_inputs_require(t->tree.root, i, t->tree.size);
   t->tree.root = splay_splay(t->tree.root, t->tree.size, i);
   invariant(t->tree.root->s == i);
   invariant(!tree_shared(t->tree.root));
   t->tree.root->x = x;
   tree_list_require_sized_tree(&t->tree);
}

//  1. Allocate a new node for the data `x`.
//  2. Split the root into the left and right subtrees of the new node, such
//     that the first `i` nodes are on the left and the rest on the right.
//  3. Replace the previous root with the new node.
void splay_insert(splay_t *t, u64 i, u64 x) {
   invariant(t != NULL);
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   if (i == t->tree.size) {
      t->tree.root = tree_allocate((node_t){
          .x = x,
          .s = t->tree.size,
          .l = splay_splayMax(t->tree.root, t->tree.size),
      });
      tree_list_sized_inserted(&t->tree);
      invariant(t->tree.size == inserted_size);
      return;
   }
   tree_split_result_t split = splay_split(t->tree.root, t->tree.size, i);
   t->tree.root = tree_allocate((node_t){
       .x = x,
       .s = i,
       .l = split.left,
       .r = split.right,
   });
   tree_list_sized_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
}

// 1. Splay the node to be deleted, making it the root.
// 2. Replace the root by a join of its left and right subtrees.
// 3. Return the deleted node.
u64 splay_delete(splay_t *t, u64 i) {
   invariant(t != NULL);
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   t->tree.root = splay_splay(t->tree.root, t->tree.size, i);
   invariant(t->tree.root->s == i);
   invariant(!tree_shared(t->tree.root));
   node_t *old = t->tree.root;
   u64 x = old->x;
   tree_share(old->l);
   tree_share(old->r);
   t->tree.root = splay_join(old->l, old->r, old->s, sizeR(old, size));
   tree_release(old);
   tree_list_sized_deleted(&t->tree);
   invariant(t->tree.size == deleted_size);
   return x;
}

// ─────────────────────────────────────────────────────────────────────────────
// List vtable implementation
// ─────────────────────────────────────────────────────────────────────────────

static list_t splay_vt_new(const list_vtable_t *vt) {
   list_vtable_require(vt);
   splay_t *t = (splay_t *)bst_calloc(1, sizeof(splay_t));
   splay_init(t);
   return tree_list_make(t, vt);
}

static list_t splay_vt_clone(list_t *self) {
   list_require(self);
   splay_t *orig = (splay_t *)self->impl;
   return tree_list_clone_impl(self, orig, sizeof(splay_t), &orig->tree);
}

static void splay_vt_free(list_t *self) {
   list_require(self);
   splay_t *t = (splay_t *)self->impl;
   tree_list_require_sized_tree(&t->tree);
   tree_list_free_impl(self, t, sizeof(*t), &t->tree);
}

static u64 splay_vt_size(list_t *self) {
   list_require(self);
   return splay_size((splay_t *)self->impl);
}

static void splay_vt_each(list_t *self, void (*visit)(u64, void *), void *ctx) {
   list_require(self);
   splay_each((splay_t *)self->impl, visit, ctx);
}

static void splay_vt_verify(list_t *self) {
   list_require(self);
   splay_verify((splay_t *)self->impl);
}

static u64 splay_vt_select(list_t *self, u64 i) {
   list_require(self);
   return splay_select((splay_t *)self->impl, i);
}

static void splay_vt_update(list_t *self, u64 i, u64 x) {
   list_require(self);
   splay_update((splay_t *)self->impl, i, x);
}

static void splay_vt_insert(list_t *self, u64 i, u64 x) {
   list_require(self);
   splay_insert((splay_t *)self->impl, i, x);
}

static u64 splay_vt_delete(list_t *self, u64 i) {
   list_require(self);
   return splay_delete((splay_t *)self->impl, i);
}

TREE_LIST_DEFINE_EVENT_APPLIERS(splay)

static void splay_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   tree_list_require_split(self, i, out_l, out_r);
   splay_t *t = (splay_t *)self->impl;
   u64 right_size = tree_split_inputs_require(t->tree.root, i, t->tree.size);
   tree_list_require_split_result(self, i, i, right_size, out_l, out_r);
   tree_share(t->tree.root);

   if (i == t->tree.size) {
      TREE_LIST_SPLIT_RESULT_PREPARED(splay_t, t->tree.size, t->tree.root,
                                      t->tree.size, NULL, 0);
      return;
   }

   tree_split_result_t split = splay_split(t->tree.root, t->tree.size, i);

   TREE_LIST_SPLIT_RESULT_PREPARED(splay_t, t->tree.size, split.left, i,
                                   split.right, right_size);
}

static list_t splay_vt_join(list_t *self, list_t *other) {
   tree_list_require_join(self, other);
   splay_t *t = (splay_t *)self->impl;
   splay_t *that = (splay_t *)other->impl;

   tree_join_roots_t roots =
       tree_list_prepare_join_roots(&t->tree, &that->tree);

   node_t *root =
       splay_join(roots.left, roots.right, t->tree.size, that->tree.size);
   TREE_LIST_JOIN_RESULT(splay_t, root,
                         tree_list_join_size(t->tree.size, that->tree.size));
}

TREE_LIST_DEFINE_SIZED_TREE_ACCESSOR(splay_t, splay)
const list_vtable_t splay_vtable = TREE_LIST_VTABLE("Splay", splay);
