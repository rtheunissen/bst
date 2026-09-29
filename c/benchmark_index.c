#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "benchmark_index.h"
#include "filesystem.h"

typedef struct {
   char **items;
   size_t len;
   size_t cap;
} string_list_t;

typedef struct {
   char *name;
   int is_dir;
} entry_t;

typedef struct {
   entry_t *items;
   size_t len;
   size_t cap;
} entry_list_t;

typedef struct {
   const char *path;
   char *content;
   size_t size;
} inline_file_t;

typedef struct {
   inline_file_t *items;
   size_t len;
} inline_file_list_t;

typedef struct {
   const char *template_path;
   const char *root_path;
} benchmark_index_config_t;

#ifndef BENCHMARK_INDEX_NO_MAIN
static const char *default_template_path =
    "docs/benchmarks/index.template.html";
static const char *default_root_path = "docs/benchmarks";
#endif
static const char *range_marker = "{{ range $_, $file := .files }}";
static const char *end_marker = "{{ end }}";
static const char *directory_marker = "{{ .directory }}";
static const char *inline_marker = "{{ inline $file }}";

#if defined(__clang__) || defined(__GNUC__)
#define BENCHMARK_INDEX_NORETURN __attribute__((noreturn))
#define BENCHMARK_INDEX_RETURNS_NONNULL __attribute__((returns_nonnull))
#else
#define BENCHMARK_INDEX_NORETURN
#define BENCHMARK_INDEX_RETURNS_NONNULL
#endif

static void die(const char *message) BENCHMARK_INDEX_NORETURN;
static void fail(const char *message) BENCHMARK_INDEX_NORETURN;
static void failf(const char *fmt, ...) BENCHMARK_INDEX_NORETURN;
static void *xmalloc(size_t size) BENCHMARK_INDEX_RETURNS_NONNULL;
static void *xrealloc(void *ptr, size_t size) BENCHMARK_INDEX_RETURNS_NONNULL;
static void *xmalloc_array(size_t count, size_t elem_size,
                           const char *message) BENCHMARK_INDEX_RETURNS_NONNULL;
static void *
xrealloc_array(void *ptr, size_t count, size_t elem_size,
               const char *message) BENCHMARK_INDEX_RETURNS_NONNULL;
static char *xstrdup(const char *str) BENCHMARK_INDEX_RETURNS_NONNULL;
static char *path_join(const char *left,
                       const char *right) BENCHMARK_INDEX_RETURNS_NONNULL;
static char *read_file(const char *path,
                       size_t *size_out) BENCHMARK_INDEX_RETURNS_NONNULL;

static void die(const char *message) {
   perror(message);
   exit(2);
}

static void fail(const char *message) {
   fprintf(stderr, "%s\n", message);
   exit(2);
}

static void failf(const char *fmt, ...) {
   if (fmt == NULL) {
      fail("failure format is null");
   }

   va_list args;
   va_start(args, fmt);
   vfprintf(stderr, fmt, args);
   va_end(args);
   fputc('\n', stderr);
   exit(2);
}

static size_t checked_add(size_t a, size_t b, const char *message) {
   return filesystem_checked_add(a, b, message);
}

static void *xmalloc(size_t size) {
   return filesystem_malloc(size);
}

static void *xrealloc(void *ptr, size_t size) {
   return filesystem_realloc(ptr, size);
}

static void *xmalloc_array(size_t count, size_t elem_size,
                           const char *message) {
   size_t size = filesystem_checked_array_size(
       count, elem_size, "array allocation count is zero",
       "array allocation element size is zero", message);
   return xmalloc(size);
}

static void *xrealloc_array(void *ptr, size_t count, size_t elem_size,
                            const char *message) {
   size_t size = filesystem_checked_array_size(
       count, elem_size, "array reallocation count is zero",
       "array reallocation element size is zero", message);
   return xrealloc(ptr, size);
}

