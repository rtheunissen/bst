package random

import (
   "golang.org/x/exp/rand"
)

type Source = rand.Source

var common = xoshiro256{}

func init() {
   common.Seed(1)
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
   source := xoshiro256{}
   source.Seed(seed)
   return &source
}
