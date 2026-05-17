#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static inline void path_format(char *buf, size_t size, const char *fmt, ...) {
   va_list args;
   va_start(args, fmt);
   int n = vsnprintf(buf, size, fmt, args);
   va_end(args);

   if (n < 0 || (size_t)n >= size) {
      fprintf(stderr, "path too long\n");
      exit(2);
   }
}

static inline void mkdir_p(const char *path) {
   if (path == NULL || path[0] == '\0') {
      fprintf(stderr, "invalid directory path\n");
      exit(2);
   }

   char buf[1024];
   size_t len = strlen(path);
   if (len >= sizeof(buf)) {
      fprintf(stderr, "directory path too long: %s\n", path);
      exit(2);
   }

   memcpy(buf, path, len + 1);
   while (len > 1 && buf[len - 1] == '/') {
      buf[--len] = '\0';
   }

   for (char *p = buf + 1; *p != '\0'; p++) {
      if (*p != '/') {
         continue;
      }
      *p = '\0';
      if (mkdir(buf, 0777) != 0 && errno != EEXIST) {
         perror(buf);
         exit(2);
      }
      *p = '/';
   }

   if (mkdir(buf, 0777) != 0 && errno != EEXIST) {
      perror(buf);
      exit(2);
   }
}

#endif // FILESYSTEM_H
