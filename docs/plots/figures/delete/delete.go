package main

import (
   "fmt"
   "github.com/rtheunissen/bst/trees"
   "github.com/rtheunissen/bst/utility/random"
   "os"
)

// @${GO} run docs/plots/dissolve/dissolve.go
//
// This requires that you change what Tree.dissolve calls,
// and also to use GC rather than Arena because the many insert/delete
// do not release the deleted nodes to a free pool.
//
func main()  {

   size := 1_000

   steps := 1_000 * size * size // 1_000 * (1_000 * 1_000) = 1_000_000_000 = 1B = 1_000 * n^2

   samples := 1_000

   iterations := 1

   fmt.Println("Step", "Steps", "Size", "AveragePathLength")

   for i := 0; i < iterations; i++ {
      t := trees.Tree{}
      r := random.New(uint64(i))
      random.Seed(uint64(i))

      fmt.Fprint(os.Stderr, ".")

      // Grow
      for n := 0; n < size; n++ {
         t.Insert(random.LessThan(t.Size() + 1, r), 0)
         t.Insert(random.LessThan(t.Size() + 1, r), 0)
         t.Delete(random.LessThan(t.Size(), r))
      }
      // Step
      for j := 0; j < steps; j++ {
         t.Insert(random.LessThan(t.Size() + 1, r), 0)
         t.Delete(random.LessThan(t.Size(), r))

         if (j+1) % (steps/samples) == 0 {
            fmt.Println(j+1, steps, size, t.Root().AveragePathLength())
         }
      }
      t.Free()
   }
}