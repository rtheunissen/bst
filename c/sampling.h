#ifndef SAMPLING_H
#define SAMPLING_H

#include "bst.h"

typedef struct {
   u64 total;
   u64 samples;
} sample_schedule_t;

typedef struct {
   u64 ordinal;
   u64 position;
} sample_schedule_point_t;

static inline void sample_schedule_require(sample_schedule_t schedule) {
   invariant(schedule.total > 0);
   invariant(schedule.samples > 0);
   invariant(schedule.samples <= schedule.total);
}

static inline sample_schedule_t sample_schedule_make(u64 total, u64 samples) {
   if (total == 0) {
      total = 1;
   }
   if (samples == 0) {
      samples = 1;
   }
   if (samples > total) {
      samples = total;
   }
   sample_schedule_t schedule = {.total = total, .samples = samples};
   sample_schedule_require(schedule);
   return schedule;
}

static inline u64 sample_scale_floor(u64 numerator, u64 total,
                                     u64 denominator) {
   invariant(numerator > 0);
   invariant(total > 0);
   invariant(denominator > 0);
   invariant(numerator <= denominator);
   invariant(denominator <= total);
   __uint128_t scaled = (__uint128_t)numerator * (__uint128_t)total;
   u64 result = (u64)(scaled / (__uint128_t)denominator);
   invariant(result > 0);
   invariant(result >= numerator);
   invariant(result <= total);
   return result;
}

static inline u64 sample_position(u64 total, u64 sample, u64 samples) {
   invariant(total > 0);
   invariant(samples > 0);
   invariant(samples <= total);
   invariant(sample > 0);
   invariant(sample <= samples);
   return sample_scale_floor(sample, total, samples);
}

static inline void sample_schedule_ordinal_require(sample_schedule_t schedule,
                                                   u64 ordinal) {
   sample_schedule_require(schedule);
   invariant(ordinal > 0);
   invariant(ordinal <= schedule.samples);
}

static inline u64 sample_schedule_position(sample_schedule_t schedule,
                                           u64 ordinal) {
   sample_schedule_ordinal_require(schedule, ordinal);
   return sample_scale_floor(ordinal, schedule.total, schedule.samples);
}

static inline u64 sample_schedule_index_ordinal(sample_schedule_t schedule,
                                                u64 index) {
   sample_schedule_require(schedule);
   invariant(index < schedule.samples);
   return size_after_inserting_one(index);
}

static inline sample_schedule_point_t
sample_schedule_point(sample_schedule_t schedule, u64 index) {
   u64 ordinal = sample_schedule_index_ordinal(schedule, index);
   u64 position = sample_schedule_position(schedule, ordinal);
   return (sample_schedule_point_t){
       .ordinal = ordinal,
       .position = position,
   };
}

static inline sample_schedule_point_t
sample_schedule_next_point(sample_schedule_t schedule, u64 index,
                           u64 previous_position) {
   sample_schedule_point_t point = sample_schedule_point(schedule, index);
   invariant(point.position > previous_position);
   return point;
}

#endif // SAMPLING_H
