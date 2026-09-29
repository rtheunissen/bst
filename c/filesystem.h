#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "bst.h"

enum {
   FOPEN_TEMPORARY_MAX_ATTEMPTS = 1024,
};

#if defined(__clang__) || defined(__GNUC__)
#define FILESYSTEM_NORETURN __attribute__((noreturn))
#else
#define FILESYSTEM_NORETURN
#endif

static inline void filesystem_fail(const char *message) FILESYSTEM_NORETURN;
static inline void filesystem_fail(const char *message) {
   fprintf(stderr, "%s\n", message ? message : "filesystem error");
   exit(2);
}

static inline void filesystem_require(bool condition, const char *message) {
   if (!condition) {
      filesystem_fail(message);
   }
}

static inline size_t filesystem_checked_add(size_t a, size_t b,
                                            const char *message) {
   filesystem_require(a <= SIZE_MAX - b, message);
   return a + b;
}

static inline size_t filesystem_checked_array_size(
    size_t count, size_t size, const char *zero_count_message,
    const char *zero_size_message, const char *overflow_message) {
   filesystem_require(count > 0, zero_count_message);
   filesystem_require(size > 0, zero_size_message);
   filesystem_require(count <= SIZE_MAX / size, overflow_message);
   return count * size;
}

static inline size_t filesystem_checked_mul(size_t a, size_t b,
                                            const char *message) {
   return filesystem_checked_array_size(a, b, "allocation count is zero",
                                        "allocation element size is zero",
                                        message);
}

static inline void *filesystem_malloc(size_t size) {
   filesystem_require(size > 0, "allocation size is zero");
   void *ptr = malloc(size);
   if (ptr == NULL) {
      perror("malloc");
      exit(2);
   }
   return ptr;
}

static inline void *filesystem_realloc(void *ptr, size_t size) {
   filesystem_require(ptr != NULL, "reallocation pointer is null");
   filesystem_require(size > 0, "reallocation size is zero");
   void *next = realloc(ptr, size);
   if (next == NULL) {
      perror("realloc");
      exit(2);
   }
   return next;
}

static inline void *filesystem_malloc_array(size_t count, size_t size,
                                            const char *message) {
   return filesystem_malloc(filesystem_checked_array_size(
       count, size, "allocation count is zero",
       "allocation element size is zero", message));
}

static inline void *filesystem_realloc_array(void *ptr, size_t count,
                                             size_t size, const char *message) {
   return filesystem_realloc(ptr,
                             filesystem_checked_array_size(
                                 count, size, "allocation count is zero",
                                 "allocation element size is zero", message));
}

static inline bool path_is_directory(const char *path) {
   filesystem_require(path != NULL, "directory path is null");
   filesystem_require(path[0] != '\0', "directory path is empty");
   struct stat st;
   if (stat(path, &st) != 0) {
      return false;
   }
   return S_ISDIR(st.st_mode);
}

static inline void mkdir_or_existing_dir(const char *path) {
   filesystem_require(path != NULL, "directory path is null");
   filesystem_require(path[0] != '\0', "directory path is empty");
   if (mkdir(path, 0777) == 0) {
      return;
   }

   int mkdir_errno = errno;
   if (mkdir_errno == EEXIST && path_is_directory(path)) {
      return;
   }

   errno = mkdir_errno;
   perror(path);
   exit(2);
}

static inline void path_format(char *buf, size_t size, const char *fmt, ...) {
   filesystem_require(buf != NULL, "path buffer is null");
   filesystem_require(size > 0, "path buffer is empty");
   filesystem_require(fmt != NULL, "path format is null");

   va_list args;
   va_start(args, fmt);
   int n = vsnprintf(buf, size, fmt, args);
   va_end(args);

   if (n < 0 || (size_t)n >= size) {
      fprintf(stderr, "path too long\n");
      exit(2);
   }
}

static inline char *path_format_alloc(const char *fmt, ...) {
   filesystem_require(fmt != NULL, "path format is null");

   va_list args;
   va_start(args, fmt);
   va_list copy;
   va_copy(copy, args);
   int n = vsnprintf(NULL, 0, fmt, args);
   va_end(args);

   if (n < 0) {
      fprintf(stderr, "path format failed\n");
      exit(2);
   }

   size_t size = filesystem_checked_add((size_t)n, 1, "path too long");
   char *buf =
       (char *)filesystem_malloc_array(size, sizeof(buf[0]), "path too long");

   int written = vsnprintf(buf, size, fmt, copy);
   va_end(copy);
   if (written < 0 || written != n) {
      fprintf(stderr, "path format failed\n");
      exit(2);
   }
   return buf;
}

