package trees

type RedBlackRelaxed struct {
  RankBalanced
}

func (tree RedBlackRelaxed) verifyHeight(p *Node) {
   invariant(p.height() <= 2 * tree.rank(p) + 1)
}

func (tree RedBlackRelaxed) verifyRanks(p *Node) {
  if p == nil {
     return
  }
  invariant(tree.rank(p) >= tree.rank(p.l))
  invariant(tree.rank(p) >= tree.rank(p.r))

   // No parent of a 0-child is a 0-child.
   invariant(!tree.isZeroChild(p, p.l) || !tree.isZeroChild(p.l, p.l.l))
   invariant(!tree.isZeroChild(p, p.l) || !tree.isZeroChild(p.l, p.l.r))
   invariant(!tree.isZeroChild(p, p.r) || !tree.isZeroChild(p.r, p.r.l))
   invariant(!tree.isZeroChild(p, p.r) || !tree.isZeroChild(p.r, p.r.r))

  tree.verifyRanks(p.l)
  tree.verifyRanks(p.r)
}