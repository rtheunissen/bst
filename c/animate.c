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
#include "cli.h"
#include "list.h"
#include "operations.h"
#include "random.h"
#include "distributions.h"
#include "tree_registry.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

// ─────────────────────────────────────────────────────────────────────────────
// Dynamic vectors
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   int *data;
   int len;
   int cap;
} vec_int_t;
typedef struct {
   u64 *data;
   int len;
   int cap;
} vec_u64_t;

static int vec_next_cap(int cap, size_t elem_size) {
   invariant(cap >= 0);
   u64 next = bst_array_next_cap((u64)cap, (u64)cap + 1, 64, elem_size);
   invariant(next <= INT_MAX);
   return (int)next;
}

static void *vec_resize_array(void *data, u64 count, size_t elem_size) {
   return data == NULL ? bst_malloc_array(count, elem_size)
                       : bst_realloc_array(data, count, elem_size);
}

static void vec_int_push(vec_int_t *v, int val) {
   invariant(v != NULL);
   invariant(v->len >= 0);
   invariant(v->cap >= 0);
   invariant(v->len <= v->cap);
   if (v->len >= v->cap) {
      invariant(v->cap < INT_MAX);
      int cap = vec_next_cap(v->cap, sizeof(v->data[0]));
      int *data =
          (int *)vec_resize_array(v->data, (u64)cap, sizeof(v->data[0]));
      v->data = data;
      v->cap = cap;
   }
   invariant(v->len < v->cap);
   v->data[v->len++] = val;
}

static void vec_int_clear(vec_int_t *v) {
   invariant(v != NULL);
   v->len = 0;
}

static void vec_int_free(vec_int_t *v) {
   invariant(v != NULL);
   free(v->data);
   v->data = NULL;
   v->len = v->cap = 0;
}

static void vec_int_reverse(vec_int_t *v) {
   invariant(v != NULL);
   invariant(v->len >= 0);
   for (int i = 0, j = v->len - 1; i < j; i++, j--) {
      int tmp = v->data[i];
      v->data[i] = v->data[j];
      v->data[j] = tmp;
   }
}

static void vec_u64_push(vec_u64_t *v, u64 val) {
   invariant(v != NULL);
   invariant(v->len >= 0);
   invariant(v->cap >= 0);
   invariant(v->len <= v->cap);
   if (v->len >= v->cap) {
      invariant(v->cap < INT_MAX);
      int cap = vec_next_cap(v->cap, sizeof(v->data[0]));
      u64 *data =
          (u64 *)vec_resize_array(v->data, (u64)cap, sizeof(v->data[0]));
      v->data = data;
      v->cap = cap;
   }
   invariant(v->len < v->cap);
   v->data[v->len++] = val;
}

static void vec_u64_clear(vec_u64_t *v) {
   invariant(v != NULL);
   v->len = 0;
}

