// Interactive terminal animation of BST operations.
//
// Translated from:
//   main/animate/animate.go
//   trees/animations/animation.go
//   trees/animations/exterior_heights.go
//   trees/animations/interior_heights.go
//   trees/animations/nodes_per_level.go
//   trees/animations/text/*.go
//
// Controls:
//   Any key      advance to next frame
//   ← (Left)     scroll up (decrease viewport offset)
//   → (Right)    scroll down (increase viewport offset)
//   ↑ (Up)       increase viewport height
//   ↓ (Down)     decrease viewport height
//   ESC/Ctrl-C   exit
//
#include "bst.h"
#include "list.h"
#include "random.h"
#include "distributions.h"
#include "splay.h"
#include "randomized.h"
#include "zip.h"
#include "treap_bottomup.h"
#include "treap_topdown.h"
#include "conc.h"
#include "llrb.h"
#include "aa.h"
#include "avl_bottomup.h"
#include "avl_topdown.h"
#include "avl_weak_bottomup.h"
#include "avl_weak_topdown.h"
#include "avl_relaxed_bottomup.h"
#include "avl_relaxed_topdown.h"
#include "redblack_bottomup.h"
#include "redblack_topdown.h"
#include "redblack_relaxed_bottomup.h"
#include "redblack_relaxed_topdown.h"
#include "wbst_bottomup.h"
#include "wbst_topdown.h"
#include "wbst_relaxed.h"
#include "lbst_bottomup.h"
#include "lbst_topdown.h"
#include "lbst_relaxed.h"
#include "treap_fingertree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>


// ─────────────────────────────────────────────────────────────────────────────
// Dynamic vectors
// ─────────────────────────────────────────────────────────────────────────────

typedef struct { int *data; int len; int cap; } vec_int_t;
typedef struct { u64 *data; int len; int cap; } vec_u64_t;

static void vec_int_push(vec_int_t *v, int val) {
   if (v->len >= v->cap) {
      v->cap = v->cap ? v->cap * 2 : 64;
      v->data = (int *)realloc(v->data, (size_t)v->cap * sizeof(int));
   }
   v->data[v->len++] = val;
}

static void vec_int_clear(vec_int_t *v) { v->len = 0; }

static void vec_int_free(vec_int_t *v) {
   free(v->data);
   v->data = NULL;
   v->len = v->cap = 0;
}

static void vec_int_reverse(vec_int_t *v) {
   for (int i = 0, j = v->len - 1; i < j; i++, j--) {
      int tmp = v->data[i];
      v->data[i] = v->data[j];
      v->data[j] = tmp;
   }
}

static void vec_u64_push(vec_u64_t *v, u64 val) {
   if (v->len >= v->cap) {
      v->cap = v->cap ? v->cap * 2 : 64;
      v->data = (u64 *)realloc(v->data, (size_t)v->cap * sizeof(u64));
   }
   v->data[v->len++] = val;
}

static void vec_u64_clear(vec_u64_t *v) { v->len = 0; }

static void vec_u64_free(vec_u64_t *v) {
   free(v->data);
   v->data = NULL;
   v->len = v->cap = 0;
}


// ─────────────────────────────────────────────────────────────────────────────
// Rendering helpers
// ─────────────────────────────────────────────────────────────────────────────

static void repeat_str(const char *s, int n) {
   for (int i = 0; i < n; i++) fputs(s, stdout);
}

static void pad_left(const char *text, int width) {
   int len = (int)strlen(text);
   for (int i = 0; i < width - len; i++) putchar(' ');
   fputs(text, stdout);
}

static void pad_right(const char *text, int width) {
   int len = (int)strlen(text);
   fputs(text, stdout);
   for (int i = 0; i < width - len; i++) putchar(' ');
}

static void comma_sep(char *buf, size_t bufsz, u64 n) {
   char tmp[32];
   snprintf(tmp, sizeof(tmp), "%llu", (unsigned long long)n);
   int len = (int)strlen(tmp);
   int commas = (len - 1) / 3;
   int total = len + commas;
   if ((size_t)total >= bufsz) { snprintf(buf, bufsz, "%s", tmp); return; }
   int j = total;
   buf[j--] = '\0';
   for (int i = len - 1, k = 0; i >= 0; i--, k++) {
      if (k > 0 && k % 3 == 0) buf[j--] = ',';
      buf[j--] = tmp[i];
   }
}


