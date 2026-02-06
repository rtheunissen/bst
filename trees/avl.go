package trees

import (
   "math"
)

func verifyAVLRanks(p *Node, s Size) int {
   if p == nil {
      return -1
   }
   // AVL rule: Every node is 1,1 or 1,2
   invariant(isOneChild(p, p.l) || isTwoChild(p, p.l))
   invariant(isOneChild(p, p.r) || isTwoChild(p, p.r))
   invariant(isOneChild(p, p.l) || isOneChild(p, p.r))
   invariant(isOneChild(p, p.r) || isOneChild(p, p.l))

   // Verify recursively, returning height.
   invariant(rank(p.l) == verifyAVLRanks(p.l, sizeL(p)))
   invariant(rank(p.r) == verifyAVLRanks(p.r, sizeR(p, s)))

   height := 1 + max(rank(p.l), rank(p.r))

   // The height of every node should not exceed the AVL height bound.
   invariant(rank(p) == height)
   invariant(rank(p) <= int(1.44 * math.Log2(float64(s + 2)) - 0.328))

   return height
}
