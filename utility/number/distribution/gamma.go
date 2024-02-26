package distribution

import (
   "github.com/rtheunissen/bst/utility/random"
   "golang.org/x/exp/rand"
   "math"
)

type Gamma struct {
   *rand.Rand
   alpha float64
}

func (Gamma) seed(seed uint64, alpha float64) Gamma {
   return Gamma{
      Rand:  rand.New(random.New(seed)),
      alpha: alpha,
   }
}

// Float64
//
// Generate using:
//
//   Marsaglia, George, and Wai Wan Tsang. "A simple method for generating
//   gamma variables." ACM Transactions on Mathematical Software (TOMS)
//   26.3 (2000): 363-372.
//
func (dist *Gamma) Float64() float64 {
   if dist.alpha == 1 {
      return dist.Rand.ExpFloat64()
   }
   d := dist.alpha - 1.0/3
   m := 1.0
   if dist.alpha < 1 {
      d += 1.0
      m = math.Pow(dist.Rand.Float64(), 1/dist.alpha)
   }
   c := 1 / (3 * math.Sqrt(d))
   for {
      x := dist.Rand.NormFloat64()
      v := 1 + x*c
      if v <= 0.0 {
         continue
      }
      v = v * v * v
      u := dist.Rand.Float64()
      if u < 1.0-0.0331*(x*x)*(x*x) {
         return m * d * v
      }
      if math.Log(u) < 0.5*x*x+d*(1-v+math.Log(v)) {
         return m * d * v
      }
   }
}
