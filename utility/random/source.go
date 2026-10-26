package random

import (
   "sync"

   "golang.org/x/exp/rand"
)

type Source = rand.Source

// The common generator is shared by every caller that does not use a generator
// of its own, so access to it is synchronized to be safe for concurrent use.
var common = synchronized{source: NewXoshiro256(1)}

type synchronized struct {
   mutex  sync.Mutex
   source Xoshiro256
}

func (s *synchronized) Uint64() uint64 {
   s.mutex.Lock()
   defer s.mutex.Unlock()
   return s.source.Uint64()
}

func (s *synchronized) Seed(seed uint64) {
   s.mutex.Lock()
   defer s.mutex.Unlock()
   s.source.Seed(seed)
}

func Seed(seed uint64) {
   common.Seed(seed)
}

func Uint64() uint64 {
   return common.Uint64()
}

func Uniform() Source {
   return &common
}

func New(seed uint64) Source {
   source := NewXoshiro256(seed)
   return &source
}
