#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

static const char *template_path = "docs/benchmarks/index.template.html";
static const char *root_path = "docs/benchmarks";
static const char *range_marker = "{{ range $_, $file := .files }}";
static const char *end_marker = "{{ end }}";
static const char *directory_marker = "{{ .directory }}";
static const char *inline_marker = "{{ inline $file }}";

static void die(const char *message) {
   perror(message);
   exit(1);
}

static void *xmalloc(size_t size) {
   void *ptr = malloc(size);
   if (!ptr) die("malloc");
   return ptr;
}

static void *xrealloc(void *ptr, size_t size) {
   void *next = realloc(ptr, size);
   if (!next) die("realloc");
   return next;
}

static char *xstrdup(const char *str) {
   size_t len = strlen(str);
   char *copy = xmalloc(len + 1);
   memcpy(copy, str, len + 1);
   return copy;
}

static int has_suffix(const char *str, const char *suffix) {
   size_t len = strlen(str);
   size_t suffix_len = strlen(suffix);
   return len >= suffix_len && strcmp(str + len - suffix_len, suffix) == 0;
}

static char *path_join(const char *left, const char *right) {
   size_t left_len = strlen(left);
   size_t right_len = strlen(right);
   int needs_slash = left_len > 0 && left[left_len - 1] != '/';
   char *path = xmalloc(left_len + (size_t)needs_slash + right_len + 1);

   memcpy(path, left, left_len);
   if (needs_slash) path[left_len++] = '/';
   memcpy(path + left_len, right, right_len);
   path[left_len + right_len] = '\0';
   return path;
}

static char *read_file(const char *path, size_t *size_out) {
   FILE *file = fopen(path, "rb");
   if (!file) die(path);

   if (fseek(file, 0, SEEK_END) != 0) die(path);
   long length = ftell(file);
   if (length < 0) die(path);
   if (fseek(file, 0, SEEK_SET) != 0) die(path);

   char *content = xmalloc((size_t)length + 1);
   size_t read = fread(content, 1, (size_t)length, file);
   if (read != (size_t)length) die(path);
   if (fclose(file) != 0) die(path);

   content[length] = '\0';
   if (size_out) *size_out = (size_t)length;
   return content;
}

static void string_list_push(string_list_t *list, char *item) {
   if (list->len == list->cap) {
      list->cap = list->cap ? list->cap * 2 : 8;
      list->items = xrealloc(list->items, list->cap * sizeof(list->items[0]));
   }
   list->items[list->len++] = item;
}

static void string_list_free(string_list_t *list) {
   for (size_t i = 0; i < list->len; i++) free(list->items[i]);
   free(list->items);
   list->items = NULL;
   list->len = 0;
   list->cap = 0;
}

static void entry_list_push(entry_list_t *list, const char *name, int is_dir) {
   if (list->len == list->cap) {
      list->cap = list->cap ? list->cap * 2 : 16;
      list->items = xrealloc(list->items, list->cap * sizeof(list->items[0]));
   }
   list->items[list->len].name = xstrdup(name);
   list->items[list->len].is_dir = is_dir;
   list->len++;
}

static void entry_list_free(entry_list_t *list) {
   for (size_t i = 0; i < list->len; i++) free(list->items[i].name);
   free(list->items);
   list->items = NULL;
   list->len = 0;
   list->cap = 0;
}

static int entry_compare(const void *left, const void *right) {
   const entry_t *a = left;
   const entry_t *b = right;
   return strcmp(a->name, b->name);
}

static int starts_with_at(const char *pos, const char *end, const char *token) {
   size_t len = strlen(token);
   return (size_t)(end - pos) >= len && strncmp(pos, token, len) == 0;
}

static void write_processed(
   FILE *out,
   const char *start,
   size_t len,
   const char *directory,
   const char *inline_path
) {
   const char *pos = start;
   const char *end = start + len;

   while (pos < end) {
      if (starts_with_at(pos, end, directory_marker)) {
         fputs(directory, out);
         pos += strlen(directory_marker);
      } else if (inline_path && starts_with_at(pos, end, inline_marker)) {
         size_t inline_size = 0;
         char *inline_content = read_file(inline_path, &inline_size);
         fwrite(inline_content, 1, inline_size, out);
         free(inline_content);
         pos += strlen(inline_marker);
      } else {
         fputc(*pos, out);
         pos++;
      }
   }
}

static void write_index(const char *directory, const string_list_t *files, const char *tmpl) {
   char *index_path = path_join(directory, "index.html");
   FILE *out = fopen(index_path, "wb");
   if (!out) die(index_path);

   const char *range = strstr(tmpl, range_marker);
   if (!range) {
      write_processed(out, tmpl, strlen(tmpl), directory, NULL);
   } else {
      const char *block_start = range + strlen(range_marker);
      const char *block_end = strstr(block_start, end_marker);
      if (!block_end) {
         fprintf(stderr, "%s: missing %s\n", template_path, end_marker);
         exit(1);
      }

      write_processed(out, tmpl, (size_t)(range - tmpl), directory, NULL);
      for (size_t i = 0; i < files->len; i++) {
         write_processed(out, block_start, (size_t)(block_end - block_start), directory, files->items[i]);
      }
      write_processed(out,
         block_end + strlen(end_marker),
         strlen(block_end + strlen(end_marker)),
         directory,
         NULL);
   }

   if (fclose(out) != 0) die(index_path);
   free(index_path);
}

static void index_directory(const char *directory, const char *tmpl) {
   DIR *dir = opendir(directory);
   if (!dir) die(directory);

   entry_list_t entries = {0};
   struct dirent *entry;

   while ((entry = readdir(dir)) != NULL) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

      char *full_path = path_join(directory, entry->d_name);
      struct stat info;
      if (stat(full_path, &info) != 0) die(full_path);

      if (S_ISDIR(info.st_mode) || S_ISREG(info.st_mode)) {
         entry_list_push(&entries, entry->d_name, S_ISDIR(info.st_mode));
      }
      free(full_path);
   }

   if (closedir(dir) != 0) die(directory);
   qsort(entries.items, entries.len, sizeof(entries.items[0]), entry_compare);

   string_list_t files = {0};
   for (size_t i = 0; i < entries.len; i++) {
      char *full_path = path_join(directory, entries.items[i].name);
      if (entries.items[i].is_dir) {
         index_directory(full_path, tmpl);
      } else if (has_suffix(entries.items[i].name, ".svg")) {
         string_list_push(&files, full_path);
         full_path = NULL;
      }
      free(full_path);
   }

   if (files.len > 0) write_index(directory, &files, tmpl);

   string_list_free(&files);
   entry_list_free(&entries);
}

int main(void) {
   char *tmpl = read_file(template_path, NULL);
   index_directory(root_path, tmpl);
   free(tmpl);
   return 0;
}
