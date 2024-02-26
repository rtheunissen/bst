package distribution

import (
   "github.com/rtheunissen/bst/utility/number"
)

type Median struct {
}

func (Median) New(uint64) number.Distribution {
   return Median{}
}

func (Median) LessThan(n uint64) uint64 {
   return n / 2
}
