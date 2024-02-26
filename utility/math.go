package utility

import (
   "golang.org/x/exp/constraints"
)

func Distance[T constraints.Integer](a, b T) T {
   if a > b {
      return a - b
   } else {
      return b - a
   }
}
