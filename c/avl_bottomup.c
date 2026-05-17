#include "avl_bottomup.h"
#include <math.h>

// ─────────────────────────────────────────────────────────────────────────────
// Shared AVL balancing (used by ALL AVL variants)
// ─────────────────────────────────────────────────────────────────────────────

node_t *avl_balanceInsertL(node_t *p) {
   if (!isZeroChild(p, p->l)) return p;
   if (isOneChild(p, p->r)) {
      promote(p);
      return p;
   }
   if (isTwoChild(p->l, p->l->r)) {
      treeRotateR(&p);
      demote(p->r);
      return p;
   }
   assert_bst(isOneChild(p->l, p->l->r));
   treeRotateLR(&p);
   promote(p);
   demote(p->l);
   demote(p->r);
   return p;
}

node_t *avl_balanceInsertR(node_t *p) {
   if (isZeroChild(p, p->r)) {
      if (isOneChild(p, p->l)) {
         promote(p);
      } else if (isTwoChild(p->r, p->r->l)) {
         treeRotateL(&p);
         demote(p->l);
      } else {
         treeRotateRL(&p);
         promote(p);
         demote(p->l);
         demote(p->r);
      }
   }
   return p;
}

node_t *avl_balanceDeleteL(node_t *p) {
   if (isTwoTwo(p)) {
      demote(p);
      return p;
   }
   if (isThreeChild(p, p->l)) {
      assert_bst(isOneChild(p, p->r));
      if (isTwoChild(p->r, p->r->r)) {
         treeRotateRL(&p);
         promote(p);
         demote(p->r);
         demote(p->l);
         demote(p->l);
         return p;
      }
      assert_bst(isOneChild(p->r, p->r->r));
      if (isTwoChild(p->r, p->r->l)) {
         treeRotateL(&p);
         demote(p->l);
         demote(p->l);
         return p;
      } else {
         assert_bst(isOneChild(p->r, p->r->l));
         assert_bst(isOneChild(p->r, p->r->r));
         treeRotateL(&p);
         promote(p);
         demote(p->l);
         return p;
      }
   }
   return p;
}

node_t *avl_balanceDeleteR(node_t *p) {
   if (isTwoTwo(p)) {
      demote(p);
      return p;
   }
   if (isThreeChild(p, p->r)) {
      if (isTwoChild(p->l, p->l->l)) {
         treeRotateLR(&p);
         promote(p);
         demote(p->l);
         demote(p->r);
         demote(p->r);
      } else {
         if (isTwoChild(p->l, p->l->r)) {
            treeRotateR(&p);
            demote(p->r);
            demote(p->r);
         } else {
            treeRotateR(&p);
            promote(p);
            demote(p->r);
         }
      }
   }
   return p;
}

// ─────────────────────────────────────────────────────────────────────────────
// Build / Join / Split (shared across AVL family)
// ─────────────────────────────────────────────────────────────────────────────

node_t *avl_buildL(node_t *l, node_t *p, node_t *r, u64 sl) {
   assert_bst(rank_of(r) >= rank_of(l));
   if (rankDifference(r, l) <= 1) {
      p->l = l; p->r = r; p->s = sl;
      p->y = (u64)(rank_of(r) + 1);
      return p;
   }
   tree_persist(&r);
   r->s = sizeL(r) + sl + 1;
   r->l = avl_buildL(l, p, r->l, sl);
   return avl_balanceInsertL(r);
}

node_t *avl_buildR(node_t *l, node_t *p, node_t *r, u64 sl) {
   assert_bst(rank_of(l) >= rank_of(r));
   if (rankDifference(l, r) <= 1) {
      p->l = l; p->r = r; p->s = sl;
      p->y = (u64)(rank_of(l) + 1);
      return p;
   }
   tree_persist(&l);
   l->r = avl_buildR(l->r, p, r, sizeR(l, sl));
   return avl_balanceInsertR(l);
}

node_t *avl_build(node_t *l, node_t *p, node_t *r, u64 sl) {
   assert_bst(sl == node_size(l));
   if (rank_of(l) < rank_of(r)) {
      return avl_buildL(l, p, r, sl);
   } else {
      return avl_buildR(l, p, r, sl);
   }
}

