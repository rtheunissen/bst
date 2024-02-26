package trees

import (
   "github.com/rtheunissen/bst/types/list"
   "math"
)

type AVL struct {
   RankBalanced
}

func (tree AVL) verifyRanks(p *Node, s list.Size) int {
   if p == nil {
      return -1
   }
   // AVL rule: Every node is 1,1 or 1,2
   invariant(tree.isOneChild(p, p.l) || tree.isTwoChild(p, p.l))
   invariant(tree.isOneChild(p, p.r) || tree.isTwoChild(p, p.r))
   invariant(tree.isOneChild(p, p.l) || tree.isOneChild(p, p.r))
   invariant(tree.isOneChild(p, p.r) || tree.isOneChild(p, p.l))

   // Verify recursively, returning height.
   invariant(tree.rank(p.l) == tree.verifyRanks(p.l, p.sizeL()))
   invariant(tree.rank(p.r) == tree.verifyRanks(p.r, p.sizeR(s)))

   height := 1 + max(tree.rank(p.l), tree.rank(p.r))

   // The height of every node should not exceed the AVL height bound.
   invariant(tree.rank(p) == height)
   invariant(tree.rank(p) <= int(1.44 * math.Log2(float64(s + 2)) - 0.328))

   return height
}