// ─────────────────────────────────────────────────────────────────────────────
// Tree analysis
// ─────────────────────────────────────────────────────────────────────────────

static tree_t *list_tree(list_t *l) {
   return (tree_t *)l->impl;
}

static void exterior_heights(tree_t *tree, vec_int_t *out_l, vec_int_t *out_r) {
   vec_int_clear(out_l);
   vec_int_clear(out_r);
   if (tree->root == NULL) return;

   for (node_t *l = tree->root->l; l != NULL; l = l->l) {
      vec_int_push(out_l, height(l));
   }
   for (node_t *r = tree->root->r; r != NULL; r = r->r) {
      vec_int_push(out_r, height(r));
   }
   vec_int_reverse(out_l);
}

static void interior_heights(tree_t *tree, vec_int_t *out_l, vec_int_t *out_r) {
   vec_int_clear(out_l);
   vec_int_clear(out_r);
   if (tree->root == NULL) return;

   for (node_t *l = tree->root->l; l != NULL; l = l->l) {
      vec_int_push(out_l, height(l->r) + 1);
   }
   for (node_t *r = tree->root->r; r != NULL; r = r->r) {
      vec_int_push(out_r, height(r->l) + 1);
   }
   vec_int_reverse(out_l);
}

static void count_nodes_per_level(node_t *p, vec_u64_t *counter, int level) {
   if (p == NULL) return;
   while (counter->len <= level) {
      vec_u64_push(counter, 0);
   }
   counter->data[level]++;
   count_nodes_per_level(p->l, counter, level + 1);
   count_nodes_per_level(p->r, counter, level + 1);
}

static void nodes_per_level(tree_t *tree, vec_u64_t *out_l, vec_u64_t *out_r) {
   vec_u64_clear(out_l);
   vec_u64_clear(out_r);
   if (tree->root == NULL) return;
   count_nodes_per_level(tree->root->l, out_l, 0);
   count_nodes_per_level(tree->root->r, out_r, 0);
}


// ─────────────────────────────────────────────────────────────────────────────
// Histogram rendering (Exterior/Interior heights)
// ─────────────────────────────────────────────────────────────────────────────

static int max_histogram_bar_width = 80;
static int max_histogram_bar_width_so_far = 0;

static void truncated_bar(const char *ch, int width) {
   if (width > max_histogram_bar_width) {
      repeat_str(ch, max_histogram_bar_width);
      printf("···");
   } else {
      repeat_str(ch, width);
   }
}

static void render_histogram(const char *title,
                             vec_int_t *series_l, vec_int_t *series_r,
                             int h, int offset)
{
   // Viewport clipping
   int numberOfRowsL = series_l->len;
   int numberOfRowsR = series_r->len;

   int capacityL = (int)floor((double)h / 2.0) + offset;
   int capacityR = (int)ceil((double)h / 2.0) - offset;

   // Compute visible slice of left series
   int l_start = MAX(0, MIN(numberOfRowsL, numberOfRowsL - capacityL));
   int l_end_raw = MIN(numberOfRowsL - l_start, MAX(0, numberOfRowsL - l_start + capacityR));
   int l_end = l_start + l_end_raw;

   // Compute visible slice of right series
   int r_start = MIN(numberOfRowsR, MAX(0, -capacityL));
   int r_end_raw = MIN(numberOfRowsR - r_start, MAX(0, capacityR));
   int r_end = r_start + r_end_raw;

   int visible_l = l_end - l_start;
   int visible_r = r_end - r_start;

   // Padding to keep centered
   int paddingTop = 0;
   int paddingBot = 0;
   int emptySpace = h - (visible_l + visible_r);
   if (emptySpace > 0) {
      paddingTop = MAX(0, MIN(emptySpace, capacityL - numberOfRowsL));
      paddingBot = MAX(0, MIN(emptySpace, capacityR - numberOfRowsR));
   }

   printf("\n");
   printf(" ┌ %s\n", title);

   for (int i = paddingTop; i >= 0; i--) {
      printf(" │\n");
   }
   for (int i = l_start; i < l_end; i++) {
      int w = series_l->data[i];
      if (w > max_histogram_bar_width_so_far) max_histogram_bar_width_so_far = w;
      printf(" │");
      truncated_bar("░", w);
      printf("▏\n");
   }
   for (int i = r_start; i < r_end; i++) {
      int w = series_r->data[i];
      if (w > max_histogram_bar_width_so_far) max_histogram_bar_width_so_far = w;
      printf(" │");
      truncated_bar("▓", w);
      printf("▏\n");
   }
   for (int i = paddingBot; i >= 0; i--) {
      printf(" │\n");
   }

   printf(" └");
   repeat_str("─", max_histogram_bar_width_so_far);
   printf("┤ %d\n", max_histogram_bar_width_so_far);
}


