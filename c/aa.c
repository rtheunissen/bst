#include "aa.h"
#include "rank.h"
#include "tree_list_adapter.h"

// ─────────────────────────────────────────────────────────────────────────────
// AA tree internals
//
// An AA tree by Arne Andersson. Red links may only lean right.
// This is the inverse of the LLRB constraint.
//
// Two operations maintain the invariant:
//    skew:  if the left child is a 0-child, rotate right.
//    split: if the right grandchild is a 0-child of the right child which
//           is itself a 0-child, rotate left and promote.
// ─────────────────────────────────────────────────────────────────────────────

static void aa_skew(node_t **p, u64 size) {
   invariant(p != NULL);
   tree_sized_root_shape_require(*p, size);
   if (*p != NULL && isZeroChild(*p, (*p)->l)) {
      treeRotateRSized(p, size);
   }
}

static void aa_split(node_t **p, u64 size) {
   invariant(p != NULL);
   tree_sized_root_shape_require(*p, size);
   if (*p != NULL && (*p)->r != NULL && isZeroChild(*p, (*p)->r) &&
       isZeroChild((*p)->r, (*p)->r->r)) {
      treeRotateLSized(p, size);
      promote(*p);
   }
}

static node_t *aa_insert(node_t *p, u64 size, u64 i, u64 x) {
   u64 inserted_size = tree_insert_inputs_require(p, i, size);
   if (p == NULL) {
      invariant(i == 0);
      return tree_allocate((node_t){.x = x});
   }
   tree_persist_unshared(&p);
   if (i <= p->s) {
      u64 child_size = sizeL(p);
      node_t **l = insertL(p);
      *l = aa_insert(*l, child_size, i, x);
   } else {
      u64 child_size = sizeR(p, size);
      node_t **r = insertR(p, &i);
      *r = aa_insert(*r, child_size, i, x);
   }
   aa_skew(&p, inserted_size);
   aa_split(&p, inserted_size);
   return p;
}

static void aa_decreaseLevel(node_t *p) {
   invariant(p != NULL);
   int rl = rank_of(p->l);
   int rr = rank_of(p->r);

   int shouldBe = 1 + MIN(rl, rr);

   if (shouldBe < rank_of(p)) {
      p->y = (u64)shouldBe;
      if (shouldBe < rr) {
         tree_persist_unshared(&p->r);
         p->r->y = (u64)shouldBe;
      }
   }
}

static node_t *aa_fixAfterDelete(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   aa_decreaseLevel(p);
   aa_skew(&p, size);
   if (p->r != NULL) {
      u64 right_size = sizeR(p, size);
      tree_persist_unshared(&p->r);
      aa_skew(&p->r, right_size);
      if (p->r->r != NULL) {
         u64 right_right_size = sizeR(p->r, right_size);
         tree_persist_unshared(&p->r->r);
         aa_skew(&p->r->r, right_right_size);
      }
   }
   aa_split(&p, size);
   if (p->r != NULL) {
      u64 right_size = sizeR(p, size);
      tree_persist_unshared(&p->r);
      aa_split(&p->r, right_size);
   }
   return p;
}

typedef struct {
   node_t *root;
   node_t *node;
} aa_extract_result_t;

static aa_extract_result_t aa_deleteMin(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   tree_persist_unshared(&p);
   if (p->l == NULL) {
      node_t *r = p->r;
      p->r = NULL;
      return (aa_extract_result_t){.root = r, .node = p};
   }
   invariant(p->s > 0);
   u64 child_size = sizeL(p);
   node_t **l = deleteL(p);
   aa_extract_result_t result = aa_deleteMin(*l, child_size);
   *l = result.root;
   result.root = aa_fixAfterDelete(p, size_after_removing_one(size));
   return result;
}

static aa_extract_result_t aa_deleteMax(node_t *p, u64 size) {
   tree_nonempty_root_shape_require(p, size);
   tree_persist_unshared(&p);
   if (p->r == NULL) {
      node_t *l = p->l;
      p->l = NULL;
      return (aa_extract_result_t){.root = l, .node = p};
   }
   u64 child_size = sizeR(p, size);
   aa_extract_result_t result = aa_deleteMax(p->r, child_size);
   p->r = result.root;
   result.root = aa_fixAfterDelete(p, size_after_removing_one(size));
   return result;
}