node_t *avl_deleteMin(node_t *p, node_t **min) {
   tree_persist(&p);
   if (p->l == NULL) { node_t *r = p->r; p->r = NULL; *min = p; return r; }
   p->s = p->s - 1;
   p->l = avl_deleteMin(p->l, min);
   return avl_balanceDeleteL(p);
}

node_t *avl_deleteMax(node_t *p, node_t **max) {
   tree_persist(&p);
   if (p->r == NULL) { node_t *l = p->l; p->l = NULL; *max = p; return l; }
   p->r = avl_deleteMax(p->r, max);
   return avl_balanceDeleteR(p);
}

node_t *avl_joinL(node_t *l, node_t *r, u64 sl) {
   node_t *p = NULL;
   if (rankDifference(r, l) <= 1) {
      node_t *r2 = avl_deleteMin(r, &p);
      return avl_build(l, p, r2, sl);
   }
   tree_persist(&r);
   r->s = sizeL(r) + sl;
   r->l = avl_joinL(l, r->l, sl);
   return avl_balanceInsertL(r);
}

node_t *avl_joinR(node_t *l, node_t *r, u64 sl) {
   node_t *p = NULL;
   if (rankDifference(l, r) <= 1) {
      node_t *l2 = avl_deleteMax(l, &p);
      return avl_build(l2, p, r, sl - 1);
   }
   tree_persist(&l);
   l->r = avl_joinR(l->r, r, sizeR(l, sl));
   return avl_balanceInsertR(l);
}

node_t *avl_join(node_t *l, node_t *r, u64 sl, u64 sr) {
   if (l == NULL) return r;
   if (r == NULL) return l;
   if (rank_of(l) < rank_of(r) || (rank_of(l) == rank_of(r) && sl <= sr)) {
      return avl_joinL(l, r, sl);
   } else {
      return avl_joinR(l, r, sl);
   }
}

void avl_split(node_t *p, u64 i, u64 s, node_t **out_l, node_t **out_r) {
   if (p == NULL) { *out_l = NULL; *out_r = NULL; return; }
   assert_bst(s == node_size(p));
   tree_persist(&p);
   if (i <= sizeL(p)) {
      node_t *l, *r;
      avl_split(p->l, i, sizeL(p), &l, &r);
      *out_l = l;
      *out_r = avl_build(r, p, p->r, sizeL(p) - i);
   } else {
      node_t *l, *r;
      avl_split(p->r, i - (sizeL(p) + 1), sizeR(p, s), &l, &r);
      *out_l = avl_build(p->l, p, l, sizeL(p));
      *out_r = r;
   }
}


// ─────────────────────────────────────────────────────────────────────────────
// AVLBottomUp-specific insert/delete
// ─────────────────────────────────────────────────────────────────────────────

node_t *avl_bu_insert_op(node_t *p, u64 i, u64 x) {
   if (p == NULL) return tree_allocate((node_t){.x = x});
   tree_persist(&p);
   if (i <= sizeL(p)) {
      p->s = sizeL(p) + 1;
      p->l = avl_bu_insert_op(p->l, i, x);
      return avl_balanceInsertL(p);
   } else {
      p->r = avl_bu_insert_op(p->r, i - (sizeL(p) + 1), x);
      return avl_balanceInsertR(p);
   }
}

node_t *avl_bu_delete_op(node_t *p, u64 s, u64 i, u64 *x) {
   u64 sl = sizeL(p);
   u64 sr = sizeR(p, s);
   if (i == sl) {
      *x = p->x;
      tree_share(p->l);
      tree_share(p->r);
      node_t *result = avl_join(p->l, p->r, sl, sr);
      tree_release(p);
      return result;
   }
   tree_persist(&p);
   if (i < sl) {
      p->s = sl - 1;
      p->l = avl_bu_delete_op(p->l, sl, i, x);
      return avl_balanceDeleteL(p);
   } else {
      p->r = avl_bu_delete_op(p->r, sr, i - (sl + 1), x);
      return avl_balanceDeleteR(p);
   }
}


// ─────────────────────────────────────────────────────────────────────────────
// Vtable
// ─────────────────────────────────────────────────────────────────────────────