static void vec_u64_free(vec_u64_t *v) {
   invariant(v != NULL);
   free(v->data);
   v->data = NULL;
   v->len = v->cap = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Rendering helpers
// ─────────────────────────────────────────────────────────────────────────────

static void repeat_str(const char *s, int n) {
   invariant(s != NULL);
   invariant(n >= 0);
   for (int i = 0; i < n; i++) {
      fputs(s, stdout);
   }
}

static int display_text_len(const char *text) {
   invariant(text != NULL);
   size_t len = strlen(text);
   invariant(len <= (size_t)INT_MAX);
   return (int)len;
}

static void pad_left(const char *text, int width) {
   int len = display_text_len(text);
   invariant(width >= len);
   for (int i = 0; i < width - len; i++) {
      putchar(' ');
   }
   fputs(text, stdout);
}

static void pad_right(const char *text, int width) {
   int len = display_text_len(text);
   invariant(width >= len);
   fputs(text, stdout);
   for (int i = 0; i < width - len; i++) {
      putchar(' ');
   }
}

static void format_row_number(char *buf, size_t bufsz, int row) {
   invariant(buf != NULL);
   invariant(bufsz > 0);
   invariant(row >= 0);
   int written = snprintf(buf, bufsz, "%d", row);
   invariant(written >= 0);
   invariant((size_t)written < bufsz);
}

static void comma_sep(char *buf, size_t bufsz, u64 n) {
   invariant(buf != NULL);
   invariant(bufsz > 0);
   char tmp[32];
   int written = snprintf(tmp, sizeof(tmp), "%llu", (unsigned long long)n);
   invariant(written >= 0);
   invariant((size_t)written < sizeof(tmp));
   int len = written;
   int commas = (len - 1) / 3;
   int total = len + commas;
   invariant((size_t)total < bufsz);
   int j = total;
   buf[j--] = '\0';
   for (int i = len - 1, k = 0; i >= 0; i--, k++) {
      if (k > 0 && k % 3 == 0) {
         buf[j--] = ',';
      }
      buf[j--] = tmp[i];
   }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tree analysis
// ─────────────────────────────────────────────────────────────────────────────

static void exterior_heights(tree_t *tree, vec_int_t *out_l, vec_int_t *out_r) {
   vec_int_clear(out_l);
   vec_int_clear(out_r);
   if (tree->root == NULL) {
      return;
   }

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
   if (tree->root == NULL) {
      return;
   }

   for (node_t *l = tree->root->l; l != NULL; l = l->l) {
      vec_int_push(out_l, height(l->r) + 1);
   }
   for (node_t *r = tree->root->r; r != NULL; r = r->r) {
      vec_int_push(out_r, height(r->l) + 1);
   }
   vec_int_reverse(out_l);
}

typedef struct {
   node_t *node;
   int level;
} node_level_frame_t;

enum {
   NODE_LEVEL_STACK_INLINE_CAP = 128,
};

static void node_level_stack_push(node_level_frame_t **stack,
                                  node_level_frame_t *inline_stack, u64 *len,
                                  u64 *cap, node_t *node, int level) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(len != NULL);
   invariant(cap != NULL);
   invariant(*stack != NULL);
   invariant(*cap > 0);
   invariant(*len <= *cap);
   invariant(level >= 0);
   if (node == NULL) {
      return;
   }

   if (*len == *cap) {
      *stack = (node_level_frame_t *)stack_array_grow(
          *stack, inline_stack, *len, cap, sizeof(**stack));
   }

   invariant(*len < *cap);
   u64 index = *len;
   *len = size_after_inserting_one(*len);
   (*stack)[index] = (node_level_frame_t){.node = node, .level = level};
}

static node_level_frame_t node_level_stack_pop(node_level_frame_t *stack,
                                               u64 *len) {
   invariant(stack != NULL);
   invariant(len != NULL);
   invariant(*len > 0);
   *len = size_after_removing_one(*len);
   return stack[*len];
}

static void node_level_stack_dispose(node_level_frame_t *stack,
                                     node_level_frame_t *inline_stack, u64 len,
                                     u64 cap) {
   invariant(stack != NULL);
   invariant(inline_stack != NULL);
   invariant(cap > 0);
   invariant(len <= cap);
   invariant(stack == inline_stack || cap > NODE_LEVEL_STACK_INLINE_CAP);
   if (stack != inline_stack) {
      free(stack);
   }
}

static void count_nodes_per_level(node_t *root, vec_u64_t *counter) {
   invariant(counter != NULL);

   node_level_frame_t inline_stack[NODE_LEVEL_STACK_INLINE_CAP];
   node_level_frame_t *stack = inline_stack;
   u64 len = 0;
   u64 cap = NODE_LEVEL_STACK_INLINE_CAP;

   node_level_stack_push(&stack, inline_stack, &len, &cap, root, 0);
   while (len > 0) {
      node_level_frame_t frame = node_level_stack_pop(stack, &len);
      node_t *node = frame.node;
      int level = frame.level;

      while (counter->len <= level) {
         vec_u64_push(counter, 0);
      }
      counter->data[level] = size_after_inserting_one(counter->data[level]);

      invariant(level < INT_MAX);
      int child_level = level + 1;
      node_level_stack_push(&stack, inline_stack, &len, &cap, node->r,
                            child_level);
      node_level_stack_push(&stack, inline_stack, &len, &cap, node->l,
                            child_level);
   }

   node_level_stack_dispose(stack, inline_stack, len, cap);
}

static void nodes_per_level(tree_t *tree, vec_u64_t *out_l, vec_u64_t *out_r) {
   vec_u64_clear(out_l);
   vec_u64_clear(out_r);
   if (tree->root == NULL) {
      return;
   }
   count_nodes_per_level(tree->root->l, out_l);
   count_nodes_per_level(tree->root->r, out_r);
}

// ─────────────────────────────────────────────────────────────────────────────
// Histogram rendering (Exterior/Interior heights)
// ─────────────────────────────────────────────────────────────────────────────

static int max_histogram_bar_width = 80;
static int max_histogram_bar_width_so_far = 0;

static void truncated_bar(const char *ch, int width) {
   invariant(ch != NULL);
   invariant(width >= 0);
   invariant(max_histogram_bar_width >= 0);
   if (width > max_histogram_bar_width) {
      repeat_str(ch, max_histogram_bar_width);
      printf("···");
   } else {
      repeat_str(ch, width);
   }
}

static void render_histogram(const char *title, vec_int_t *series_l,
                             vec_int_t *series_r, int h, int offset) {
   // Viewport clipping
   int numberOfRowsL = series_l->len;
   int numberOfRowsR = series_r->len;

   int capacityL = (int)floor((double)h / 2.0) + offset;
   int capacityR = (int)ceil((double)h / 2.0) - offset;

   // Compute visible slice of left series
   int l_start = MAX(0, MIN(numberOfRowsL, numberOfRowsL - capacityL));
   int l_end_raw = MIN(numberOfRowsL - l_start,
                       MAX(0, numberOfRowsL - l_start + capacityR));
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

   for (int i = 0; i < paddingTop; i++) {
      printf(" │\n");
   }
   for (int i = l_start; i < l_end; i++) {
      int w = series_l->data[i];
      if (w > max_histogram_bar_width_so_far) {
         max_histogram_bar_width_so_far = w;
      }
      printf(" │");
      truncated_bar("░", w);
      printf("▏\n");
   }
   for (int i = r_start; i < r_end; i++) {
      int w = series_r->data[i];
      if (w > max_histogram_bar_width_so_far) {
         max_histogram_bar_width_so_far = w;
      }
      printf(" │");
      truncated_bar("▓", w);
      printf("▏\n");
   }
   for (int i = 0; i < paddingBot; i++) {
      printf(" │\n");
   }

   printf(" └");
   repeat_str("─",
              MIN(max_histogram_bar_width_so_far, max_histogram_bar_width));
   if (max_histogram_bar_width_so_far > max_histogram_bar_width) {
      printf("···");
   }
   printf("┤ %d\n", max_histogram_bar_width_so_far);
}

// ─────────────────────────────────────────────────────────────────────────────
// Stacked histogram rendering (NodesPerLevel)
// ─────────────────────────────────────────────────────────────────────────────

static int max_stacked_height = 128;
static int max_stacked_height_so_far = 0;

static void render_stacked_histogram(const char *title, vec_u64_t *series_l,
                                     vec_u64_t *series_r, int display_width,
                                     int display_height) {
   max_stacked_height_so_far =
       MAX(max_stacked_height_so_far, MAX(series_l->len, series_r->len));
   if (display_height < max_stacked_height_so_far) {
      display_height = max_stacked_height_so_far;
   }
   if (display_height > max_stacked_height) {
      display_height = max_stacked_height;
   }

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
      invariant(display_width >= barWidthL);
      invariant(display_width >= barWidthR);

      char rownum[16];
      format_row_number(rownum, sizeof(rownum), row);

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
   if (total == 0) {
      return;
   }
   int w = (int)(log2((double)total)) + 1;
   int x = (int)((double)focus / (double)total * (double)w);
   if (x >= w) {
      x = w - 1;
   }
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
                           const char *distribution, u64 size) {
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
// Animation state
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
   // Config
   operation_t op;
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
   invariant(a != NULL);
   operation_require(&a->op);
   if (operation_inserts_only(&a->op)) {
      return list_size(&a->instance) < a->op.scale;
   }
   return a->step_count < a->op.steps;
}

static void anim_op_step(animation_t *a) {
   invariant(a != NULL);
   invariant(a->step_count < UINT64_MAX);
   invariant(a->frame_step < UINT64_MAX);

   a->frame_step = size_after_inserting_one(a->frame_step);
   operation_step_t step =
       operation_step(&a->op, a->instance, a->dist, &a->step_count);
   a->instance = step.instance;
   a->last_position = step.position;
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
   invariant(a != NULL);
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
   default:
      bst_unreachable();
   }
}

