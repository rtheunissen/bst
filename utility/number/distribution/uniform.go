package distribution

import (
   "github.com/rtheunissen/bst/utility/number"
   random "github.com/rtheunissen/bst/utility/random"
)

type Uniform struct {
   random.Source
}

func (uniform Uniform) New(seed uint64) number.Distribution {
   uniform.Source = random.New(seed)
   return &uniform
}

func (uniform *Uniform) LessThan(n uint64) uint64 {
   if n == 0 {
      panic("n must be > 0")
   }
   return random.LessThan(n, uniform.Source)
}
