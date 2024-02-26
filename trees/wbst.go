package trees

import (
   "github.com/rtheunissen/bst/types/list"
   "math"
   "math/big"
)

type WeightBalance interface {
   maxHeight(s list.Size) int
   isBalanced(x, y list.Size) bool
   singleRotation(x, y list.Size) bool
}

type ThreeTwo struct {

}

// The maximum height |h| of a BB[α] tree is log_{1/(1-α)}(n+1).
//
//      α = 1/(Δ+1)
//      Δ = 3
//
//    |h| = log_{1/(1-(1/(Δ+1)))}(n+1)
//        = log_{(Δ+1)/Δ}(n+1)
//        = log_{4/3}(n+1)
//
func (ThreeTwo) maxHeight(s list.Size) int {
   return int(math.Log(float64(s)+1)/math.Log(float64(4)/float64(3)))
}

func (ThreeTwo) isBalanced(x, y list.Size) bool {
   return 3 * (x + 1) >= (y + 1)
}

func (ThreeTwo) singleRotation(x, y list.Size) bool {
   return 2 * (x + 1) > (y + 1)
}

type Rational struct {
   Delta *big.Rat
   Gamma *big.Rat
   Cache map[[2]list.Size]bool
}

func (rat Rational) maxHeight(s list.Size) int {
   delta, _ := rat.Delta.Float64()
   return int(math.Log(float64(s)+1)/math.Log((delta+1)/delta))
}

func (rat Rational) isBalanced(x, y list.Size) bool {
   if x >= y {
      return true
   }
   if rat.Cache == nil {
      rat.Cache = map[[2]list.Size]bool{}
   }
   key := [2]list.Size{x, y}
   if balanced, cached := rat.Cache[key]; cached {
      return balanced
   } else {
      var a big.Rat
      var b big.Rat
      a.SetUint64(x + 1)
      b.SetUint64(y + 1)
      balanced = a.Mul(rat.Delta, &a).Cmp(&b) >= 0
      rat.Cache[key] = balanced
      return balanced
   }
}

func (rat Rational) singleRotation(x, y list.Size) bool {
   if (x + 1) >= (y + 1) {
      return true
   }
   var a, b big.Rat
   a.SetUint64(x + 1)
   b.SetUint64(y + 1)
   single := a.Mul(rat.Gamma, &a).Cmp(&b) > 0
   return single
}

