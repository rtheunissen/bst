package trees

import (
   "fmt"
   "github.com/rtheunissen/bst/utility/random"
)

// This experiment measures the number of rotations per update
// in a top-down red-black tree.
//
// This requires `make measurements-on` to track rotation count.
//
func redBlackTopDownRotations() {
   tree := RedBlackTopDown{}
   size := Size(10_000_000)
   rotations := Rotations{}
   //
   // INSERT
   //
   totalRotations := uint64(0)
   for i := Size(0); i < size; i++ {
      tree.Insert(random.Uint64() % (tree.size + 1), 0)
      totalRotations += rotations.Measure(tree).(uint64)
      invariant(rotations.Measure(tree).(uint64) <= 6) // !!
      rotations.Reset()
   }
   fmt.Println("Insert", "total rotations / operations", float64(totalRotations) / float64(size))
   invariant(totalRotations <= size)
   //
   // DELETE
   //
   totalRotations = uint64(0)
   for i := Size(0); i < size; i++ {
      tree.Delete(random.Uint64() % tree.size)
      totalRotations += rotations.Measure(tree).(uint64)
      invariant(rotations.Measure(tree).(uint64) <= 6) // !!
      rotations.Reset()
   }
   fmt.Println("Delete", "total rotations / operations", float64(totalRotations) / float64(size))
   invariant(totalRotations <= size)
}

func Sandbox() {
   redBlackTopDownRotations()
}