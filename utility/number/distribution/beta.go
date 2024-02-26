package distribution

import (
   "github.com/rtheunissen/bst/utility/number"
   "golang.org/x/exp/rand"
   "math"
)

type RandomBeta struct {
   Beta
}
func (RandomBeta) New(seed uint64) number.Distribution {
   return &RandomBeta{
      Beta: Beta{a: rand.Float64()*100+0.01, b: rand.Float64()*100+0.01}.Seed(seed),
   }
}

func (dist *RandomBeta) LessThan(n uint64) uint64 {
   return dist.Beta.LessThan(n)
}

type Beta struct {
   a, b float64
   x, y Gamma
}

func (dist Beta) Seed(seed uint64) Beta {
   if dist.a == 0 || dist.b == 0 {
      panic("alpha and beta must be > 0")
   }
   dist.x = Gamma{}.seed(seed, dist.a)
   dist.y = Gamma{}.seed(seed, dist.b)
   return dist
}

// Float64 returns, as a float64, a pseudo-random number in the half-open interval [0.0,1.0).
func (dist *Beta) Float64() float64 {
   x := dist.x.Float64()
   y := dist.y.Float64()
   return x / (x + y)
}

func (dist *Beta) LessThan(n uint64) uint64 {
   if n == 0 {
      panic("n must be > 0")
   }
   return uint64(math.Ceil(dist.Float64() * float64(n - 1)))
}


// Rand returns a random sample drawn from the distribution.
//
// Rand panics if either alpha or beta is <= 0.
//func (dist *Beta) gamma(shape float64) float64 {
//   return math.Gamma(shape)
   //if shape == 1 {
   //   return rand.ExpFloat64() // Generate from exponential
   //}
   //
   //// Generate using:
   ////  Marsaglia, George, and Wai Wan Tsang. "A simple method for generating
   ////  gamma variables." ACM Transactions on Mathematical Software (TOMS)
   ////  26.3 (2000): 363-372.
   //alpha := shape - 1.0/3
   //m := 1.0
   //if shape < 1 {
   //   alpha += 1.0
   //   m = math.Pow(rand.Float64(), 1/shape)
   //}
   //c := 1 / (3 * math.Sqrt(alpha))
   //for {
   //   x := rand.NormFloat64()
   //   v := 1 + x*c
   //   if v <= 0.0 {
   //      continue
   //   }
   //   v = v * v * v
   //   u := rand.Float64()
   //   if u < 1.0-0.0331*(x*x)*(x*x) {
   //      return m * alpha * v
   //   }
   //   if math.Log(u) < 0.5*x*x+alpha*(1-v+math.Log(v)) {
   //      return m * alpha * v
   //   }
   //}
//}