static inline bool path_has_file_name(const char *path) {
   filesystem_require(path != NULL, "file path is null");
   filesystem_require(path[0] != '\0', "file path is empty");

   size_t len = strlen(path);
   if (path[len - 1] == '/') {
      return false;
   }

   const char *name = strrchr(path, '/');
   name = name == NULL ? path : name + 1;
   return strcmp(name, ".") != 0 && strcmp(name, "..") != 0;
}

static inline bool path_is_component(const char *name) {
   return bst_name_is_path_component(name);
}

static inline void path_require_component(const char *name,
                                          const char *message) {
   filesystem_require(message != NULL, "path component message is null");
   filesystem_require(path_is_component(name), message);
}

static inline char *path_parent_dir_alloc(const char *path) {
   filesystem_require(path != NULL, "file path is null");
   filesystem_require(path[0] != '\0', "file path is empty");
   filesystem_require(path_has_file_name(path), "file path has no file name");

   const char *slash = strrchr(path, '/');
   if (slash == NULL) {
      return path_format_alloc(".");
   }
   if (slash == path) {
      return path_format_alloc("/");
   }

   size_t len = (size_t)(slash - path);
   char *dir = (char *)filesystem_malloc_array(
       filesystem_checked_add(len, 1, "path too long"), sizeof(dir[0]),
       "path too long");
   memcpy(dir, path, len);
   dir[len] = '\0';
   return dir;
}

static inline void path_require_same_parent_dir(const char *left_path,
                                                const char *right_path,
                                                const char *message) {
   filesystem_require(message != NULL, "parent directory message is null");
   char *left_parent = path_parent_dir_alloc(left_path);
   char *right_parent = path_parent_dir_alloc(right_path);
   bool same_parent = strcmp(left_parent, right_parent) == 0;
   free(left_parent);
   free(right_parent);
   filesystem_require(same_parent, message);
}

static inline void path_require_parent_dir(const char *file_path,
                                           const char *dir_path,
                                           const char *message) {
   filesystem_require(message != NULL, "parent directory message is null");
   char *parent = path_parent_dir_alloc(file_path);
   bool is_parent = dir_path != NULL && strcmp(dir_path, parent) == 0;
   free(parent);
   filesystem_require(is_parent, message);
}

static inline void path_rename_or_exit(const char *old_path,
                                       const char *new_path) {
   filesystem_require(old_path != NULL, "old path is null");
   filesystem_require(old_path[0] != '\0', "old path is empty");
   filesystem_require(new_path != NULL, "new path is null");
   filesystem_require(new_path[0] != '\0', "new path is empty");

   if (rename(old_path, new_path) != 0) {
      int rename_errno = errno;
      fprintf(stderr, "rename %s -> %s: %s\n", old_path, new_path,
              strerror(rename_errno));
      exit(2);
   }
}

static inline void path_unlink_if_exists_or_exit(const char *path) {
   filesystem_require(path != NULL, "unlink path is null");
   filesystem_require(path[0] != '\0', "unlink path is empty");

   if (unlink(path) == 0) {
      return;
   }
   if (errno == ENOENT) {
      return;
   }

   perror(path);
   exit(2);
}

static inline int open_flags_with_cloexec(int flags) {
#ifdef O_CLOEXEC
   return flags | O_CLOEXEC;
#else
   return flags;
#endif
}

static inline void filesystem_close_preserving_errno(int fd) {
   if (fd < 0) {
      return;
   }

   int saved_errno = errno;
   (void)close(fd);
   errno = saved_errno;
}

static inline void filesystem_fclose_preserving_errno(FILE *file) {
   if (file == NULL) {
      return;
   }

   int saved_errno = errno;
   (void)fclose(file);
   errno = saved_errno;
}

static inline void filesystem_unlink_preserving_errno(const char *path) {
   if (path == NULL) {
      return;
   }

   int saved_errno = errno;
   (void)unlink(path);
   errno = saved_errno;
}

static inline bool fd_set_cloexec(int fd, const char *path) {
   filesystem_require(fd >= 0, "file descriptor is invalid");
   filesystem_require(path != NULL, "file path is null");
   filesystem_require(path[0] != '\0', "file path is empty");

   int flags = fcntl(fd, F_GETFD);
   if (flags < 0) {
      return false;
   }
   if ((flags & FD_CLOEXEC) == 0 &&
       fcntl(fd, F_SETFD, flags | FD_CLOEXEC) != 0) {
      return false;
   }
   return true;
}