static char *xstrdup(const char *str) {
   if (str == NULL) {
      fail("string is null");
   }
   size_t len = strlen(str);
   size_t size = checked_add(len, 1, "string too large");
   char *copy = xmalloc(size);
   memcpy(copy, str, size);
   return copy;
}

static int has_suffix(const char *str, const char *suffix) {
   if (str == NULL) {
      fail("suffix input is null");
   }
   if (suffix == NULL) {
      fail("suffix is null");
   }
   size_t len = strlen(str);
   size_t suffix_len = strlen(suffix);
   return len >= suffix_len && strcmp(str + len - suffix_len, suffix) == 0;
}

static int is_hidden_entry(const char *name) {
   if (name == NULL) {
      fail("directory entry name is null");
   }
   return name[0] == '.';
}

static int is_indexed_svg_file(const char *name) {
   if (name == NULL) {
      fail("index file name is null");
   }
   return has_suffix(name, ".svg");
}

static char *path_join(const char *left, const char *right) {
   if (left == NULL) {
      fail("left path is null");
   }
   if (right == NULL) {
      fail("right path is null");
   }
   size_t left_len = strlen(left);
   size_t right_len = strlen(right);
   int needs_slash = left_len > 0 && left[left_len - 1] != '/';
   size_t size = checked_add(left_len, (size_t)needs_slash, "path too large");
   size = checked_add(size, right_len, "path too large");
   size = checked_add(size, 1, "path too large");
   char *path = xmalloc(size);

   memcpy(path, left, left_len);
   if (needs_slash) {
      path[left_len++] = '/';
   }
   memcpy(path + left_len, right, right_len);
   path[left_len + right_len] = '\0';
   return path;
}

static char *read_file(const char *path, size_t *size_out) {
   if (path == NULL) {
      fail("file path is null");
   }
   FILE *file = fopen_or_exit(path, "rb");

   int fd = fileno(file);
   if (fd < 0) {
      filesystem_fclose_preserving_errno(file);
      die(path);
   }

   struct stat info;
   if (fstat(fd, &info) != 0) {
      filesystem_fclose_preserving_errno(file);
      die(path);
   }
   if (!S_ISREG(info.st_mode)) {
      fclose_or_exit(file, path);
      failf("%s: not a regular file", path);
   }
   if (info.st_size < 0 || (uintmax_t)info.st_size > SIZE_MAX) {
      fclose_or_exit(file, path);
      failf("%s: file too large", path);
   }

   size_t len = (size_t)info.st_size;
   char *content = xmalloc(checked_add(len, 1, "file too large"));
   file_read_or_exit(content, len, file, path);
   fclose_or_exit(file, path);

   content[len] = '\0';
   if (size_out != NULL) {
      *size_out = len;
   }
   return content;
}

static void
benchmark_index_config_require(const benchmark_index_config_t *config) {
   if (config == NULL) {
      fail("index config is null");
   }
   if (config->template_path == NULL) {
      fail("template path is null");
   }
   if (config->template_path[0] == '\0') {
      fail("template path is empty");
   }
   if (config->root_path == NULL) {
      fail("root path is null");
   }
   if (config->root_path[0] == '\0') {
      fail("root path is empty");
   }
}

static size_t next_cap(size_t cap, size_t needed, size_t initial) {
   if (needed == 0) {
      fail("list growth need is zero");
   }
   if (initial == 0) {
      fail("list initial capacity is zero");
   }

   size_t next = cap == 0 ? initial : cap;
   while (next < needed) {
      if (next > SIZE_MAX / 2) {
         next = needed;
         break;
      }
      next *= 2;
   }
   return next;
}

static void *list_reserve_items(void *items, size_t *cap, size_t needed,
                                size_t initial, size_t elem_size,
                                const char *message) {
   if (cap == NULL) {
      fail("list capacity pointer is null");
   }
   if (message == NULL) {
      fail("list growth message is null");
   }
   if (*cap > 0 && items == NULL) {
      fail("list items are null");
   }
   if (*cap == 0 && items != NULL) {
      fail("list capacity is zero");
   }
   if (needed <= *cap) {
      return items;
   }

   size_t next = next_cap(*cap, needed, initial);
   items = items == NULL ? xmalloc_array(next, elem_size, message)
                         : xrealloc_array(items, next, elem_size, message);
   *cap = next;
   return items;
}

