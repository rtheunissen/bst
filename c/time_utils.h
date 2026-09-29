#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include "bst.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum {
   TIME_NS_PER_MS = 1000000ULL,
   TIME_NS_PER_SECOND = 1000ULL * TIME_NS_PER_MS,
};

static inline bool duration_sum_would_overflow(u64 duration, u64 increment) {
   return duration > UINT64_MAX - increment;
}

static inline u64 duration_sum(u64 duration, u64 increment) {
   invariant(!duration_sum_would_overflow(duration, increment));
   return duration + increment;
}

static inline u64 duration_ns_from_timespec(struct timespec ts) {
   invariant(ts.tv_sec >= 0);
   invariant(ts.tv_nsec >= 0);
   invariant(ts.tv_nsec < (long)TIME_NS_PER_SECOND);
   invariant((u64)ts.tv_sec <= UINT64_MAX / TIME_NS_PER_SECOND);

   u64 seconds = (u64)ts.tv_sec * TIME_NS_PER_SECOND;
   return duration_sum(seconds, (u64)ts.tv_nsec);
}

static inline u64 monotonic_time_ns(void) {
   struct timespec ts;
   if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
      perror("clock_gettime");
      exit(2);
   }

   return duration_ns_from_timespec(ts);
}

static inline u64 elapsed_time_ns_since(u64 start) {
   u64 now = monotonic_time_ns();
   invariant(now >= start);
   return now - start;
}

static inline u64 nonzero_duration_ns(u64 duration) {
   return duration == 0 ? 1 : duration;
}

static inline u64 duration_average_nonzero(u64 duration, u64 count) {
   invariant(count > 0);
   invariant(duration >= count);
   u64 average = duration / count;
   invariant(average > 0);
   return average;
}

static inline bool duration_reached(u64 elapsed, u64 duration) {
   invariant(duration > 0);
   return elapsed >= duration;
}

static inline bool elapsed_time_ns_reached(u64 start, u64 duration) {
   return duration_reached(elapsed_time_ns_since(start), duration);
}

static inline u64 elapsed_time_ns_since_nonzero(u64 start) {
   return nonzero_duration_ns(elapsed_time_ns_since(start));
}

static inline void format_local_time_or_exit(char *buf, size_t size,
                                             const char *format) {
   invariant(buf != NULL);
   invariant(size > 0);
   invariant(format != NULL);
   invariant(format[0] != '\0');

   time_t now = time(NULL);
   if (now == (time_t)-1) {
      perror("time");
      exit(2);
   }

   struct tm tm;
   if (localtime_r(&now, &tm) == NULL) {
      perror("localtime_r");
      exit(2);
   }

   if (strftime(buf, size, format, &tm) == 0) {
      fprintf(stderr, "time format too long\n");
      exit(2);
   }
}

#endif // TIME_UTILS_H