static inline void fd_set_cloexec_or_exit(int fd, const char *path) {
   if (!fd_set_cloexec(fd, path)) {
      perror(path);
      exit(2);
   }
}

static inline void path_sync_parent_dir_or_exit(const char *path) {
   char *dir = path_parent_dir_alloc(path);
   int fd = open(dir, open_flags_with_cloexec(O_RDONLY));
   if (fd < 0) {
      perror(dir);
      free(dir);
      exit(2);
   }
   if (!fd_set_cloexec(fd, dir)) {
      filesystem_close_preserving_errno(fd);
      perror(dir);
      free(dir);
      exit(2);
   }
   if (fsync(fd) != 0) {
      perror(dir);
      close(fd);
      free(dir);
      exit(2);
   }
   if (close(fd) != 0) {
      perror(dir);
      free(dir);
      exit(2);
   }
   free(dir);
}

static inline void mkdir_p(const char *path) {
   filesystem_require(path != NULL, "directory path is null");
   filesystem_require(path[0] != '\0', "directory path is empty");

   char *buf = path_format_alloc("%s", path);
   size_t len = strlen(buf);
   while (len > 1 && buf[len - 1] == '/') {
      buf[--len] = '\0';
   }

   for (char *p = buf + 1; *p != '\0'; p++) {
      if (*p != '/') {
         continue;
      }
      *p = '\0';
      mkdir_or_existing_dir(buf);
      *p = '/';
   }

   mkdir_or_existing_dir(buf);
   free(buf);
}

static inline FILE *fopen_or_exit(const char *path, const char *mode) {
   filesystem_require(path != NULL, "file path is null");
   filesystem_require(path[0] != '\0', "file path is empty");
   filesystem_require(mode != NULL, "file mode is null");
   filesystem_require(mode[0] != '\0', "file mode is empty");

   FILE *file = fopen(path, mode);
   if (file == NULL) {
      perror(path);
      exit(2);
   }
   int fd = fileno(file);
   if (fd < 0) {
      filesystem_fclose_preserving_errno(file);
      perror(path);
      exit(2);
   }
   if (!fd_set_cloexec(fd, path)) {
      filesystem_fclose_preserving_errno(file);
      perror(path);
      exit(2);
   }
   return file;
}

static inline bool fopen_temporary_mode_is_supported(const char *mode) {
   if (mode == NULL) {
      return false;
   }
   return strcmp(mode, "w") == 0 || strcmp(mode, "wb") == 0;
}

static inline char *path_temporary_attempt_alloc(const char *path, pid_t pid,
                                                 unsigned int attempt) {
   filesystem_require(path != NULL, "file path is null");
   filesystem_require(path[0] != '\0', "file path is empty");
   filesystem_require(path_has_file_name(path), "file path has no file name");
   filesystem_require(pid > 0, "temporary file pid is invalid");
   return path_format_alloc("%s.tmp.%ld.%u", path, (long)pid, attempt);
}

static inline FILE *fopen_temporary_or_exit(char **tmp_path_out,
                                            const char *path,
                                            const char *mode) {
   filesystem_require(tmp_path_out != NULL, "temporary path output is null");
   filesystem_require(*tmp_path_out == NULL,
                      "temporary path output is not empty");

   filesystem_require(path != NULL, "file path is null");
   filesystem_require(path[0] != '\0', "file path is empty");
   filesystem_require(mode != NULL, "temporary file mode is null");
   filesystem_require(mode[0] != '\0', "temporary file mode is empty");
   filesystem_require(fopen_temporary_mode_is_supported(mode),
                      "temporary file mode must be w or wb");

   for (unsigned int attempt = 0; attempt < FOPEN_TEMPORARY_MAX_ATTEMPTS;
        attempt++) {
      char *tmp_path = path_temporary_attempt_alloc(path, getpid(), attempt);
      int fd = open(tmp_path,
                    open_flags_with_cloexec(O_WRONLY | O_CREAT | O_EXCL), 0666);
      if (fd < 0) {
         if (errno == EEXIST) {
            free(tmp_path);
            continue;
         }
         perror(tmp_path);
         free(tmp_path);
         exit(2);
      }
      if (!fd_set_cloexec(fd, tmp_path)) {
         filesystem_close_preserving_errno(fd);
         filesystem_unlink_preserving_errno(tmp_path);
         perror(tmp_path);
         free(tmp_path);
         exit(2);
      }

      FILE *file = fdopen(fd, mode);
      if (file == NULL) {
         filesystem_close_preserving_errno(fd);
         filesystem_unlink_preserving_errno(tmp_path);
         perror(tmp_path);
         free(tmp_path);
         exit(2);
      }

      *tmp_path_out = tmp_path;
      return file;
   }

   fprintf(stderr, "could not create temporary file for %s\n", path);
   exit(2);
}

