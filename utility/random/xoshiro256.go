package random

import (
   "math/bits"
)

// Xoshiro256 is the xoshiro256** generator by Blackman and Vigna.
//
//    - https://prng.di.unimi.it/xoshiro256starstar.c
//
// A generator can be stored by value so that copying a data structure also
// copies its generator, and both copies then draw values independently.
//
// The zero value is not seeded: the all-zero state is a fixed point that only
// ever produces zero, so a generator must be seeded before it is used.
type Xoshiro256 [4]uint64

// NewXoshiro256 returns a generator seeded with the given seed.
func NewXoshiro256(seed uint64) (source Xoshiro256) {
   source.Seed(seed)
   return
}

func (source *Xoshiro256) Seed(seed uint64) {
   s64 := SplitMix64(seed)
   source[0] = s64.Uint64()
   source[1] = s64.Uint64()
   source[2] = s64.Uint64()
   source[3] = s64.Uint64()
}

func (source *Xoshiro256) Uint64() uint64 {
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

func (source *Xoshiro256) Int63() int64 {
   return int64(source.Uint64() >> 1)
}

// Fork returns a new generator seeded by the next value of this generator,
// without advancing this generator. The receiver is a copy, so forking does not
// modify the original, which keeps forking safe for concurrent readers.
func (source Xoshiro256) Fork() Xoshiro256 {
   return NewXoshiro256(source.Uint64())
}
