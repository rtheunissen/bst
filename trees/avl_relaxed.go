package trees

func verifyAVLRelaxedRanks(p *Node) {
  if p == nil {
     return
  }
  // The height of a relaxed AVL tree is no greater than its rank.
  invariant(rank(p) >= height(p))

  // Every rank difference is positive.
  invariant(rank(p) > rank(p.l))
  invariant(rank(p) > rank(p.r))

  verifyAVLRelaxedRanks(p.l)
  verifyAVLRelaxedRanks(p.r)
}
