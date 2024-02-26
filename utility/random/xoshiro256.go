package random

import (
   "math/bits"
)

// TODO: which exactly?
type xoshiro256 [4]uint64

func (source *xoshiro256) Seed(seed uint64) {
   s64 := SplitMix64(seed)
   source[0] = s64.Uint64()
   source[1] = s64.Uint64()
   source[2] = s64.Uint64()
   source[3] = s64.Uint64()
}

func (source *xoshiro256) Uint64() uint64 {
   result := bits.RotateLeft64(source[1]*5, 7) * 9

   t := source[1] << 17

   source[2] ^= source[0]
   source[3] ^= source[1]
   source[1] ^= source[2]
   source[0] ^= source[3]

   source[2] ^= t
   source[3] = bits.RotateLeft64(source[3], 45)

   return result
}

func (source *xoshiro256) Int63() int64 {
   return int64(source.Uint64() >> 1)
}

