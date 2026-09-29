#ifndef BENCHMARK_ORDER_H
#define BENCHMARK_ORDER_H

#include "bst.h"

static inline u64 benchmark_balanced_latin_square_period(int candidate_count) {
   invariant(candidate_count > 0);
   u64 count = (u64)candidate_count;
   return candidate_count % 2 == 0 ? count : count * 2;
}

static inline int benchmark_balanced_latin_square_index(int candidate_count,
                                                        u64 trial,
                                                        int position) {
   invariant(candidate_count > 0);
   invariant(position >= 0);
   invariant(position < candidate_count);

   u64 count = (u64)candidate_count;
   u64 row = trial % count;
   u64 column = (u64)position;

   // A Williams design uses each Latin-square row and, for an odd number of
   // candidates, its reversal. This balances both execution position and the
   // candidate that immediately precedes each candidate.
   if (candidate_count % 2 != 0 && (trial / count) % 2 != 0) {
      column = count - column - 1;
   }

   u64 base = 0;
   if (column % 2 != 0) {
      base = (column + 1) / 2;
   } else if (column != 0) {
      base = count - column / 2;
   }

   return (int)((base + row) % count);
}

#endif // BENCHMARK_ORDER_H
