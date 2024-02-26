package main

import (
   "github.com/rtheunissen/bst/trees"
   "math"
)

func B(height, size uint64) bool {
   return float64(height) > 2*math.Log2(float64(size))
   //          ≡ height - 1 > 2 * ⌊log₂(weight)⌋
   //          ≡ height / 2 > ⌊log₂(weight)⌋
   //    return x - 1 > 2 * uint64(math.Floor(math.Log2(float64(y))))
}
func A(height, size uint64) bool {
   return uint64(float64(height)/2) > uint64(math.Floor(math.Log2(float64(size+1))))
   // return (height) / 2 > uint64(math.Floor(math.Log2(float64(size))))
   // return x / 2 > uint64(math.Floor(math.Log2(float64(y))))
   //return math2.SmallerFloorLog2(y, 1 << (x >> 1))
   // return uint64(math.Log2(float64(y))) < uint64(math.Log2(float64(uint64(1) << ((x + 1) / 2))))

   // return math.Ceil(float64(x) / 2) > float64(uint64(math.Log2(float64(y))))
   // return utility.SmallerFloorLog2(y, uint64(1) << ((x >> 1) + (x & 1)))
   // return uint64(math.Log2(float64(uint64(1) << ((x >> 1) + (x & 1))))) > uint64(math.Log2(float64(y)))
   // if x > y {
   //    return x - y <= 1
   // } else {
   //    return y - x <= 1
   // }
   // return a > int(math.Floor(math.Log2(float64(b))))
}

// d is a

func main() {
   trees.Sandbox()
   //
   //
   //height := 20
   //C := map[int]uint64{}
   //g := distributions.UShape{}.Seed(123)
   //N := 100_000
   //g.Seed(1200)
   //
   //// First we count the distribution across a range S
   //for i := 0; i < N; i++ {
   //  C[int(float64(g.LessThan(uint64(height))))]++
   //}
   //
   //fmt.Print(console.Clear)
   //
   //buf := strings.Builder{}
   //
   //for i := 0; i < height; i++ {
   //   barW := math.Ceil(float64(C[i]))/(float64(N) / 200)
   //   buf.WriteString(utility.PadLeft(fmt.Sprint(i), 3) + " " + utility.Repeat("▓", int(barW)) + "\n")
   //}
   //fmt.Print(buf.String())
   //time.Sleep(time.Millisecond)

   // p := tree.CreateWorstCaseIvoMuusseTree(7)
   //
   // spew.Dump(p)

   // avl := tree.Splay{}
   // prng := distributions.Uniform{}
   // prng.Seed(123)
   //
   // for i := 0; i < 1_000000; i++ {
   //    avl.Insert(prng.LessThan(uint64(i+1)), Data(i))
   // }
   //
   // p := avl.Root()
   // p = tree.Median{}.Balance(p, avl.Size())
   //

   // r1 := random.NewSource(1)
   //
   // t1 := tree.Randomized{
   //    BST:    tree.BST{},
   //    Source: *r1,
   // }
   // t2 := t1.Clone().(*tree.Randomized)
   //
   // spew.Dump(t1.Source)
   // spew.Dump(t2.Source)
   //
   // t1.Insert(0, 0)
   // t1.Insert(1, 0)
   // t1.Insert(2, 0)
   // t1.Insert(3, 0)
   //
   // spew.Dump(t1.Source)
   // spew.Dump(t2.Source)

   // tree.Sandbox()

   //
   // for i := uint64(0); i < 20; i++ {
   //    for j := uint64(0); j < 20; j++ {
   //       if isWeightBalanced(i, j) {
   //          println(i, j, "BALANCED")
   //       } else {
   //          println(i, j, "NOT BALANCED")
   //       }
   //    }
   // }
   // n := 20_000
   // rand.Seed(1)
   // for {
   //    t := trees.BST{}.New(&trees.LBSTRelaxed{})
   //    for i := 0; i < n; i++ {
   //       t._insert(trees.Size(rand.Intn(int(t.Size+1))), nil)
   //       t.Verify()
   //    }
   //    print(".")
   // }
   // println(trees.TOTAL_SCAPEGOATS)

   // for height := uint64(1); height < 128; height++ {
   //   for size := uint64(1); size < 1000000; size++ {
   //      if A(height, size) != B(height, size) {
   //         panic(fmt.Sprint(height, size))
   //      }
   //   }
   // }
   // bst := tree.New{Strategy: tree.Randomized{}}
   // bst._insert(0, "x")
   // for k, v := range bst.toArray() {
   //    println(k, fmt.Sprint(v))
   // }

   //println(utility.PowerOf2LessThanOrEqualTo(14))
   //println(utility.PowerOf2LessThanOrEqualTo(15))
   //println(utility.PowerOf2LessThanOrEqualTo(16))
   //println(utility.PowerOf2LessThanOrEqualTo(17))
}