static void anim_render(animation_t *a) {
   invariant(a != NULL);
   // Clear screen
   printf("\x1b[2J\x1b[H");

   switch (a->anim_type) {
   case ANIM_EXTERIOR_HEIGHTS:
   case ANIM_INTERIOR_HEIGHTS:
      printf("\n");
      render_histogram(anim_type_titles[a->anim_type], &a->heights[0],
                       &a->heights[1], a->viewport_height, a->viewport_offset);
      printf("\n");
      render_focus_bar(a->last_position, list_size(&a->instance));
      printf("\n");
      render_details(a->tree_vt->name, a->op.name, a->dist_name,
                     list_size(&a->instance));
      printf("\n");
      break;
   case ANIM_NODES_PER_LEVEL: {
      int w = list_size(&a->instance) > 0
                  ? (int)util_log2(list_size(&a->instance)) + 1
                  : 1;
      printf("\n");
      render_stacked_histogram(anim_type_titles[a->anim_type], &a->counts[0],
                               &a->counts[1], w, a->viewport_height);
      printf("\n");
      render_focus_bar(a->last_position, list_size(&a->instance));
      printf("\n");
      render_details(a->tree_vt->name, a->op.name, a->dist_name,
                     list_size(&a->instance));
      printf("\n");
      break;
   }
   default:
      bst_unreachable();
   }
   fflush(stdout);
}