static void string_list_push(string_list_t *list, char *item) {
   if (list == NULL) {
      fail("string list is null");
   }
   if (item == NULL) {
      fail("string list item is null");
   }
   size_t needed = checked_add(list->len, 1, "list too large");
   list->items =
       (char **)list_reserve_items(list->items, &list->cap, needed, 8,
                                   sizeof(list->items[0]), "list too large");
   if (list->items == NULL) {
      fail("string list items are null");
   }
   list->items[list->len] = item;
   list->len = needed;
}

static void string_list_free(string_list_t *list) {
   if (list == NULL) {
      return;
   }
   for (size_t i = 0; i < list->len; i++) {
      free(list->items[i]);
   }
   free(list->items);
   list->items = NULL;
   list->len = 0;
   list->cap = 0;
}

static void entry_list_push(entry_list_t *list, const char *name, int is_dir) {
   if (list == NULL) {
      fail("entry list is null");
   }
   if (name == NULL) {
      fail("entry name is null");
   }
   size_t needed = checked_add(list->len, 1, "list too large");
   list->items =
       (entry_t *)list_reserve_items(list->items, &list->cap, needed, 16,
                                     sizeof(list->items[0]), "list too large");
   if (list->items == NULL) {
      fail("entry list items are null");
   }
   list->items[list->len].name = xstrdup(name);
   list->items[list->len].is_dir = is_dir;
   list->len = needed;
}

static void entry_list_free(entry_list_t *list) {
   if (list == NULL) {
      return;
   }
   for (size_t i = 0; i < list->len; i++) {
      free(list->items[i].name);
   }
   free(list->items);
   list->items = NULL;
   list->len = 0;
   list->cap = 0;
}

static int entry_compare(const void *left, const void *right) {
   if (left == NULL) {
      fail("left entry is null");
   }
   if (right == NULL) {
      fail("right entry is null");
   }
   const entry_t *a = left;
   const entry_t *b = right;
   return strcmp(a->name, b->name);
}

static void entry_list_sort(entry_list_t *list) {
   if (list == NULL) {
      fail("entry list is null");
   }
   if (list->len <= 1) {
      return;
   }
   if (list->items == NULL) {
      fail("entry list items are null");
   }
   qsort(list->items, list->len, sizeof(list->items[0]), entry_compare);
}

static int classify_index_entry(const char *path, int *is_dir) {
   if (path == NULL) {
      fail("entry path is null");
   }
   if (is_dir == NULL) {
      fail("entry type output is null");
   }

   struct stat info;
   if (lstat(path, &info) != 0) {
      die(path);
   }
   if (S_ISDIR(info.st_mode)) {
      *is_dir = 1;
      return 1;
   }
   if (S_ISREG(info.st_mode)) {
      *is_dir = 0;
      return 1;
   }
   if (S_ISLNK(info.st_mode)) {
      if (stat(path, &info) != 0) {
         return 0;
      }
      if (S_ISREG(info.st_mode)) {
         *is_dir = 0;
         return 1;
      }
   }
   return 0;
}

static int starts_with_at(const char *pos, const char *end, const char *token) {
   if (pos == NULL) {
      fail("marker position is null");
   }
   if (end == NULL) {
      fail("marker end is null");
   }
   if (token == NULL) {
      fail("marker token is null");
   }
   if (pos > end) {
      fail("marker range is invalid");
   }
   size_t len = strlen(token);
   return (size_t)(end - pos) >= len && strncmp(pos, token, len) == 0;
}