// ─────────────────────────────────────────────────────────────────────────────
// Stacked histogram rendering (NodesPerLevel)
// ─────────────────────────────────────────────────────────────────────────────

static int max_stacked_height = 128;
static int max_stacked_height_so_far = 0;

static void render_stacked_histogram(const char *title,
                                     vec_u64_t *series_l, vec_u64_t *series_r,
                                     int display_width, int display_height)
{
   max_stacked_height_so_far = MAX(max_stacked_height_so_far, MAX(series_l->len, series_r->len));
   if (display_height < max_stacked_height_so_far)
      display_height = max_stacked_height_so_far;
   if (display_height > max_stacked_height)
      display_height = max_stacked_height;

   printf(" ┌ %s\n", title);
   printf(" │\n");

   for (int row = 0; row <= display_height; row++) {
      int barWidthL = 0;
      int barWidthR = 0;
      if (row < series_l->len && series_l->data[row] > 0) {
         barWidthL = (int)(log2((double)series_l->data[row])) + 1;
      }
      if (row < series_r->len && series_r->data[row] > 0) {
         barWidthR = (int)(log2((double)series_r->data[row])) + 1;
      }

      char rownum[16];
      snprintf(rownum, sizeof(rownum), "%d", row);

      printf(" │");
      pad_left(rownum, 4);
      repeat_str(" ", display_width - barWidthL + 1);
      if (barWidthL > 0) {
         printf("▕");
         repeat_str("░", barWidthL);
      } else {
         printf(" ");
      }
      if (barWidthR > 0) {
         repeat_str("▓", barWidthR);
         printf("▏");
      }
      printf("\n");
   }
   printf(" └\n");
}


// ─────────────────────────────────────────────────────────────────────────────
// Focus bar
// ─────────────────────────────────────────────────────────────────────────────

static void render_focus_bar(u64 focus, u64 total) {
   if (total == 0) return;
   int w = (int)(log2((double)total)) + 1;
   int x = (int)((double)focus / (double)total * (double)w);
   if (x > w) x = 0;
   printf(" Access: ");
   repeat_str("░", x);
   repeat_str("▓", 1);
   repeat_str("░", w - x - 1);
   printf(" ");
}


// ─────────────────────────────────────────────────────────────────────────────
// Details
// ─────────────────────────────────────────────────────────────────────────────

static void render_details(const char *strategy, const char *operation,
                           const char *distribution, u64 size)
{
   const char *labels[] = {"Strategy", "Operation", "Distribution", "Size"};
   int max_len = 12; // "Distribution" length
   char sizebuf[32];
   comma_sep(sizebuf, sizeof(sizebuf), size);
   const char *values[] = {strategy, operation, distribution, sizebuf};

   for (int i = 0; i < 4; i++) {
      printf("\n ");
      pad_right(labels[i], max_len + 2);
      printf(" %s", values[i]);
   }
   printf("\n");
}


// ─────────────────────────────────────────────────────────────────────────────
// Animation types
// ─────────────────────────────────────────────────────────────────────────────

enum { ANIM_EXTERIOR_HEIGHTS, ANIM_INTERIOR_HEIGHTS, ANIM_NODES_PER_LEVEL };

static const char *anim_type_names[] = {
   "ExteriorHeights",
   "InteriorHeights",
   "NodesPerLevel",
};
static const int num_anim_types = 3;

static const char *anim_type_titles[] = {
   "Exterior heights along the spines",
   "Interior heights along the spines",
   "Number of nodes per level, log2",
};


