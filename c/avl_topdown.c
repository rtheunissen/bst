#include "avl_topdown.h"
#include "avl_bottomup.h"
#include "rank.h"
#include "tree_list_adapter.h"

static node_t *avl_td_balance(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   invariant(p != NULL);
   invariant(!tree_shared(p));
   if (isTwoTwo(p)) {
      demote(p);
      return p;
   }
   if (isThreeChild(p, p->l)) {
      if (isTwoChild(p->r, p->r->r)) {
         treeRotateRLSized(&p, size);
         promote(p);
         demote(p->r);
         demote(p->l);
         demote(p->l);
      } else {
         treeRotateLSized(&p, size);
         demote(p->l);
         demote(p->l);
      }
   } else if (isThreeChild(p, p->r)) {
      if (isTwoChild(p->l, p->l->l)) {
         treeRotateLRSized(&p, size);
         promote(p);
         demote(p->l);
         demote(p->r);
         demote(p->r);
      } else {
         treeRotateRSized(&p, size);
         demote(p->r);
         demote(p->r);
      }
   }
   return p;
}

static void avl_td_insert(node_t **p, u64 size, u64 i, u64 x) {
   invariant(p != NULL);
   (void)tree_insert_inputs_require(*p, i, size);
   if (*p == NULL) {
      invariant(i == 0);
      tree_attach(p, x);
      return;
   }
   tree_persist_unshared(p);
   node_t **a = p;
   u64 a_size = size;
   u64 j = i;
   u64 subtree_size = size;
   for (;;) {
      invariant(*p != NULL);
      invariant((*p)->s < subtree_size);
      u64 left_size = sizeL(*p);
      if (i <= left_size) {
         u64 child_size = left_size;
         (void)tree_insert_inputs_require((*p)->l, i, child_size);
         p = insertL(*p);
         subtree_size = child_size;
      } else {
         u64 child_size = sizeR(*p, subtree_size);
         u64 child_rank = rank_in_right_subtree(i, left_size);
         (void)tree_insert_inputs_require((*p)->r, child_rank, child_size);
         p = insertR(*p, &i);
         subtree_size = child_size;
         invariant(i == child_rank);
      }
      if (*p == NULL) {
         *p = tree_allocate((node_t){.x = x});
         break;
      }
      tree_persist_unshared(p);
      if (!isOneOne(*p)) {
         a = p;
         a_size = subtree_size;
         j = i;
      }
   }
   for (node_t *q = *a; q != *p;) {
      promote(q);
      if (j <= sizeL(q)) {
         q = q->l;
      } else {
         j = rank_in_right_subtree(j, sizeL(q));
         q = q->r;
      }
   }
   *a = avl_td_balance(*a, size_after_inserting_one(a_size));
}

TREE_VT_SIMPLE(avl_topdown_t, avl_td)

static void avl_td_vt_verify(list_t *self) {
   list_require(self);
   avl_topdown_t *t = (avl_topdown_t *)self->impl;
   verifyAVLRanks(t->tree.root, t->tree.size);
}
static void avl_td_vt_insert(list_t *self, u64 i, u64 x) {
   list_require(self);
   avl_topdown_t *t = (avl_topdown_t *)self->impl;
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   avl_td_insert(&t->tree.root, size, i, x);
   tree_list_sized_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
}
static u64 avl_td_vt_delete(list_t *self, u64 i) {
   list_require(self);
   avl_topdown_t *t = (avl_topdown_t *)self->impl;
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   avl_bu_delete_result_t result =
       avl_bu_delete_op(t->tree.root, t->tree.size, i);
   t->tree.root = result.root;
   tree_list_sized_deleted(&t->tree);
   invariant(t->tree.size == deleted_size);
   return result.value;
}
static void avl_td_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   tree_list_require_split(self, i, out_l, out_r);
   avl_topdown_t *t = (avl_topdown_t *)self->impl;
   u64 size = t->tree.size;
   u64 right_size = tree_split_inputs_require(t->tree.root, i, size);
   tree_share(t->tree.root);
   tree_split_result_t split = avl_split(t->tree.root, i, size);
   TREE_LIST_SPLIT_RESULT_PREPARED(avl_topdown_t, size, split.left, i,
                                   split.right, right_size);
}
static list_t avl_td_vt_join(list_t *self, list_t *other) {
   tree_list_require_join(self, other);
   avl_topdown_t *t = (avl_topdown_t *)self->impl;
   avl_topdown_t *that = (avl_topdown_t *)other->impl;
   tree_join_roots_t roots =
       tree_list_prepare_join_roots(&t->tree, &that->tree);
   node_t *root =
       avl_join(roots.left, roots.right, t->tree.size, that->tree.size);
   u64 joined_size = tree_list_join_size(t->tree.size, that->tree.size);
   TREE_LIST_JOIN_RESULT(avl_topdown_t, root, joined_size);
}
const list_vtable_t avl_topdown_vtable = TREE_LIST_VTABLE("AVLTopDown", avl_td);
