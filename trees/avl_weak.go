package trees

import (
   "github.com/rtheunissen/bst/utility"
)

func verifyAVLWeakHeight(root *Node, size Size) {
   if root == nil {
      return
   }
   h := height(root)
   invariant(rank(root) >= h || h == 0)
   invariant(rank(root) <= 2*h)
   invariant(rank(root) <= 2*int(utility.Log2(size)))
}

func verifyAVLWeakRanks(p *Node) {
   if p == nil {
      return
   }
   if isLeaf(p) {
      invariant(rank(p) == 0)
   }
   invariant(rank(p) > rank(p.l))
   invariant(rank(p) > rank(p.r))

   invariant(isOneChild(p, p.l) || isTwoChild(p, p.l))
   invariant(isOneChild(p, p.r) || isTwoChild(p, p.r))

   verifyAVLWeakRanks(p.l)
   verifyAVLWeakRanks(p.r)
}
