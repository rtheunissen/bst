package trees

import (
   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/utility"
   "github.com/rtheunissen/bst/utility/number"
   "github.com/rtheunissen/bst/utility/number/distribution"
   "github.com/rtheunissen/bst/utility/random"
   "testing"
)

func TestBalancers(t *testing.T) {
   balancers := []Balancer{
      &Median{},
      &Height{},
      &Weight{},
      &Log{},
      &Cost{},
      &DSW{},
   }
   distributions := []number.Distribution{
      &distribution.Uniform{},
      &distribution.Normal{},
      &distribution.Skewed{},
      &distribution.Zipf{},
      &distribution.Maximum{},
   }
   testBalancers(t, 1000, balancers, distributions) // TODO: make consistent with test suites and benchmarks patterns exactly
}

func testBalancers(t *testing.T, scale list.Size, balancers []Balancer, distributions []number.Distribution) {
   for _, balancer := range balancers {

      t.Run(utility.NameOf(balancer), func(t *testing.T) {

         for _, distribution := range distributions {

            t.Run(utility.NameOf(distribution), func(t *testing.T) {

               tree := Tree{}
               reference := list.Reference{}
               dist := distribution.New(1)

               for tree.size < scale {

                  i := dist.LessThan(tree.size + 1)
                  x := random.Uint64()

                  tree.Insert(i, x)
                  reference.Insert(i, x)

                  tree = balancer.Restore(tree.Clone())
                  tree = balancer.Restore(tree.Clone())

                  balancer.Verify(tree)
               }
               tree.Free()
            })
         }
      })
   }
}