typedef struct {
   node_t *root;
   u64 value;
} aa_delete_result_t;

static aa_delete_result_t aa_delete(node_t *p, u64 size, u64 i) {
   u64 deleted_size = tree_delete_inputs_require(p, i, size);
   tree_persist_unshared(&p);
   u64 sl = sizeL(p);
   u64 sr = sizeR(p, size);
   aa_delete_result_t result;
   if (i < p->s) {
      invariant(p->l != NULL);
      node_t **l = deleteL(p);
      result = aa_delete(*l, sl, i);
      *l = result.root;
   } else if (i > p->s) {
      invariant(p->r != NULL);
      node_t **r = deleteR(p, &i);
      result = aa_delete(*r, sr, i);
      *r = result.root;
   } else {
      result.value = p->x;
      if (p->l == NULL && p->r == NULL) {
         tree_release(p);
         return (aa_delete_result_t){.root = NULL, .value = result.value};
      }
      if (p->l == NULL) {
         invariant(p->r != NULL);
         aa_extract_result_t extracted = aa_deleteMin(p->r, sr);
         p->r = extracted.root;
         node_t *min = extracted.node;
         invariant(min != NULL);
         p->x = min->x;
         tree_release(min);
      } else {
         node_t **l = deleteL(p);
         aa_extract_result_t extracted = aa_deleteMax(*l, sl);
         *l = extracted.root;
         node_t *max = extracted.node;
         invariant(max != NULL);
         p->x = max->x;
         tree_release(max);
      }
   }
   result.root = aa_fixAfterDelete(p, deleted_size);
   return result;
}

static node_t *aa_build(node_t *l, node_t *p, node_t *r, u64 sl, u64 sr) {
   u64 size = tree_build_inputs_require(l, p, r, sl, sr);
   tree_debug_require_size(l, sl);
   tree_debug_require_size(r, sr);
   int rl = rank_of(l);
   int rr = rank_of(r);
   if (rank_distance(rl, rr) <= 1) {
      p->l = l;
      p->r = r;
      p->s = sl;
      p->y = rank_above(MAX(rl, rr));
      return aa_fixAfterDelete(p, size);
   }
   if (rl < rr) {
      invariant(r != NULL);
      tree_persist_unshared(&r);
      u64 right_left_size = sizeL(r);
      r->s = tree_size_from_child_sizes(sl, right_left_size);
      r->l = aa_build(l, p, r->l, sl, right_left_size);
      return aa_fixAfterDelete(r, size);
   } else {
      invariant(l != NULL);
      invariant(l->s < sl);
      tree_persist_unshared(&l);
      u64 left_right_size = sizeR(l, sl);
      l->r = aa_build(l->r, p, r, left_right_size, sr);
      return aa_fixAfterDelete(l, size);
   }
}

static node_t *aa_join(node_t *l, node_t *r, u64 sl, u64 sr) {
   tree_join_inputs_require(l, r, sl, sr);
#ifdef DEBUG
   tree_debug_require_join_inputs(l, r, sl, sr);
#endif
   if (l == NULL) {
      invariant(sl == 0);
      return r;
   }
   if (r == NULL) {
      invariant(sr == 0);
      return l;
   }
   if (rank_of(l) < rank_of(r) || (rank_of(l) == rank_of(r) && sl <= sr)) {
      aa_extract_result_t extracted = aa_deleteMin(r, sr);
      node_t *r2 = extracted.root;
      node_t *p = extracted.node;
      invariant(p != NULL);
      return aa_build(l, p, r2, sl, size_after_removing_one(sr));
   } else {
      aa_extract_result_t extracted = aa_deleteMax(l, sl);
      node_t *l2 = extracted.root;
      node_t *p = extracted.node;
      invariant(p != NULL);
      invariant(sl > 0);
      return aa_build(l2, p, r, rank_of_last(sl), sr);
   }
}

