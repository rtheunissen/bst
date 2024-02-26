package distribution

import (
   "github.com/rtheunissen/bst/utility/number"
)

type Skewed struct {
   Beta
}

func (Skewed) New(seed uint64) number.Distribution {
   return &Skewed{Beta{a: 100, b: 50}.Seed(seed)}
}
