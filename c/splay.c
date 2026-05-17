#include "splay.h"


// ─────────────────────────────────────────────────────────────────────────────
// Internal splay operations
// ─────────────────────────────────────────────────────────────────────────────

static node_t *splay_splay(node_t *p, u64 i) {
   tree_persist(&p);
   node_t n = {.s = i};
   node_t *l = &n;
   node_t *r = &n;
   while (i != p->s) {
      if (i < p->s) {
         if (i < p->l->s) {
            //
            // ROTATE RIGHT, LINK LEFT
            //
            tree_persist(&p->l);
            tree_persist(&p->l->l);
            p = rotateR(p);
          p->s = p->s - i - 1;
          r->l = p;
            r = r->l;
            p = p->l;
         } else if (i > p->l->s) {
            //
            // LINK LEFT, LINK RIGHT
            //
            tree_persist(&p->l);
            tree_persist(&p->l->r);
          p->s = p->s - i - 1;
          r->l = p;
            r = r->l;
            p = p->l;
            i = i - p->s - 1;
          l->r = p;
            l = l->r;
            p = p->r;
         } else {
            //
            // LINK LEFT, BREAK
            //
            tree_persist(&p->l);
          p->s = p->s - i - 1;
          r->l = p;
            r = r->l;
            p = p->l;
            break;
         }
      } else {
         if (i > p->s + p->r->s + 1) {
            //
            // ROTATE LEFT, LINK RIGHT
            //
            tree_persist(&p->r);
            tree_persist(&p->r->r);
            p = rotateL(p);
            i = i - p->s - 1;
          l->r = p;
            l = l->r;
            p = p->r;
         } else if (i < p->s + p->r->s + 1) {
            //
            // LINK RIGHT, LINK LEFT
            //
            tree_persist(&p->r);
            tree_persist(&p->r->l);
            i = i - p->s - 1;
          l->r = p;
            l = l->r;
            p = p->r;
          p->s = p->s - i - 1;
          r->l = p;
            r = r->l;
            p = p->l;
         } else {
            //
            // LINK RIGHT, BREAK
            //
            tree_persist(&p->r);
            i = i - p->s - 1;
          l->r = p;
            l = l->r;
            p = p->r;
            break;
         }
      }
   }
   l->r = p->l;
   r->l = p->r;
   p->r = n.l;
   p->l = n.r;
   p->s = n.s;
   return p;
}

static node_t *splay_splayMax(node_t *l) {
   if (l == NULL) {
      return NULL;
   }
   tree_persist(&l);
   while (l->r != NULL) {
      if (l->r->r != NULL) {
         tree_persist(&l->r);
         tree_persist(&l->r->r);
         l->r = rotateL(l->r);
         l = rotateL(l);
      } else {
         tree_persist(&l->r);
         l = rotateL(l);
      }
   }
   return l;
}

// 1. Splay the right-most node of l*, which wouldn't have a right subtree.
// 2. Set the right subtree of the splayed node to r*.
// 3. Return the splayed node.
static node_t *splay_join(node_t *l, node_t *r) {
   if (l == NULL) {
      return r;
   }
   l = splay_splayMax(l);
   l->r = r;
   return l;
}

// 1. Splay the node at `i`.
// 2. Cut the left subtree of the root as l**, leaving a nil in its place.
// 3. The remaining root and right subtree is r**.
static void splay_split(node_t *p, u64 s, u64 i, node_t **out_l, node_t **out_r) {
   assert_bst(i < s);
   (void)s;
   p = splay_splay(p, i);
   *out_l = p->l;
   *out_r = p;
   p->l = NULL;
   p->s = 0;
}


// ─────────────────────────────────────────────────────────────────────────────
// Public Splay tree operations
// ─────────────────────────────────────────────────────────────────────────────

void splay_init(splay_t *t) {
   t->tree.root = NULL;
   t->tree.size = 0;
}

splay_t splay_clone(splay_t *t) {
   tree_share(t->tree.root);
   return *t;
}

void splay_free(splay_t *t) {
   free_nodes(t->tree.root);
   t->tree.root = NULL;
   t->tree.size = 0;
}

u64 splay_size(splay_t *t) {
   return t->tree.size;
}

void splay_each(splay_t *t, void (*visit)(u64, void *), void *ctx) {
   inorder(t->tree.root, visit, ctx);
}

void splay_verify(splay_t *t) {
   tree_verify_size(t->tree.root, t->tree.size);
}

// 1. Splay the node at `i`
// 2. Return the root.
u64 splay_select(splay_t *t, u64 i) {
   assert_bst(i < t->tree.size);
   t->tree.root = splay_splay(t->tree.root, i);
   return t->tree.root->x;
}

// 1. Splay the node to be updated, at position `i`.
// 2. Update the root's data.
void splay_update(splay_t *t, u64 i, u64 x) {
   assert_bst(i < t->tree.size);
   t->tree.root = splay_splay(t->tree.root, i);
   t->tree.root->x = x;
}

