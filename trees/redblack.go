package trees

import (
   "github.com/rtheunissen/bst/types/list"
   "math"
)

type RedBlack struct {
   RankBalanced
}

func (tree RedBlack) isRed(parent, child *Node) bool {
   return tree.isZeroChild(parent, child)
}

func (tree RedBlack) isRedRed(parent *Node) bool {
   return tree.isZeroZero(parent)
}

func (tree RedBlack) isBlack(parent, child *Node) bool {
   return tree.isOneChild(parent, child)
}

func (tree RedBlack) verifyHeight(p *Node, s list.Size) {
   // TODO return height, check for every node
   h := p.height()
   invariant(h <= 2 * tree.rank(p) + 1)
   invariant(h <= 2 * int(math.Log2(float64(s))))
}

func (tree RedBlack) verifyRanks(p *Node) {
   if p == nil {
      return
   }
   if p.isLeaf() {
      invariant(tree.rank(p) == 0)
   }
   invariant(tree.rank(p) >= tree.rank(p.l))
   invariant(tree.rank(p) >= tree.rank(p.r))

   // All rank differences are 0 or 1.
   invariant(tree.isZeroChild(p, p.l) || tree.isOneChild(p, p.l))
   invariant(tree.isZeroChild(p, p.r) || tree.isOneChild(p, p.r))

   // No parent of a 0-child is a 0-child.
   invariant(!tree.isZeroChild(p, p.l) || !tree.isZeroChild(p.l, p.l.l))
   invariant(!tree.isZeroChild(p, p.l) || !tree.isZeroChild(p.l, p.l.r))
   invariant(!tree.isZeroChild(p, p.r) || !tree.isZeroChild(p.r, p.r.l))
   invariant(!tree.isZeroChild(p, p.r) || !tree.isZeroChild(p.r, p.r.r))

   tree.verifyRanks(p.l)
   tree.verifyRanks(p.r)
}
