package trees

import (
	"math"
)

func isRed(parent, child *Node) bool {
   return isZeroChild(parent, child)
}

func isRedRed(parent *Node) bool {
   return isZeroZero(parent)
}

func isBlack(parent, child *Node) bool {
   return isOneChild(parent, child)
}

func verifyRedBlackHeight(p *Node, s Size) int {
   if p == nil {
      return -1
   }
   l := verifyRedBlackHeight(p.l, sizeL(p))
   r := verifyRedBlackHeight(p.r, sizeR(p, s))
   h := max(l, r) + 1
   
   invariant(h <= 2 * rank(p) + 1)
   invariant(h <= 2 * int(math.Log2(float64(s))))
   return h
}

func verifyRedBlackRanks(p *Node) {
   if p == nil {
      return
   }
   if isLeaf(p) {
      invariant(rank(p) == 0)
   }
   invariant(rank(p) >= rank(p.l))
   invariant(rank(p) >= rank(p.r))

   // All rank differences are 0 or 1.
   invariant(isZeroChild(p, p.l) || isOneChild(p, p.l))
   invariant(isZeroChild(p, p.r) || isOneChild(p, p.r))

   // No parent of a 0-child is a 0-child.
   invariant(!isZeroChild(p, p.l) || !isZeroChild(p.l, p.l.l))
   invariant(!isZeroChild(p, p.l) || !isZeroChild(p.l, p.l.r))
   invariant(!isZeroChild(p, p.r) || !isZeroChild(p.r, p.r.l))
   invariant(!isZeroChild(p, p.r) || !isZeroChild(p.r, p.r.r))

   verifyRedBlackRanks(p.l)
   verifyRedBlackRanks(p.r)
}