static void write_html_escaped(FILE *out, const char *output_path,
                               const char *text) {
   if (out == NULL) {
      fail("output file is null");
   }
   if (output_path == NULL) {
      fail("output path is null");
   }
   if (text == NULL) {
      fail("html text is null");
   }

   for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++) {
      switch (*p) {
      case '&':
         file_puts_or_exit("&amp;", out, output_path);
         break;
      case '<':
         file_puts_or_exit("&lt;", out, output_path);
         break;
      case '>':
         file_puts_or_exit("&gt;", out, output_path);
         break;
      case '"':
         file_puts_or_exit("&quot;", out, output_path);
         break;
      case '\'':
         file_puts_or_exit("&#39;", out, output_path);
         break;
      default:
         file_putc_or_exit((int)*p, out, output_path);
         break;
      }
   }
}

static void validate_template_markers(const benchmark_index_config_t *config,
                                      const char *tmpl, size_t tmpl_size) {
   benchmark_index_config_require(config);
   if (tmpl == NULL) {
      fail("template is null");
   }
   if (tmpl_size > 0 && memchr(tmpl, '\0', tmpl_size) != NULL) {
      failf("%s: template contains NUL byte", config->template_path);
   }

   const char *range = strstr(tmpl, range_marker);
   const char *end = strstr(tmpl, end_marker);
   const char *inline_file = strstr(tmpl, inline_marker);

   if (range == NULL) {
      if (end != NULL) {
         failf("%s: unexpected %s", config->template_path, end_marker);
      }
      if (inline_file != NULL) {
         failf("%s: unexpected %s outside %s", config->template_path,
               inline_marker, range_marker);
      }
      return;
   }

   if (inline_file != NULL && inline_file < range) {
      failf("%s: unexpected %s before %s", config->template_path, inline_marker,
            range_marker);
   }

   if (end != NULL && end < range) {
      failf("%s: unexpected %s before %s", config->template_path, end_marker,
            range_marker);
   }

   const char *block_start = range + strlen(range_marker);
   const char *block_end = strstr(block_start, end_marker);
   if (block_end == NULL) {
      failf("%s: missing %s", config->template_path, end_marker);
   }

   const char *nested_range = strstr(block_start, range_marker);
   if (nested_range != NULL && nested_range < block_end) {
      failf("%s: unexpected nested %s", config->template_path, range_marker);
   }

   const char *block_inline = strstr(block_start, inline_marker);
   if (block_inline == NULL || block_inline >= block_end) {
      failf("%s: missing %s inside %s", config->template_path, inline_marker,
            range_marker);
   }
   const char *extra_inline =
       strstr(block_inline + strlen(inline_marker), inline_marker);
   if (extra_inline != NULL && extra_inline < block_end) {
      failf("%s: unexpected extra %s inside %s", config->template_path,
            inline_marker, range_marker);
   }

   const char *tail = block_end + strlen(end_marker);
   if (strstr(tail, range_marker) != NULL) {
      failf("%s: unexpected extra %s", config->template_path, range_marker);
   }
   if (strstr(tail, end_marker) != NULL) {
      failf("%s: unexpected extra %s", config->template_path, end_marker);
   }
   if (strstr(tail, inline_marker) != NULL) {
      failf("%s: unexpected %s outside %s", config->template_path,
            inline_marker, range_marker);
   }
}

static inline_file_t inline_file_read(const char *inline_path) {
   if (inline_path == NULL) {
      fail("inline path is null");
   }

   size_t inline_size = 0;
   char *inline_content = read_file(inline_path, &inline_size);
   if (inline_size > 0 && memchr(inline_content, '\0', inline_size) != NULL) {
      free(inline_content);
      failf("%s: inline file contains NUL byte", inline_path);
   }
   return (inline_file_t){
       .path = inline_path,
       .content = inline_content,
       .size = inline_size,
   };
}

static void inline_file_dispose(inline_file_t *file) {
   if (file == NULL) {
      return;
   }
   free(file->content);
   *file = (inline_file_t){0};
}

