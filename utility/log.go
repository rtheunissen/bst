package utility

import (
   "golang.org/x/exp/constraints"
   "math"
   "math/bits"
)

func Log[T constraints.Integer](n T, base float64) T {
   return T(math.Log(float64(n))/math.Log(base))
}

// Calculates the integer part of the binary log of `x`:
//
//    ⌊log₂(x)⌋ when x > 0, otherwise 0
//
func Log2[T constraints.Unsigned](x T) T {
   if x == 0 {
      return 0
   } else {
      return T(bits.Len64(uint64(x)) - 1)
   }
}

// Determines if the floor of the binary log of `x` is less than that of `y`:
//
//    ⌊log₂(x)⌋ < ⌊log₂(y)⌋
//
//
func SmallerMSB[T constraints.Unsigned](x, y T) bool {
   return !GreaterThanOrEqualToMSB(x, y)
   //return x < ^x & y                   // Warren, H.S. (2002). Hacker's Delight. 2nd Edition, section 5-3.
   //     x < y && x < (x^y)           // Chan, T.M. (2002). Closest-point problems simplified on the RAM. SODA '02.
   //     x < y && ((x&y) << 1) < y    // Roura, S. (2001). A New Method for Balancing Binary Search Trees. ICALP.
}

func GreaterThanOrEqualToMSB[T constraints.Unsigned](x, y T) bool {
   return x >= ^x & y                   // Warren, H.S. (2002). Hacker's Delight. 2nd Edition, section 5-3.
   //return x >= y || x >= (x^y)           // Chan, T.M. (2002). Closest-point problems simplified on the RAM. SODA '02.
   //return x >= y || ((x&y) << 1) >= y    // Roura, S. (2001). A New Method for Balancing Binary Search Trees. ICALP.
}
