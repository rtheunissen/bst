package utility

import (
   "math"
   "math/bits"
)

func Log(n uint64, base float64) uint64 {
   return uint64(math.Log(float64(n))/math.Log(base))
}

// Calculates the integer part of the binary log of `x`:
//
//    ⌊log₂(x)⌋ when x > 0, otherwise 0
//
func Log2(x uint64) uint64 {
   if x == 0 {
      return 0
   } else {
      return uint64(bits.Len64(x) - 1)
   }
}

// Determines if the floor of the binary log of `x` is less than that of `y`:
//
//    ⌊log₂(x)⌋ < ⌊log₂(y)⌋
//
//
func SmallerMSB(x, y uint64) bool {
   return !GreaterThanOrEqualToMSB(x, y)
}

func GreaterThanOrEqualToMSB(x, y uint64) bool {
   return x >= ^x & y
}
