#include "bst.h"

int util_redblack_height_bound(u64 size) {
   invariant(size > 0);
   u64 log = util_log2(size);
   invariant(log <= 63);
   u64 bound = size_sum(log, log);
   __uint128_t square = (__uint128_t)size * (__uint128_t)size;
   u64 half_step_shift = size_after_inserting_one(bound);
   invariant(half_step_shift < 128);
   __uint128_t next_half_step = (__uint128_t)1 << half_step_shift;
   if (square >= next_half_step) {
      bound = size_after_inserting_one(bound);
   }
   invariant(bound <= (u64)INT_MAX);
   return (int)bound;
}

int util_avl_height_bound(u64 size) {
   invariant(size > 0);
   // min_size[h] = min_size[h - 1] + min_size[h - 2] + 1
   static const u64 min_size[] = {1ULL,
                                  2ULL,
                                  4ULL,
                                  7ULL,
                                  12ULL,
                                  20ULL,
                                  33ULL,
                                  54ULL,
                                  88ULL,
                                  143ULL,
                                  232ULL,
                                  376ULL,
                                  609ULL,
                                  986ULL,
                                  1596ULL,
                                  2583ULL,
                                  4180ULL,
                                  6764ULL,
                                  10945ULL,
                                  17710ULL,
                                  28656ULL,
                                  46367ULL,
                                  75024ULL,
                                  121392ULL,
                                  196417ULL,
                                  317810ULL,
                                  514228ULL,
                                  832039ULL,
                                  1346268ULL,
                                  2178308ULL,
                                  3524577ULL,
                                  5702886ULL,
                                  9227464ULL,
                                  14930351ULL,
                                  24157816ULL,
                                  39088168ULL,
                                  63245985ULL,
                                  102334154ULL,
                                  165580140ULL,
                                  267914295ULL,
                                  433494436ULL,
                                  701408732ULL,
                                  1134903169ULL,
                                  1836311902ULL,
                                  2971215072ULL,
                                  4807526975ULL,
                                  7778742048ULL,
                                  12586269024ULL,
                                  20365011073ULL,
                                  32951280098ULL,
                                  53316291172ULL,
                                  86267571271ULL,
                                  139583862444ULL,
                                  225851433716ULL,
                                  365435296161ULL,
                                  591286729878ULL,
                                  956722026040ULL,
                                  1548008755919ULL,
                                  2504730781960ULL,
                                  4052739537880ULL,
                                  6557470319841ULL,
                                  10610209857722ULL,
                                  17167680177564ULL,
                                  27777890035287ULL,
                                  44945570212852ULL,
                                  72723460248140ULL,
                                  117669030460993ULL,
                                  190392490709134ULL,
                                  308061521170128ULL,
                                  498454011879263ULL,
                                  806515533049392ULL,
                                  1304969544928656ULL,
                                  2111485077978049ULL,
                                  3416454622906706ULL,
                                  5527939700884756ULL,
                                  8944394323791463ULL,
                                  14472334024676220ULL,
                                  23416728348467684ULL,
                                  37889062373143905ULL,
                                  61305790721611590ULL,
                                  99194853094755496ULL,
                                  160500643816367087ULL,
                                  259695496911122584ULL,
                                  420196140727489672ULL,
                                  679891637638612257ULL,
                                  1100087778366101930ULL,
                                  1779979416004714188ULL,
                                  2880067194370816119ULL,
                                  4660046610375530308ULL,
                                  7540113804746346428ULL,
                                  12200160415121876737ULL};
   const u64 count = sizeof(min_size) / sizeof(min_size[0]);
   u64 lo = 0;
   u64 hi = count;
   while (lo < hi) {
      u64 span = suffix_size_after_prefix(hi, lo);
      u64 mid = size_sum(lo, span / 2);
      if (min_size[mid] <= size) {
         lo = size_after_inserting_one(mid);
      } else {
         hi = mid;
      }
   }
   invariant(lo > 0);
   invariant(lo <= count);
   u64 max_height = rank_of_last(lo);
   invariant(min_size[max_height] <= size);
   invariant(lo == count || size < min_size[lo]);
   invariant(max_height <= (u64)INT_MAX);
   return (int)max_height;
}
