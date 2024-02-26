package distribution

import (
   "github.com/rtheunissen/bst/utility/number"
)

type Minimum struct {
}

func (Minimum) New(uint64) number.Distribution {
   return Minimum{}
}

func (Minimum) LessThan(uint64) uint64 {
   return 0
}
