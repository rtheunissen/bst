package trees

import (
   "testing"

   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/utility"
   "github.com/rtheunissen/bst/utility/number"
   "github.com/rtheunissen/bst/utility/number/distribution"
   "github.com/rtheunissen/bst/utility/random"
)

var balancers = []Balancer{
   &Median{},
   &Height{},
   &Weight{},
   &Log{},
   &Cost{},
   &DSW{},
}

func TestBalancers(t *testing.T) {
   distributions := []number.Distribution{
      &distribution.Uniform{},
      &distribution.Normal{},
      &distribution.Skewed{},
      &distribution.Zipf{},
      &distribution.Maximum{},
   }
   testBalancers(t, 1000, balancers, distributions)
}

// Restores balance to a clone of a tree after every insertion, then checks that
// the restored tree has the same values and is balanced, that the tree that was
// cloned was not modified, and that restoring balance again changes nothing.
func testBalancers(t *testing.T, scale list.Size, balancers []Balancer, distributions []number.Distribution) {
   for _, balancer := range balancers {

      t.Run(utility.NameOf(balancer), func(t *testing.T) {

         for _, distribution := range distributions {

            t.Run(utility.NameOf(distribution), func(t *testing.T) {
               list.Seeded(t, func(rng random.Source) {

                  tree := Tree{}
                  reference := list.Reference{}
                  dist := distribution.New(rng.Uint64())

                  for tree.size < scale {

                     i := dist.LessThan(tree.size + 1)
                     x := rng.Uint64()

                     tree.Insert(i, x)
                     reference.Insert(i, x)

                     original := fingerprintOfRoot(tree.root)
                     restored := balancer.Restore(tree.Clone())

                     if !original.equal(fingerprintOfRoot(tree.root)) {
                        t.Fatalf("restoring balance to a clone modified the tree that was cloned")
                     }
                     verifyBalancer(balancer, restored, reference)

                     balanced := fingerprintOfRoot(restored.root)
                     restored = balancer.Restore(restored)

                     if !balanced.equal(fingerprintOfRoot(restored.root)) {
                        t.Fatalf("restoring balance to a balanced tree modified it")
                     }
                     tree = restored
                  }
                  tree.Free()
               })
            })
         }
      })
   }
}

// Restores balance to the worst-case trees for each balancer: a linked list,
// and a linked list that changes direction at its median.
func TestBalancersWorstCase(t *testing.T) {
   for _, balancer := range balancers {
      t.Run(utility.NameOf(balancer), func(t *testing.T) {
         for size := list.Size(0); size <= 300; size++ {
            for _, tree := range []Tree{Tree{}.Vine(size), Tree{}.WorstCaseMedianVine(max(size, 1))} {
               reference := list.Reference{}
               tree.Each(func(x list.Data) {
                  reference = append(reference, x)
               })
               original := fingerprintOfRoot(tree.root)
               restored := balancer.Restore(tree.Clone())

               if !original.equal(fingerprintOfRoot(tree.root)) {
                  t.Fatalf("restoring balance to a clone of size %d modified the tree that was cloned", size)
               }
               verifyBalancer(balancer, restored, reference)
               tree.Free()
            }
         }
      })
   }
}

func verifyBalancer(balancer Balancer, tree Tree, reference list.Reference) {
   tree.verifySizes()
   balancer.Verify(tree)

   i := 0
   tree.Each(func(x list.Data) {
      invariant(x == reference[i])
      i++
   })
   invariant(i == len(reference))
}
