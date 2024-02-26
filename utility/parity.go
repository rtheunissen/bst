package utility

import (
   "golang.org/x/exp/constraints"
)

func Even[T constraints.Integer](i T) bool {
   return i & 1 == 0
}