package trees

func rank(p *Node) int {
   if p == nil {
      return -1
   } else {
      return int(p.y)
   }
}

func rankDifference(parent, child *Node) int {
   assert(rank(parent) >= rank(child))
   return rank(parent) - rank(child)
}

func hasZeroChild(p *Node) bool {
   return isZeroChild(p, p.l) || isZeroChild(p, p.r)
}

func isZeroChild(parent, child *Node) bool {
   return rankDifference(parent, child) == 0
}

func isOneChild(parent, child *Node) bool {
   return rankDifference(parent, child) == 1
}

func isTwoChild(parent, child *Node) bool {
   return rankDifference(parent, child) == 2
}

func isThreeChild(parent, child *Node) bool {
   return rankDifference(parent, child) == 3
}

func promote(p *Node) {
   p.y++
}

func demote(p *Node) {
   p.y--
}

func isZeroZero(p *Node) bool {
   return isZeroChild(p, p.l) && isZeroChild(p, p.r)
}

func isOneOne(p *Node) bool {
   return isOneChild(p, p.l) && isOneChild(p, p.r)
}

func isTwoTwo(p *Node) bool {
   return isTwoChild(p, p.l) && isTwoChild(p, p.r)
}
