package trees

type RankBalanced struct {
}

func (tree RankBalanced) rank(p *Node) int {
   if p == nil {
      return -1
   } else {
      return int(p.y)
   }
}

func (tree RankBalanced) rankDifference(parent, child *Node) int {
   assert(tree.rank(parent) >= tree.rank(child))
   return tree.rank(parent) - tree.rank(child)
}

func (tree RankBalanced) hasZeroChild(p *Node) bool {
   return tree.isZeroChild(p, p.l) || tree.isZeroChild(p, p.r)
}

func (tree RankBalanced) isZeroChild(parent, child *Node) bool {
   return tree.rankDifference(parent, child) == 0
}

func (tree RankBalanced) isOneChild(parent, child *Node) bool {
   return tree.rankDifference(parent, child) == 1
}

func (tree RankBalanced) isTwoChild(parent, child *Node) bool {
   return tree.rankDifference(parent, child) == 2
}

func (tree RankBalanced) isThreeChild(parent, child *Node) bool {
   return tree.rankDifference(parent, child) == 3
}

func (tree RankBalanced) promote(p *Node){
   p.y++
}

func (tree RankBalanced) demote(p *Node) {
   p.y--
}

func (tree RankBalanced) isZeroZero(p *Node) bool {
   return tree.isZeroChild(p, p.l) && tree.isZeroChild(p, p.r)
}

func (tree RankBalanced) isOneOne(p *Node) bool {
   return tree.isOneChild(p, p.l) && tree.isOneChild(p, p.r)
}

func (tree RankBalanced) isTwoTwo(p *Node) bool {
   return tree.isTwoChild(p, p.l) && tree.isTwoChild(p, p.r)
}
