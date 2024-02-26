package random

type SplitMix64 uint64

func (s64 *SplitMix64) Uint64() uint64 {
   *s64 += 0x9E3779B97F4A7C15

   var z = uint64(*s64)

   z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9
   z = (z ^ (z >> 27)) * 0x94D049BB133111EB
   z = (z ^ (z >> 31))

   return z
}

func (s64 *SplitMix64) Seed(seed uint64) {
   *s64 = SplitMix64(seed)
}

func (s64 *SplitMix64) Int63() int64 {
   return int64(s64.Uint64() >> 1)
}