static void anim_free(animation_t *a) {
   invariant(a != NULL);
   list_free(&a->instance);
   vec_int_free(&a->heights[0]);
   vec_int_free(&a->heights[1]);
   vec_u64_free(&a->counts[0]);
   vec_u64_free(&a->counts[1]);
   dist_destroy(a->dist);
}

static int viewport_offset_min(int height) {
   invariant(height >= 0);
   return -(height / 2);
}

static int viewport_offset_max(int height) {
   invariant(height >= 0);
   return height / 2;
}

static void anim_clamp_viewport_offset(animation_t *a) {
   invariant(a != NULL);
   invariant(a->viewport_height >= 0);
   a->viewport_offset =
       MAX(viewport_offset_min(a->viewport_height),
           MIN(viewport_offset_max(a->viewport_height), a->viewport_offset));
}

static void anim_move_viewport_offset(animation_t *a, int delta) {
   invariant(a != NULL);
   invariant(delta == -1 || delta == 1);
   anim_clamp_viewport_offset(a);
   int min_offset = viewport_offset_min(a->viewport_height);
   int max_offset = viewport_offset_max(a->viewport_height);
   if (delta < 0 && a->viewport_offset > min_offset) {
      a->viewport_offset--;
   } else if (delta > 0 && a->viewport_offset < max_offset) {
      a->viewport_offset++;
   }
}

static void anim_increase_viewport_height(animation_t *a) {
   invariant(a != NULL);
   invariant(a->viewport_height >= 0);
   if (a->viewport_height < INT_MAX) {
      a->viewport_height++;
   }
   anim_clamp_viewport_offset(a);
}

static void anim_decrease_viewport_height(animation_t *a) {
   invariant(a != NULL);
   invariant(a->viewport_height >= 0);
   if (a->viewport_height > 0) {
      a->viewport_height--;
   }
   anim_clamp_viewport_offset(a);
}

