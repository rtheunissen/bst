package number

import (
   "github.com/rtheunissen/bst/utility"
   "github.com/rtheunissen/bst/utility/number/distribution"
   "github.com/rtheunissen/bst/utility/random"
   "testing"
)

func TestDistributions(t *testing.T) {
  distributions := []Distribution{
    &distribution.Uniform{},
    &distribution.Normal{},
    &distribution.Skewed{},
    &distribution.Median{},
    &distribution.Ascending{},
    &distribution.Descending{},
  }
  for _, instance := range distributions {
    t.Run(utility.NameOf(instance), func(t *testing.T) {
       t.Parallel()
       for s := 1; s <= 5; s++ {
          position := instance.New(uint64(s))
          for i := 0; i < 1_000_000; i++ {
             if n := random.Uint64(); position.LessThan(n) >= n {
                t.Fatalf("broken distribution")
             }
          }
       }
    })
  }
}