static inline_file_list_t inline_file_list_read(const string_list_t *files) {
   if (files == NULL) {
      fail("file list is null");
   }
   if (files->len > 0 && files->items == NULL) {
      fail("file list items are null");
   }

   inline_file_list_t list = {.len = files->len};
   if (list.len == 0) {
      return list;
   }

   list.items = (inline_file_t *)xmalloc_array(list.len, sizeof(list.items[0]),
                                               "inline file list too large");
   for (size_t i = 0; i < list.len; i++) {
      list.items[i] = inline_file_read(files->items[i]);
   }
   return list;
}

static void inline_file_list_free(inline_file_list_t *list) {
   if (list == NULL) {
      return;
   }
   for (size_t i = 0; i < list->len; i++) {
      inline_file_dispose(&list->items[i]);
   }
   free(list->items);
   list->items = NULL;
   list->len = 0;
}

static void write_processed(FILE *out, const char *output_path,
                            const char *start, size_t len,
                            const char *directory,
                            const inline_file_t *inline_file) {
   if (out == NULL) {
      fail("output file is null");
   }
   if (output_path == NULL) {
      fail("output path is null");
   }
   if (start == NULL) {
      fail("template start is null");
   }
   if (directory == NULL) {
      fail("directory is null");
   }
   const char *pos = start;
   const char *end = start + len;

   while (pos < end) {
      if (starts_with_at(pos, end, directory_marker)) {
         write_html_escaped(out, output_path, directory);
         pos += strlen(directory_marker);
      } else if (inline_file && starts_with_at(pos, end, inline_marker)) {
         if (inline_file->content == NULL) {
            fail("inline file content is null");
         }
         file_write_or_exit(inline_file->content, inline_file->size, out,
                            output_path);
         pos += strlen(inline_marker);
      } else {
         file_putc_or_exit((int)(unsigned char)*pos, out, output_path);
         pos++;
      }
   }
}

static void write_index(const benchmark_index_config_t *config,
                        const char *directory, const string_list_t *files,
                        const char *tmpl) {
   benchmark_index_config_require(config);
   if (directory == NULL) {
      fail("directory is null");
   }
   if (files == NULL) {
      fail("file list is null");
   }
   if (tmpl == NULL) {
      fail("template is null");
   }

   const char *range = strstr(tmpl, range_marker);
   const char *block_start = NULL;
   const char *block_end = NULL;

   if (range) {
      block_start = range + strlen(range_marker);
      block_end = strstr(block_start, end_marker);
      if (!block_end) {
         failf("%s: missing %s", config->template_path, end_marker);
      }
   }

   inline_file_list_t inline_files =
       range ? inline_file_list_read(files) : (inline_file_list_t){0};

   char *index_path = path_join(directory, "index.html");
   char *tmp_path = NULL;
   FILE *out = fopen_temporary_or_exit(&tmp_path, index_path, "wb");
   if (tmp_path == NULL) {
      fail("temporary path was not initialized");
   }

   if (!range) {
      write_processed(out, tmp_path, tmpl, strlen(tmpl), directory, NULL);
   } else {
      size_t prefix_len = (size_t)(range - tmpl);
      size_t block_len = (size_t)(block_end - block_start);
      const char *tail = block_end + strlen(end_marker);

      write_processed(out, tmp_path, tmpl, prefix_len, directory, NULL);
      for (size_t i = 0; i < files->len; i++) {
         write_processed(out, tmp_path, block_start, block_len, directory,
                         &inline_files.items[i]);
      }
      write_processed(out, tmp_path, tail, strlen(tail), directory, NULL);
   }

   file_publish_or_exit(out, tmp_path, index_path);

   inline_file_list_free(&inline_files);
   free(tmp_path);
   free(index_path);
}

static void remove_stale_nested_index(const benchmark_index_config_t *config,
                                      const char *directory) {
   benchmark_index_config_require(config);
   if (directory == NULL) {
      fail("directory is null");
   }
   if (strcmp(directory, config->root_path) == 0) {
      return;
   }

   char *index_path = path_join(directory, "index.html");
   path_unlink_if_exists_or_exit(index_path);
   free(index_path);
}