//  1. Allocate a new node for the data `x`.
//  2. Split the root into the left and right subtrees of the new node, such
//     that the first `i` nodes are on the left and the rest on the right.
//  3. Replace the previous root with the new node.
void splay_insert(splay_t *t, u64 i, u64 x) {
   assert_bst(i <= t->tree.size);
   //
   //
   if (i == t->tree.size) {
      t->tree.root = tree_allocate((node_t){.x = x, .s = t->tree.size, .l = splay_splayMax(t->tree.root)});
      t->tree.size++;
      return;
   }
   node_t *l, *r;
   splay_split(t->tree.root, t->tree.size, i, &l, &r);
   t->tree.root = tree_allocate((node_t){.x = x, .s = i, .l = l, .r = r});
   t->tree.size++;
}

// 1. Splay the node to be deleted, making it the root.
// 2. Replace the root by a join of its left and right subtrees.
// 3. Return the deleted node.
u64 splay_delete(splay_t *t, u64 i) {
   assert_bst(i < t->tree.size);
   t->tree.root = splay_splay(t->tree.root, i);
   node_t *old = t->tree.root;
   u64 x = old->x;
   tree_share(old->l);
   tree_share(old->r);
   t->tree.root = splay_join(old->l, old->r);
   tree_release(old);
   t->tree.size--;
   return x;
}


// ─────────────────────────────────────────────────────────────────────────────
// List vtable implementation
// ─────────────────────────────────────────────────────────────────────────────

static list_t splay_vt_new(const list_vtable_t *vt) {
   splay_t *t = (splay_t *)calloc(1, sizeof(splay_t));
   splay_init(t);
   return (list_t){.impl = t, .vt = vt};
}

static list_t splay_vt_clone(list_t *self) {
   splay_t *orig = (splay_t *)self->impl;
   splay_t *copy = (splay_t *)malloc(sizeof(splay_t));
   *copy = splay_clone(orig);
   return (list_t){.impl = copy, .vt = self->vt};
}

static void splay_vt_free(list_t *self) {
   splay_t *t = (splay_t *)self->impl;
   splay_free(t);
   free(t);
   self->impl = NULL;
}

static u64 splay_vt_size(list_t *self) {
   return splay_size((splay_t *)self->impl);
}

static void splay_vt_each(list_t *self, void (*visit)(u64, void *), void *ctx) {
   splay_each((splay_t *)self->impl, visit, ctx);
}

static void splay_vt_verify(list_t *self) {
   splay_verify((splay_t *)self->impl);
}

static u64 splay_vt_select(list_t *self, u64 i) {
   return splay_select((splay_t *)self->impl, i);
}

static void splay_vt_update(list_t *self, u64 i, u64 x) {
   splay_update((splay_t *)self->impl, i, x);
}

static void splay_vt_insert(list_t *self, u64 i, u64 x) {
   splay_insert((splay_t *)self->impl, i, x);
}

static u64 splay_vt_delete(list_t *self, u64 i) {
   return splay_delete((splay_t *)self->impl, i);
}

static void splay_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   splay_t *t = (splay_t *)self->impl;
   assert_bst(i <= t->tree.size);
   tree_share(t->tree.root);

   if (i == t->tree.size) {
      splay_t *sl = (splay_t *)calloc(1, sizeof(splay_t));
      sl->tree.root = t->tree.root;
      sl->tree.size = t->tree.size;

      splay_t *sr = (splay_t *)calloc(1, sizeof(splay_t));

      *out_l = (list_t){.impl = sl, .vt = self->vt};
      *out_r = (list_t){.impl = sr, .vt = self->vt};
      return;
   }
   //
   //
   node_t *lp, *rp;
   splay_split(t->tree.root, t->tree.size, i, &lp, &rp);

   splay_t *sl = (splay_t *)calloc(1, sizeof(splay_t));
   sl->tree.root = lp;
   sl->tree.size = i;

   splay_t *sr = (splay_t *)calloc(1, sizeof(splay_t));
   sr->tree.root = rp;
   sr->tree.size = t->tree.size - i;

   *out_l = (list_t){.impl = sl, .vt = self->vt};
   *out_r = (list_t){.impl = sr, .vt = self->vt};
}

static list_t splay_vt_join(list_t *self, list_t *other) {
   splay_t *t = (splay_t *)self->impl;
   splay_t *that = (splay_t *)other->impl;

   tree_share(t->tree.root);
   tree_share(that->tree.root);

   splay_t *result = (splay_t *)calloc(1, sizeof(splay_t));
   result->tree.root = splay_join(t->tree.root, that->tree.root);
   result->tree.size = t->tree.size + that->tree.size;

   return (list_t){.impl = result, .vt = self->vt};
}

const list_vtable_t splay_vtable = {
   .name      = "Splay",
   .new_fn    = splay_vt_new,
   .clone_fn  = splay_vt_clone,
   .free_fn   = splay_vt_free,
   .size_fn   = splay_vt_size,
   .each_fn   = splay_vt_each,
   .verify_fn = splay_vt_verify,
   .select_fn = splay_vt_select,
   .update_fn = splay_vt_update,
   .insert_fn = splay_vt_insert,
   .delete_fn = splay_vt_delete,
   .split_fn  = splay_vt_split,
   .join_fn   = splay_vt_join,
};