// ─────────────────────────────────────────────────────────────────────────────
// Operations
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
   ANIM_OP_INSERT,
   ANIM_OP_INSERT_PERSISTENT,
   ANIM_OP_INSERT_DELETE,
   ANIM_OP_INSERT_DELETE_PERSISTENT,
} anim_op_type_t;

typedef struct {
   anim_op_type_t type;
   const char *name;
   u64 scale;
   u64 total_steps;
} anim_op_t;

static const anim_op_t anim_ops[] = {
   {ANIM_OP_INSERT,                  "Insert",                 1000000, 1000000},
   {ANIM_OP_INSERT_PERSISTENT,       "InsertPersistent",       1000000, 1000000},
   {ANIM_OP_INSERT_DELETE,           "InsertDelete",             10000, 1000000},
   {ANIM_OP_INSERT_DELETE_PERSISTENT,"InsertDeletePersistent",   10000, 1000000},
};
static const int num_anim_ops = 4;


// ─────────────────────────────────────────────────────────────────────────────
// Tree and distribution tables
// ─────────────────────────────────────────────────────────────────────────────

static const list_vtable_t *anim_trees[] = {
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
static const int num_anim_trees = sizeof(anim_trees) / sizeof(anim_trees[0]);

static const char *anim_dist_names[] = {
   "Uniform", "Normal", "Skewed", "Zipf", "Minimum",
   "Maximum", "Median", "Queue", "Ascending", "Descending",
};
static const dist_factory_fn anim_dist_factories[] = {
   uniform_new, normal_new, skewed_new, zipf_new, minimum_new,
   maximum_new, median_new, queue_new, ascending_new, descending_new,
};
static const int num_anim_dists = 10;


// ─────────────────────────────────────────────────────────────────────────────
// Animation state
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   // Config
   anim_op_t op;
   const list_vtable_t *tree_vt;
   const char *dist_name;
   int anim_type;

   // Instance
   list_t instance;
   distribution_t *dist;

   // Viewport
   int viewport_height;
   int viewport_offset;

   // State
   u64 step_count;
   u64 frame_step;
   u64 last_position;

   // Analysis data
   vec_int_t heights[2];
   vec_u64_t counts[2];
} animation_t;

static bool anim_valid(animation_t *a) {
   switch (a->op.type) {
   case ANIM_OP_INSERT:
   case ANIM_OP_INSERT_PERSISTENT:
      return list_size(&a->instance) < a->op.scale;
   case ANIM_OP_INSERT_DELETE:
   case ANIM_OP_INSERT_DELETE_PERSISTENT:
      return a->step_count < a->op.total_steps;
   }
   return false;
}

static void anim_op_step(animation_t *a) {
   a->step_count++;
   a->frame_step++;

   switch (a->op.type) {
   case ANIM_OP_INSERT: {
      u64 i = dist_less_than(a->dist, list_size(&a->instance) + 1);
      list_insert(&a->instance, i, 0);
      a->last_position = i;
      break;
   }
   case ANIM_OP_INSERT_PERSISTENT: {
      list_t clone = list_clone(&a->instance);
      u64 i = dist_less_than(a->dist, list_size(&clone) + 1);
      list_insert(&clone, i, 0);
      list_free(&a->instance);
      a->instance = clone;
      a->last_position = i;
      break;
   }
   case ANIM_OP_INSERT_DELETE: {
      if (list_size(&a->instance) < a->op.scale || a->step_count % 2 == 0) {
         u64 i = dist_less_than(a->dist, list_size(&a->instance) + 1);
         list_insert(&a->instance, i, 0);
         a->last_position = i;
      } else {
         u64 i = dist_less_than(a->dist, list_size(&a->instance));
         list_delete(&a->instance, list_size(&a->instance) - i - 1);
         a->last_position = i;
      }
      break;
   }
   case ANIM_OP_INSERT_DELETE_PERSISTENT: {
      list_t clone = list_clone(&a->instance);
      if (list_size(&clone) < a->op.scale || a->step_count % 2 == 0) {
         u64 i = dist_less_than(a->dist, list_size(&clone) + 1);
         list_insert(&clone, i, 0);
         a->last_position = i;
      } else {
         u64 i = dist_less_than(a->dist, list_size(&clone));
         list_delete(&clone, list_size(&clone) - i - 1);
         a->last_position = i;
      }
      list_free(&a->instance);
      a->instance = clone;
      break;
   }
   }
}