static entry_list_t index_directory_entries(const char *directory) {
   if (directory == NULL) {
      fail("directory is null");
   }
   DIR *dir = opendir(directory);
   if (dir == NULL) {
      die(directory);
   }

   entry_list_t entries = {0};
   struct dirent *entry;

   for (;;) {
      errno = 0;
      entry = readdir(dir);
      if (entry == NULL) {
         break;
      }
      if (is_hidden_entry(entry->d_name)) {
         continue;
      }

      char *full_path = path_join(directory, entry->d_name);
      int is_dir = 0;
      if (classify_index_entry(full_path, &is_dir)) {
         entry_list_push(&entries, entry->d_name, is_dir);
      }
      free(full_path);
   }
   if (errno != 0) {
      die(directory);
   }

   if (closedir(dir) != 0) {
      die(directory);
   }
   entry_list_sort(&entries);
   return entries;
}

static void validate_inline_index_file(const char *path) {
   inline_file_t file = inline_file_read(path);
   inline_file_dispose(&file);
}

static void preflight_index_directory(const benchmark_index_config_t *config,
                                      const char *directory,
                                      int validate_inline_files) {
   benchmark_index_config_require(config);
   if (directory == NULL) {
      fail("directory is null");
   }
   entry_list_t entries = index_directory_entries(directory);

   for (size_t i = 0; i < entries.len; i++) {
      char *full_path = path_join(directory, entries.items[i].name);
      if (entries.items[i].is_dir) {
         preflight_index_directory(config, full_path, validate_inline_files);
      } else if (validate_inline_files &&
                 is_indexed_svg_file(entries.items[i].name)) {
         validate_inline_index_file(full_path);
      }
      free(full_path);
   }

   entry_list_free(&entries);
}

static void index_directory(const benchmark_index_config_t *config,
                            const char *directory, const char *tmpl) {
   benchmark_index_config_require(config);
   if (directory == NULL) {
      fail("directory is null");
   }
   if (tmpl == NULL) {
      fail("template is null");
   }
   entry_list_t entries = index_directory_entries(directory);

   string_list_t files = {0};
   for (size_t i = 0; i < entries.len; i++) {
      char *full_path = path_join(directory, entries.items[i].name);
      if (entries.items[i].is_dir) {
         index_directory(config, full_path, tmpl);
      } else if (is_indexed_svg_file(entries.items[i].name)) {
         string_list_push(&files, full_path);
         full_path = NULL;
      }
      free(full_path);
   }

   if (files.len > 0 || strcmp(directory, config->root_path) == 0) {
      write_index(config, directory, &files, tmpl);
   } else {
      remove_stale_nested_index(config, directory);
   }

   string_list_free(&files);
   entry_list_free(&entries);
}

void benchmark_index_run(const char *selected_template_path,
                         const char *selected_root_path) {
   benchmark_index_config_t config = {
       .template_path = selected_template_path,
       .root_path = selected_root_path,
   };
   benchmark_index_config_require(&config);

   size_t tmpl_size = 0;
   char *tmpl = read_file(config.template_path, &tmpl_size);
   validate_template_markers(&config, tmpl, tmpl_size);
   preflight_index_directory(&config, config.root_path,
                             strstr(tmpl, range_marker) != NULL);
   index_directory(&config, config.root_path, tmpl);
   free(tmpl);
}

#ifndef BENCHMARK_INDEX_NO_MAIN
int main(int argc, char **argv) {
   if (argc != 1 && argc != 3) {
      fprintf(stderr, "usage: %s [TEMPLATE_PATH ROOT_PATH]\n", argv[0]);
      return 2;
   }

   const char *template_path = default_template_path;
   const char *root_path = default_root_path;
   if (argc == 3) {
      template_path = argv[1];
      root_path = argv[2];
   }

   benchmark_index_run(template_path, root_path);
   return 0;
}
#endif