static inline void file_printf_or_exit(FILE *file, const char *path,
                                       const char *fmt, ...) {
   filesystem_require(file != NULL, "file handle is null");
   filesystem_require(path != NULL, "file path is null");
   filesystem_require(path[0] != '\0', "file path is empty");
   filesystem_require(fmt != NULL, "file format is null");

   va_list args;
   va_start(args, fmt);
   int n = vfprintf(file, fmt, args);
   va_end(args);

   if (n < 0) {
      perror(path);
      exit(2);
   }
}

static inline void file_puts_or_exit(const char *str, FILE *file,
                                     const char *path) {
   filesystem_require(str != NULL, "file string is null");
   filesystem_require(file != NULL, "file handle is null");
   filesystem_require(path != NULL, "file path is null");
   filesystem_require(path[0] != '\0', "file path is empty");

   if (fputs(str, file) == EOF) {
      perror(path);
      exit(2);
   }
}

static inline void file_putc_or_exit(int ch, FILE *file, const char *path) {
   filesystem_require(ch >= 0 && ch <= UCHAR_MAX, "file byte is out of range");
   filesystem_require(file != NULL, "file handle is null");
   filesystem_require(path != NULL, "file path is null");
   filesystem_require(path[0] != '\0', "file path is empty");

   if (fputc((unsigned char)ch, file) == EOF) {
      perror(path);
      exit(2);
   }
}

static inline void file_write_or_exit(const void *ptr, size_t size, FILE *file,
                                      const char *path) {
   filesystem_require(size == 0 || ptr != NULL, "file write buffer is null");
   filesystem_require(file != NULL, "file handle is null");
   filesystem_require(path != NULL, "file path is null");
   filesystem_require(path[0] != '\0', "file path is empty");

   if (size == 0) {
      return;
   }

   if (fwrite(ptr, 1, size, file) == size) {
      return;
   }

   if (ferror(file)) {
      perror(path);
   } else {
      fprintf(stderr, "short write: %s\n", path);
   }
   exit(2);
}

static inline void file_read_or_exit(void *ptr, size_t size, FILE *file,
                                     const char *path) {
   filesystem_require(size == 0 || ptr != NULL, "file read buffer is null");
   filesystem_require(file != NULL, "file handle is null");
   filesystem_require(path != NULL, "file path is null");
   filesystem_require(path[0] != '\0', "file path is empty");

   if (size == 0) {
      return;
   }

   if (fread(ptr, 1, size, file) == size) {
      return;
   }

   if (ferror(file)) {
      perror(path);
   } else {
      fprintf(stderr, "short read: %s\n", path);
   }
   exit(2);
}

static inline void file_sync_or_exit(FILE *file, const char *path) {
   filesystem_require(file != NULL, "file handle is null");
   filesystem_require(path != NULL, "file path is null");
   filesystem_require(path[0] != '\0', "file path is empty");

   if (fflush(file) != 0) {
      perror(path);
      exit(2);
   }

   int fd = fileno(file);
   if (fd < 0) {
      perror(path);
      exit(2);
   }
   if (fsync(fd) != 0) {
      perror(path);
      exit(2);
   }
}

static inline void fclose_or_exit(FILE *file, const char *path) {
   filesystem_require(file != NULL, "file handle is null");
   filesystem_require(path != NULL, "file path is null");
   filesystem_require(path[0] != '\0', "file path is empty");

   if (fclose(file) != 0) {
      perror(path);
      exit(2);
   }
}