//     1 to   100 : Render every step
//   100 to  1000 : Render every 10th
//  1000 to 10000 : Render every 100th etc.
static bool should_render_frame(animation_t *a) {
   double nextLog10 = ceil(log10((double)(list_size(&a->instance) + 1)));
   double nextPow10 = pow(10.0, nextLog10);
   u64 skipSteps = (u64)(nextPow10 / 1000.0);
   return skipSteps == 0 || a->frame_step % skipSteps == 0;
}

static void anim_advance(animation_t *a) {
   while (anim_valid(a)) {
      anim_op_step(a);
      if (should_render_frame(a)) {
         return;
      }
   }
}

static void anim_update_analysis(animation_t *a) {
   tree_t *tree = list_tree(&a->instance);
   switch (a->anim_type) {
   case ANIM_EXTERIOR_HEIGHTS:
      exterior_heights(tree, &a->heights[0], &a->heights[1]);
      break;
   case ANIM_INTERIOR_HEIGHTS:
      interior_heights(tree, &a->heights[0], &a->heights[1]);
      break;
   case ANIM_NODES_PER_LEVEL:
      nodes_per_level(tree, &a->counts[0], &a->counts[1]);
      break;
   }
}

static void anim_render(animation_t *a) {
   // Clear screen
   printf("\x1b[2J\x1b[H");

   switch (a->anim_type) {
   case ANIM_EXTERIOR_HEIGHTS:
   case ANIM_INTERIOR_HEIGHTS:
      printf("\n");
      render_histogram(
         anim_type_titles[a->anim_type],
         &a->heights[0], &a->heights[1],
         a->viewport_height, a->viewport_offset);
      printf("\n");
      render_focus_bar(a->last_position, list_size(&a->instance));
      printf("\n");
      render_details(a->tree_vt->name, a->op.name,
                     a->dist_name, list_size(&a->instance));
      printf("\n");
      break;
   case ANIM_NODES_PER_LEVEL: {
      int w = list_size(&a->instance) > 0
            ? (int)util_log2(list_size(&a->instance)) + 1
            : 1;
      printf("\n");
      render_stacked_histogram(
         anim_type_titles[a->anim_type],
         &a->counts[0], &a->counts[1],
         w, a->viewport_height);
      printf("\n");
      render_focus_bar(a->last_position, list_size(&a->instance));
      printf("\n");
      render_details(a->tree_vt->name, a->op.name,
                     a->dist_name, list_size(&a->instance));
      printf("\n");
      break;
   }
   }
   fflush(stdout);
}

static void anim_free(animation_t *a) {
   vec_int_free(&a->heights[0]);
   vec_int_free(&a->heights[1]);
   vec_u64_free(&a->counts[0]);
   vec_u64_free(&a->counts[1]);
   dist_destroy(a->dist);
}


// ─────────────────────────────────────────────────────────────────────────────
// Menu system (cooked terminal mode)
// ─────────────────────────────────────────────────────────────────────────────

static int choose_menu(const char *prompt, const char **options, int count) {
   printf("\n\n");
   for (int i = 0; i < count; i++) {
      printf(" (%2d) \x1b[1m%s\x1b[22m\n", i + 1, options[i]);
   }
   printf("\n \x1b[7m%s:\x1b[27m ", prompt);
   fflush(stdout);

   int choice = 1;
   if (scanf("%d", &choice) != 1) choice = 1;
   // Clear the rest of the input line
   int c;
   while ((c = getchar()) != '\n' && c != EOF) {}
   printf("\n");

   if (choice < 1 || choice > count) {
      return choose_menu(prompt, options, count);
   }
   return choice - 1;
}


// ─────────────────────────────────────────────────────────────────────────────
// Terminal raw mode and key reading
// ─────────────────────────────────────────────────────────────────────────────

static struct termios orig_termios;