#define SIMPLE_VT_BODY(TYPE, VTNAME) \
static list_t VTNAME##_vt_new(const list_vtable_t *vt) { \
   TYPE *t = (TYPE *)calloc(1, sizeof(TYPE)); \
   return (list_t){t, vt}; \
} \
static list_t VTNAME##_vt_clone(list_t *self) { \
   TYPE *orig = (TYPE *)self->impl; \
   TYPE *copy = (TYPE *)malloc(sizeof(TYPE)); \
   tree_share(orig->tree.root); \
   *copy = *orig; \
   return (list_t){copy, self->vt}; \
} \
static void VTNAME##_vt_free(list_t *self) { \
   TYPE *t = (TYPE *)self->impl; \
   free_nodes(t->tree.root); \
   free(t); self->impl = NULL; \
} \
static u64  VTNAME##_vt_size(list_t *self) { return ((TYPE *)self->impl)->tree.size; } \
static void VTNAME##_vt_each(list_t *self, void (*v)(u64, void*), void *c) { inorder(((TYPE *)self->impl)->tree.root, v, c); } \
static u64  VTNAME##_vt_select(list_t *self, u64 i) { return tree_lookup(&((TYPE *)self->impl)->tree, i); } \
static void VTNAME##_vt_update(list_t *self, u64 i, u64 x) { tree_update_base(&((TYPE *)self->impl)->tree, i, x); }

SIMPLE_VT_BODY(avl_bottomup_t, avl_bu)

static void avl_bu_vt_verify(list_t *self) {
   avl_bottomup_t *t = (avl_bottomup_t *)self->impl;
   tree_verify_size(t->tree.root, t->tree.size);
   verifyAVLRanks(t->tree.root, t->tree.size);
}

static void avl_bu_vt_insert(list_t *self, u64 i, u64 x) {
   avl_bottomup_t *t = (avl_bottomup_t *)self->impl;
   t->tree.root = avl_bu_insert_op(t->tree.root, i, x);
   t->tree.size++;
}

static u64 avl_bu_vt_delete(list_t *self, u64 i) {
   avl_bottomup_t *t = (avl_bottomup_t *)self->impl;
   assert_bst(i < t->tree.size);
   u64 x;
   t->tree.root = avl_bu_delete_op(t->tree.root, t->tree.size, i, &x);
   t->tree.size--;
   return x;
}

static void avl_bu_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   avl_bottomup_t *t = (avl_bottomup_t *)self->impl;
   assert_bst(i <= t->tree.size);
   tree_share(t->tree.root);
   node_t *l, *r;
   avl_split(t->tree.root, i, t->tree.size, &l, &r);
   avl_bottomup_t *sl = (avl_bottomup_t *)calloc(1, sizeof(avl_bottomup_t)); sl->tree = (tree_t){l, i};
   avl_bottomup_t *sr = (avl_bottomup_t *)calloc(1, sizeof(avl_bottomup_t)); sr->tree = (tree_t){r, t->tree.size - i};
   *out_l = (list_t){sl, self->vt};
   *out_r = (list_t){sr, self->vt};
}

static list_t avl_bu_vt_join(list_t *self, list_t *other) {
   avl_bottomup_t *t = (avl_bottomup_t *)self->impl;
   avl_bottomup_t *that = (avl_bottomup_t *)other->impl;
   tree_share(t->tree.root); tree_share(that->tree.root);
   avl_bottomup_t *result = (avl_bottomup_t *)calloc(1, sizeof(avl_bottomup_t));
   result->tree.root = avl_join(t->tree.root, that->tree.root, t->tree.size, that->tree.size);
   result->tree.size = t->tree.size + that->tree.size;
   return (list_t){result, self->vt};
}

const list_vtable_t avl_bottomup_vtable = {
   "AVLBottomUp", avl_bu_vt_new, avl_bu_vt_clone, avl_bu_vt_free,
   avl_bu_vt_size, avl_bu_vt_each, avl_bu_vt_verify,
   avl_bu_vt_select, avl_bu_vt_update, avl_bu_vt_insert, avl_bu_vt_delete,
   avl_bu_vt_split, avl_bu_vt_join,
};