// ─────────────────────────────────────────────────────────────────────────────
// Menu system (cooked terminal mode)
// ─────────────────────────────────────────────────────────────────────────────

static const char **menu_options_alloc(int count) {
   invariant(count > 0);
   return (const char **)bst_malloc_array((u64)count, sizeof(const char *));
}

static void discard_line_tail(void) {
   int c;
   while ((c = getchar()) != '\n' && c != EOF) {
   }
}

static int choose_menu(const char *prompt, const char **options, int count) {
   invariant(prompt != NULL);
   invariant(options != NULL);
   invariant(count > 0);
   char line[64];

   for (;;) {
      printf("\n\n");
      for (int i = 0; i < count; i++) {
         printf(" (%2d) \x1b[1m%s\x1b[22m\n", i + 1, options[i]);
      }
      printf("\n \x1b[7m%s:\x1b[27m ", prompt);
      fflush(stdout);

      if (fgets(line, sizeof(line), stdin) == NULL) {
         return -1;
      }
      bool complete_line = strchr(line, '\n') != NULL;
      if (!complete_line) {
         discard_line_tail();
      }
      printf("\n");

      int choice = 0;
      if (complete_line && cli_parse_choice_line(line, count, &choice)) {
         return choice;
      }
   }
}

// ─────────────────────────────────────────────────────────────────────────────
// Terminal raw mode and key reading
// ─────────────────────────────────────────────────────────────────────────────

static struct termios orig_termios;
static bool raw_mode_active = false;
static bool raw_mode_atexit_registered = false;

static bool require_interactive_terminal(void) {
   if (!isatty(STDIN_FILENO)) {
      fprintf(stderr, "animate requires stdin to be an interactive terminal\n");
      return false;
   }
   if (!isatty(STDOUT_FILENO)) {
      fprintf(stderr,
              "animate requires stdout to be an interactive terminal\n");
      return false;
   }
   return true;
}

static void disable_raw_mode(void) {
   if (!raw_mode_active) {
      return;
   }
   if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) != 0) {
      perror("tcsetattr");
   }
   raw_mode_active = false;
}

static void enable_raw_mode(void) {
   if (tcgetattr(STDIN_FILENO, &orig_termios) != 0) {
      perror("tcgetattr");
      exit(2);
   }
   if (!raw_mode_atexit_registered) {
      if (atexit(disable_raw_mode) != 0) {
         fprintf(stderr, "failed to register terminal restore hook\n");
         exit(2);
      }
      raw_mode_atexit_registered = true;
   }
   struct termios raw = orig_termios;
   raw.c_iflag &= ~(IXON);
   raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
   raw.c_cc[VMIN] = 1;
   raw.c_cc[VTIME] = 0;
   if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
      perror("tcsetattr");
      exit(2);
   }
   raw_mode_active = true;
}