static void disable_raw_mode(void) {
   tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void enable_raw_mode(void) {
   tcgetattr(STDIN_FILENO, &orig_termios);
   atexit(disable_raw_mode);
   struct termios raw = orig_termios;
   raw.c_lflag &= ~(ECHO | ICANON | ISIG);
   raw.c_cc[VMIN] = 1;
   raw.c_cc[VTIME] = 0;
   tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static bool char_available(void) {
   struct timeval tv = {0, 100000}; // 100ms
   fd_set fds;
   FD_ZERO(&fds);
   FD_SET(STDIN_FILENO, &fds);
   return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

typedef enum {
   KEY_NONE,
   KEY_ARROW_UP,
   KEY_ARROW_DOWN,
   KEY_ARROW_LEFT,
   KEY_ARROW_RIGHT,
   KEY_EXIT,
   KEY_OTHER,
} key_t;

static key_t read_key(void) {
   unsigned char c;
   if (read(STDIN_FILENO, &c, 1) != 1) return KEY_NONE;

   // ESC or Ctrl-C/D/Z/Q
   if (c == 3 || c == 4 || c == 26 || c == 17) return KEY_EXIT;

   if (c == 27) { // ESC
      if (!char_available()) return KEY_EXIT;
      unsigned char seq[2];
      if (read(STDIN_FILENO, &seq[0], 1) != 1) return KEY_EXIT;
      if (seq[0] != '[') return KEY_EXIT;
      if (!char_available()) return KEY_EXIT;
      if (read(STDIN_FILENO, &seq[1], 1) != 1) return KEY_EXIT;
      switch (seq[1]) {
         case 'A': return KEY_ARROW_UP;
         case 'B': return KEY_ARROW_DOWN;
         case 'C': return KEY_ARROW_RIGHT;
         case 'D': return KEY_ARROW_LEFT;
      }
      return KEY_EXIT;
   }
   return KEY_OTHER;
}


// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────

int main(void) {
   random_seed(1);

   // ── Menus (cooked mode) ──────────────────────────────────────────────

   // Choose operation
   const char *op_names[4];
   for (int i = 0; i < num_anim_ops; i++) op_names[i] = anim_ops[i].name;
   int op_idx = choose_menu("Operation", op_names, num_anim_ops);

   // Choose strategy
   const char *tree_names[25];
   for (int i = 0; i < num_anim_trees; i++) tree_names[i] = anim_trees[i]->name;
   int tree_idx = choose_menu("Strategy", tree_names, num_anim_trees);

   // Choose distribution
   int dist_idx = choose_menu("Distribution", anim_dist_names, num_anim_dists);

   // Choose animation type
   int anim_idx = choose_menu("Animation", anim_type_names, num_anim_types);

   // ── Set up arena ─────────────────────────────────────────────────────

   arena_t arena;
   arena_init(&arena, 2000000); // 2M nodes initially, grows as needed
   global_arena = &arena;

   // ── Initialize animation ─────────────────────────────────────────────

   animation_t anim = {0};
   anim.op = anim_ops[op_idx];
   anim.tree_vt = anim_trees[tree_idx];
   anim.dist_name = anim_dist_names[dist_idx];
   anim.anim_type = anim_idx;
   anim.viewport_height = 40;
   anim.viewport_offset = 0;
   anim.step_count = 0;
   anim.frame_step = 0;
   anim.last_position = 0;

   // Create tree instance and distribution
   anim.instance = list_new(anim.tree_vt);
   anim.dist = anim_dist_factories[dist_idx](random_uint64());

   // ── Animation loop (raw mode) ────────────────────────────────────────

   enable_raw_mode();

   while (anim_valid(&anim)) {
      anim_render(&anim);

      key_t key = read_key();
      switch (key) {
      case KEY_ARROW_LEFT:
         anim.viewport_offset = MAX(-anim.viewport_height / 2, anim.viewport_offset - 1);
         break;
      case KEY_ARROW_RIGHT:
         anim.viewport_offset = MIN(+anim.viewport_height / 2, anim.viewport_offset + 1);
         break;
      case KEY_ARROW_UP:
         anim.viewport_height++;
         break;
      case KEY_ARROW_DOWN:
         anim.viewport_height = MAX(0, anim.viewport_height - 1);
         break;
      case KEY_EXIT:
         goto done;
      case KEY_OTHER:
      default:
         anim_advance(&anim);
         anim_update_analysis(&anim);
         break;
      }
   }

done:
   disable_raw_mode();
   printf("\x1b[2J\x1b[H"); // clear screen
   fflush(stdout);

   anim_free(&anim);
   global_arena = NULL;
   arena_destroy(&arena);

   return 0;
}