static tree_split_result_t aa_split_impl(node_t *root, u64 split_size,
                                         u64 size) {
   (void)tree_split_inputs_require(root, split_size, size);
   if (root == NULL) {
      return (tree_split_result_t){.left = NULL, .right = NULL};
   }
   tree_persist_unshared(&root);

   u64 left_size = sizeL(root);
   u64 right_size = sizeR(root, size);

   if (split_size <= root->s) {
      tree_split_result_t split = aa_split_impl(root->l, split_size, left_size);

      u64 right_split_size = suffix_size_after_prefix(left_size, split_size);
      return (tree_split_result_t){
          .left = split.left,
          .right = aa_build(split.right, root, root->r, right_split_size,
                            right_size),
      };
   }

   u64 right_rank = rank_in_right_subtree(split_size, left_size);
   tree_split_result_t split = aa_split_impl(root->r, right_rank, right_size);
   return (tree_split_result_t){
       .left = aa_build(root->l, root, split.left, left_size, right_rank),
       .right = split.right,
   };
}

static tree_split_result_t aa_split_op(node_t *root, u64 split_size, u64 size) {
   (void)tree_split_inputs_require(root, split_size, size);
   if (root != NULL) {
      tree_debug_require_size(root, size);
   }
   return aa_split_impl(root, split_size, size);
}

static void aa_verifyRankNode(node_t *p, void *ctx) {
   (void)ctx;
   if (isLeaf(p)) {
      invariant(rank_of(p) == 0);
   }
   // All rank differences are positive (no left 0-children allowed).
   invariant(rank_of(p) > rank_of(p->l));

   // Right child may be a 0-child or 1-child.
   invariant(isZeroChild(p, p->r) || isOneChild(p, p->r));

   // No two consecutive right 0-children.
   if (isZeroChild(p, p->r)) {
      invariant(!isZeroChild(p->r, p->r->r));
   }
}

static void aa_verifyRanks(node_t *p, u64 size) {
   invariant(tree_verify_size_each_node(p, size, aa_verifyRankNode, NULL) ==
             size);
}

// ─────────────────────────────────────────────────────────────────────────────
// List vtable
// ─────────────────────────────────────────────────────────────────────────────

TREE_LIST_SIMPLE_FUNCTIONS(aa_t, aa)

static void aa_vt_verify(list_t *self) {
   list_require(self);
   aa_t *t = (aa_t *)self->impl;
   aa_verifyRanks(t->tree.root, t->tree.size);
}

static void aa_vt_insert(list_t *self, u64 i, u64 x) {
   list_require(self);
   aa_t *t = (aa_t *)self->impl;
   u64 size = t->tree.size;
   u64 inserted_size = tree_insert_inputs_require(t->tree.root, i, size);
   t->tree.root = aa_insert(t->tree.root, size, i, x);
   tree_list_sized_inserted(&t->tree);
   invariant(t->tree.size == inserted_size);
}

static u64 aa_vt_delete(list_t *self, u64 i) {
   list_require(self);
   aa_t *t = (aa_t *)self->impl;
   u64 size = t->tree.size;
   u64 deleted_size = tree_delete_inputs_require(t->tree.root, i, size);
   aa_delete_result_t result = aa_delete(t->tree.root, size, i);
   t->tree.root = result.root;
   tree_list_sized_deleted(&t->tree);
   invariant(t->tree.size == deleted_size);
   return result.value;
}

static void aa_vt_split(list_t *self, u64 i, list_t *out_l, list_t *out_r) {
   tree_list_require_split(self, i, out_l, out_r);
   aa_t *t = (aa_t *)self->impl;
   u64 size = t->tree.size;
   u64 right_size = tree_split_inputs_require(t->tree.root, i, size);
   tree_share(t->tree.root);
   tree_split_result_t split = aa_split_op(t->tree.root, i, size);
   TREE_LIST_SPLIT_RESULT_PREPARED(aa_t, size, split.left, i, split.right,
                                   right_size);
}

static list_t aa_vt_join(list_t *self, list_t *other) {
   tree_list_require_join(self, other);
   aa_t *t = (aa_t *)self->impl;
   aa_t *that = (aa_t *)other->impl;
   tree_join_roots_t roots =
       tree_list_prepare_join_roots(&t->tree, &that->tree);
   node_t *root =
       aa_join(roots.left, roots.right, t->tree.size, that->tree.size);
   u64 joined_size = tree_list_join_size(t->tree.size, that->tree.size);
   TREE_LIST_JOIN_RESULT(aa_t, root, joined_size);
}

const list_vtable_t aa_vtable = TREE_LIST_VTABLE("AA", aa);
