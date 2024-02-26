package trees

import (
   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/utility"
)

type AVLWeak struct {
   RankBalanced
}

func (tree AVLWeak) verifyHeight(root *Node, size list.Size) {
   if root == nil {
      return
   }
   height := root.height()
   invariant(tree.rank(root) >= height || height == 0)
   invariant(tree.rank(root) <= 2*height)
   invariant(tree.rank(root) <= 2*int(utility.Log2(size)))
}

func (tree AVLWeak) verifyRanks(p *Node) {
   if p == nil {
      return
   }
   if p.isLeaf() {
      invariant(tree.rank(p) == 0)
   }
   invariant(tree.rank(p) > tree.rank(p.l))
   invariant(tree.rank(p) > tree.rank(p.r))

   invariant(tree.isOneChild(p, p.l) || tree.isTwoChild(p, p.l))
   invariant(tree.isOneChild(p, p.r) || tree.isTwoChild(p, p.r))

   tree.verifyRanks(p.l)
   tree.verifyRanks(p.r)
}
