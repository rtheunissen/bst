package distribution

import (
   "github.com/rtheunissen/bst/utility/number"
)

type Maximum struct {
}

func (Maximum) New(uint64) number.Distribution {
   return Maximum{}
}

func (Maximum) LessThan(n uint64) uint64 {
   if n == 0 {
      return n
   } else {
      return n - 1
   }
}