static inline void file_handle_require_path(FILE *file, const char *path) {
   filesystem_require(file != NULL, "file handle is null");
   filesystem_require(path != NULL, "file path is null");
   filesystem_require(path[0] != '\0', "file path is empty");

   int fd = fileno(file);
   if (fd < 0) {
      perror(path);
      exit(2);
   }

   struct stat file_stat;
   if (fstat(fd, &file_stat) != 0) {
      perror(path);
      exit(2);
   }

   struct stat path_stat;
   if (stat(path, &path_stat) != 0) {
      perror(path);
      exit(2);
   }

   filesystem_require(file_stat.st_dev == path_stat.st_dev &&
                          file_stat.st_ino == path_stat.st_ino,
                      "file handle does not match path");
}

typedef struct {
   char *path;
   char *tmp_path;
   bool finished;
} staged_file_t;

static inline void staged_file_require(const staged_file_t *file) {
   filesystem_require(file != NULL, "staged file is null");
   filesystem_require(file->path != NULL, "staged file path is null");
   filesystem_require(file->path[0] != '\0', "staged file path is empty");
   filesystem_require(path_has_file_name(file->path),
                      "staged file path has no file name");
   filesystem_require(!file->finished || file->tmp_path != NULL,
                      "finished staged file has no temporary path");
}

static inline void staged_file_require_publishable(const staged_file_t *file) {
   staged_file_require(file);
   filesystem_require(file->tmp_path != NULL,
                      "staged temporary file path is null");
   filesystem_require(file->tmp_path[0] != '\0',
                      "staged temporary file path is empty");
   filesystem_require(path_has_file_name(file->tmp_path),
                      "staged temporary file path has no file name");
   filesystem_require(strcmp(file->tmp_path, file->path) != 0,
                      "staged temporary file path matches target");
   path_require_same_parent_dir(
       file->tmp_path, file->path,
       "staged temporary file path is not in target directory");
   filesystem_require(file->finished, "staged file is not finished");
}

static inline void staged_file_dispose(staged_file_t *file) {
   if (file == NULL) {
      return;
   }
   if (file->tmp_path != NULL) {
      filesystem_require(file->path == NULL ||
                             strcmp(file->tmp_path, file->path) != 0,
                         "staged temporary file path matches target");
      path_unlink_if_exists_or_exit(file->tmp_path);
   }
   free(file->path);
   free(file->tmp_path);
   file->path = NULL;
   file->tmp_path = NULL;
   file->finished = false;
}

static inline FILE *staged_file_open_or_exit(staged_file_t *file,
                                             const char *mode) {
   staged_file_require(file);
   filesystem_require(file->tmp_path == NULL,
                      "staged temporary file path already exists");
   filesystem_require(!file->finished, "staged file is already finished");
   return fopen_temporary_or_exit(&file->tmp_path, file->path, mode);
}

static inline void staged_file_finish_or_exit(staged_file_t *file,
                                              FILE *handle) {
   staged_file_require(file);
   filesystem_require(file->tmp_path != NULL,
                      "staged temporary file path is null");
   filesystem_require(!file->finished, "staged file is already finished");
   filesystem_require(handle != NULL, "staged file handle is null");

   file_handle_require_path(handle, file->tmp_path);
   file_sync_or_exit(handle, file->tmp_path);
   fclose_or_exit(handle, file->tmp_path);
   file->finished = true;
}

static inline void staged_file_publish_or_exit(staged_file_t *file) {
   staged_file_require_publishable(file);

   path_rename_or_exit(file->tmp_path, file->path);
   path_sync_parent_dir_or_exit(file->path);
   free(file->tmp_path);
   file->tmp_path = NULL;
   file->finished = false;
}

static inline void file_publish_or_exit(FILE *file, const char *tmp_path,
                                        const char *path) {
   filesystem_require(file != NULL, "file handle is null");
   filesystem_require(tmp_path != NULL, "temporary file path is null");
   filesystem_require(tmp_path[0] != '\0', "temporary file path is empty");
   filesystem_require(path_has_file_name(tmp_path),
                      "temporary file path has no file name");
   filesystem_require(path != NULL, "publish path is null");
   filesystem_require(path[0] != '\0', "publish path is empty");
   filesystem_require(path_has_file_name(path),
                      "publish path has no file name");
   filesystem_require(strcmp(tmp_path, path) != 0,
                      "temporary file path matches target");
   path_require_same_parent_dir(
       tmp_path, path, "temporary file path is not in publish directory");

   file_handle_require_path(file, tmp_path);
   file_sync_or_exit(file, tmp_path);
   fclose_or_exit(file, tmp_path);
   path_rename_or_exit(tmp_path, path);
   path_sync_parent_dir_or_exit(path);
}

#endif // FILESYSTEM_H
