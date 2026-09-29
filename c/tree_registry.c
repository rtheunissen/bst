#include "tree_registry.h"

#include "aa.h"
#include "avl_bottomup.h"
#include "avl_relaxed_bottomup.h"
#include "avl_relaxed_topdown.h"
#include "avl_topdown.h"
#include "avl_weak_bottomup.h"
#include "avl_weak_topdown.h"
#include "conc.h"
#include "filesystem.h"
#include "lbst_bottomup.h"
#include "lbst_relaxed.h"
#include "lbst_topdown.h"
#include "llrb.h"
#include "randomized.h"
#include "redblack_bottomup.h"
#include "redblack_relaxed_bottomup.h"
#include "redblack_relaxed_topdown.h"
#include "redblack_topdown.h"
#include "splay.h"
#include "treap_bottomup.h"
#include "treap_fingertree.h"
#include "treap_topdown.h"
#include "wbst_bottomup.h"
#include "wbst_relaxed.h"
#include "wbst_topdown.h"
#include "zip.h"

#include <stdlib.h>
#include <string.h>

const list_vtable_t *const tree_registry_all[] = {
    &avl_bottomup_vtable,
    &avl_topdown_vtable,
    &avl_weak_topdown_vtable,
    &avl_weak_bottomup_vtable,
    &avl_relaxed_topdown_vtable,
    &avl_relaxed_bottomup_vtable,
    &redblack_bottomup_vtable,
    &redblack_topdown_vtable,
    &redblack_relaxed_bottomup_vtable,
    &redblack_relaxed_topdown_vtable,
    &wbst_bottomup_vtable,
    &wbst_topdown_vtable,
    &wbst_relaxed_vtable,
    &lbst_bottomup_vtable,
    &lbst_topdown_vtable,
    &lbst_relaxed_vtable,
    &treap_bottomup_vtable,
    &treap_topdown_vtable,
    &treap_fingertree_vtable,
    &randomized_vtable,
    &zip_vtable,
    &splay_vtable,
    &conc_vtable,
    &llrb_vtable,
    &aa_vtable,
};

const int tree_registry_count =
    sizeof(tree_registry_all) / sizeof(tree_registry_all[0]);

static bool tree_registry_name_is_lookupable(const char *name) {
   return name != NULL && name[0] != '\0' &&
          list_vtable_name_is_component(name);
}

static void tree_registry_require_slice(const list_vtable_t *const *trees,
                                        int count) {
   invariant(count >= 0);
   invariant(count == 0 || trees != NULL);
   for (int i = 0; i < count; i++) {
      const list_vtable_t *vt = trees[i];
      list_vtable_require(vt);
      for (int j = i + 1; j < count; j++) {
         const list_vtable_t *other = trees[j];
         list_vtable_require(other);
         invariant(vt != other);
         invariant(strcmp(vt->name, other->name) != 0);
      }
   }
}

static void tree_registry_require(void) {
   invariant(tree_registry_count > 0);
   tree_registry_require_slice(tree_registry_all, tree_registry_count);
}

static const list_vtable_t *tree_registry_find_lookupable(const char *name) {
   invariant(tree_registry_name_is_lookupable(name));
   tree_registry_require();
   for (int i = 0; i < tree_registry_count; i++) {
      const list_vtable_t *vt = tree_registry_all[i];
      list_vtable_require(vt);
      if (strcmp(name, vt->name) == 0) {
         return vt;
      }
   }
   return NULL;
}

const list_vtable_t *tree_registry_find(const char *name) {
   if (!tree_registry_name_is_lookupable(name)) {
      return NULL;
   }
   return tree_registry_find_lookupable(name);
}

const list_vtable_t *tree_registry_find_or_exit(const char *name) {
   if (name == NULL) {
      fprintf(stderr, "tree name is null\n");
      tree_registry_fprint_names(stderr, "trees");
      exit(2);
   }
   if (name[0] == '\0') {
      fprintf(stderr, "tree name is empty\n");
      tree_registry_fprint_names(stderr, "trees");
      exit(2);
   }
   if (!list_vtable_name_is_component(name)) {
      fprintf(stderr, "tree name is not a path component\n");
      tree_registry_fprint_names(stderr, "trees");
      exit(2);
   }

   const list_vtable_t *vt = tree_registry_find_lookupable(name);
   if (vt == NULL) {
      fprintf(stderr, "unknown tree: %s\n", name);
      tree_registry_fprint_names(stderr, "trees");
      exit(2);
   }
   return vt;
}

void tree_registry_selection_init(tree_registry_selection_t *selection,
                                  const char *name) {
   invariant(selection != NULL);
   *selection = (tree_registry_selection_t){0};

   if (name == NULL) {
      tree_registry_require();
      selection->items = tree_registry_all;
      selection->count = tree_registry_count;
      invariant(selection->count > 0);
      return;
   }

   selection->selected[0] = tree_registry_find_or_exit(name);
   selection->items = selection->selected;
   selection->count = 1;
}

void tree_registry_fprint_names(FILE *file, const char *label) {
   invariant(file != NULL);
   invariant(label != NULL);
   tree_registry_require();

   fprintf(file, "%s:", label);
   for (int i = 0; i < tree_registry_count; i++) {
      const list_vtable_t *vt = tree_registry_all[i];
      list_vtable_require(vt);
      fprintf(file, " %s", vt->name);
   }
   fprintf(file, "\n");
}
