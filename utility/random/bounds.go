package random

import "math/bits"

// LessThan
//
//    - https://news.ycombinator.com/item?id=28396077
//    - https://www.pcg-random.org/posts/bounded-rands.html
//    - https://arxiv.org/pdf/1805.10941.pdf
//    - https://github.com/apple/swift/pull/39143/files
//
// TODO: check what happens when we pass 0
//
func LessThan(n uint64, random Source) uint64 {
   a, f := bits.Mul64(n, random.Uint64()); /* optional */ if f <= -n { return a }
   b, _ := bits.Mul64(n, random.Uint64())
   _, c := bits.Add64(f, b, 0)
   return a + c
}
