package utility

import (
   "math"
   "testing"
)

func TestSmallerFloorLog2(t *testing.T) {
   //
   // This is the reference function that we can trust is correct.
   //
   control := func (x, y uint64) bool {
      return math.Floor(math.Log2(float64(x))) < math.Floor(math.Log2(float64(y)))
   }
   //
   // This is the test function, comparing each strategy to the control.
   //
   test := func(t *testing.T, strategy func (x, y uint64) bool) {
      for i := uint64(1); i < 1_000; i++ {
         for j := uint64(1); j < 1_000; j++ {
            if strategy(i, j) != control(i, j) { t.Fatalf("i: %d, j: %d", i, j) }
            if strategy(j, i) != control(j, i) { t.Fatalf("i: %d, j: %d", i, j) }
         }
      }
   }
   //
   // Warren, H.S. (2002). Hacker's Delight. 2nd Edition, section 5-3.
   //
   t.Run("warren", func(t *testing.T) {
      t.Parallel()
      test(t, func(x, y uint64) bool {
         return x < ^x&y
      })
   })
   //
   // Chan, T.M. (2002). Closest-point problems simplified on the RAM. SODA '02.
   //
   t.Run("chan", func(t *testing.T) {
      t.Parallel()
      test(t, func(x, y uint64) bool {
         return x < y && x < (x ^ y)
      })
   })
   //
   // Roura, S. (2001). A New Method for Balancing Binary Search Trees. ICALP.
   //
   t.Run("roura", func(t *testing.T) {
      t.Parallel()
      test(t, func(x, y uint64) bool {
         return x < y && ((x & y) << 1) < y
      })
   })
}

func BenchmarkSmallerFloorLog2(b *testing.B) {
   n := uint64(1_000)
   u := uint64(0)

   b.Run("warren", func(b *testing.B) {
      u = 0
      for i := 0; i < b.N; i++ {
         for x := uint64(0); x < n; x++ {
            for y := uint64(0); y < n; y++ {
               if x < ^x&y {
                  u++
               }
            }
         }
      }
   })

   b.Run("chan", func(b *testing.B) {
      u = 0
      for i := 0; i < b.N; i++ {
         for x := uint64(0); x < n; x++ {
            for y := uint64(0); y < n; y++ {
               if x < y && x < (x^y) {
                  u++
               }
            }
         }
      }
   })

   b.Run("roura", func(b *testing.B) {
      u = 0
      for i := 0; i < b.N; i++ {
         for x := uint64(0); x < n; x++ {
            for y := uint64(0); y < n; y++ {
               if x < y && ((x&y)<<1) < y {
                  u++
               }
            }
         }
      }
   })
}