static bool char_available(void) {
   for (;;) {
      struct timeval tv = {0, 100000}; // 100ms
      fd_set fds;
      FD_ZERO(&fds);
      FD_SET(STDIN_FILENO, &fds);
      int ready = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
      if (ready > 0) {
         return FD_ISSET(STDIN_FILENO, &fds);
      }
      if (ready == 0) {
         return false;
      }
      if (errno == EINTR) {
         continue;
      }
      perror("select");
      exit(2);
   }
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

static bool read_key_byte(unsigned char *out) {
   invariant(out != NULL);
   for (;;) {
      ssize_t n = read(STDIN_FILENO, out, 1);
      if (n == 1) {
         return true;
      }
      if (n == 0) {
         return false;
      }
      if (errno == EINTR) {
         continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
         return false;
      }
      perror("read");
      exit(2);
   }
}

static key_t read_key(void) {
   unsigned char c;
   if (!read_key_byte(&c)) {
      return KEY_NONE;
   }

   // ESC or Ctrl-C/D/Z/Q
   if (c == 3 || c == 4 || c == 26 || c == 17) {
      return KEY_EXIT;
   }

   if (c == 27) { // ESC
      if (!char_available()) {
         return KEY_EXIT;
      }
      unsigned char seq[2];
      if (!read_key_byte(&seq[0])) {
         return KEY_EXIT;
      }
      if (seq[0] != '[') {
         return KEY_EXIT;
      }
      if (!char_available()) {
         return KEY_EXIT;
      }
      if (!read_key_byte(&seq[1])) {
         return KEY_EXIT;
      }
      switch (seq[1]) {
      case 'A':
         return KEY_ARROW_UP;
      case 'B':
         return KEY_ARROW_DOWN;
      case 'C':
         return KEY_ARROW_RIGHT;
      case 'D':
         return KEY_ARROW_LEFT;
      default:
         break;
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
   if (!require_interactive_terminal()) {
      return 2;
   }

   // ── Menus (cooked mode) ──────────────────────────────────────────────

   operation_t operations[operation_default_count];
   int operation_count = operation_defaults(operations);

   // Choose operation
   const char **op_names = menu_options_alloc(operation_count);
   for (int i = 0; i < operation_count; i++) {
      op_names[i] = operations[i].name;
   }
   int op_idx = choose_menu("Operation", op_names, operation_count);
   free(op_names);
   if (op_idx < 0) {
      return 2;
   }

   // Choose strategy
   const char **tree_names = menu_options_alloc(tree_registry_count);
   for (int i = 0; i < tree_registry_count; i++) {
      list_vtable_require(tree_registry_all[i]);
      tree_names[i] = tree_registry_all[i]->name;
   }
   int tree_idx = choose_menu("Strategy", tree_names, tree_registry_count);
   free(tree_names);
   if (tree_idx < 0) {
      return 2;
   }

   // Choose distribution
   const char **dist_options = menu_options_alloc(dist_animation_count);
   for (int i = 0; i < dist_animation_count; i++) {
      invariant(dist_animation[i] != NULL);
      invariant(dist_animation[i]->name != NULL);
      invariant(dist_animation[i]->factory != NULL);
      dist_options[i] = dist_animation[i]->name;
   }
   int dist_idx =
       choose_menu("Distribution", dist_options, dist_animation_count);
   free(dist_options);
   if (dist_idx < 0) {
      return 2;
   }

   // Choose animation type
   int anim_idx = choose_menu("Animation", anim_type_names, num_anim_types);
   if (anim_idx < 0) {
      return 2;
   }

   // ── Set up arena ─────────────────────────────────────────────────────

   arena_t arena;
   arena_init(&arena, 2000000); // 2M nodes initially, grows as needed
   arena_t *previous_arena = arena_install(&arena);

   // ── Initialize animation ─────────────────────────────────────────────

   animation_t anim = {0};
   anim.op = operations[op_idx];
   anim.tree_vt = tree_registry_all[tree_idx];
   anim.dist_name = dist_animation[dist_idx]->name;
   anim.anim_type = anim_idx;
   anim.viewport_height = 40;
   anim.viewport_offset = 0;
   anim.step_count = 0;
   anim.frame_step = 0;
   anim.last_position = 0;

   // Create tree instance and distribution
   anim.instance = list_new(anim.tree_vt);
   anim.dist = dist_create(dist_animation[dist_idx], random_uint64());
   invariant(anim.dist != NULL);

   // ── Animation loop (raw mode) ────────────────────────────────────────

   enable_raw_mode();

   while (anim_valid(&anim)) {
      anim_render(&anim);

      key_t key = read_key();
      switch (key) {
      case KEY_ARROW_LEFT:
         anim_move_viewport_offset(&anim, -1);
         break;
      case KEY_ARROW_RIGHT:
         anim_move_viewport_offset(&anim, 1);
         break;
      case KEY_ARROW_UP:
         anim_increase_viewport_height(&anim);
         break;
      case KEY_ARROW_DOWN:
         anim_decrease_viewport_height(&anim);
         break;
      case KEY_EXIT:
         goto done;
      case KEY_NONE:
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
   arena_restore(previous_arena);
   arena_destroy(&arena);

   return 0;
}
