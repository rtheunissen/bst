package trees

type AVLRelaxed struct {
   RankBalanced
}

func (tree AVLRelaxed) verifyRanks(p *Node) {
  if p == nil {
     return
  }
  // The height of a relaxed AVL tree is no greater than its rank.
  invariant(tree.rank(p) >= p.height())

  // Every rank difference is positive.
  invariant(tree.rank(p) > tree.rank(p.l))
  invariant(tree.rank(p) > tree.rank(p.r))

  tree.verifyRanks(p.l)
  tree.verifyRanks(p.r)
}
