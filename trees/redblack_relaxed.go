package trees

func verifyRedBlackRelaxedHeight(p *Node) {
   invariant(height(p) <= 2 * rank(p) + 1)
}

func verifyRedBlackRelaxedRanks(p *Node) {
  if p == nil {
     return
  }
  invariant(rank(p) >= rank(p.l))
  invariant(rank(p) >= rank(p.r))

   // No parent of a 0-child is a 0-child.
   invariant(!isZeroChild(p, p.l) || !isZeroChild(p.l, p.l.l))
   invariant(!isZeroChild(p, p.l) || !isZeroChild(p.l, p.l.r))
   invariant(!isZeroChild(p, p.r) || !isZeroChild(p.r, p.r.l))
   invariant(!isZeroChild(p, p.r) || !isZeroChild(p.r, p.r.r))

  verifyRedBlackRelaxedRanks(p.l)
  verifyRedBlackRelaxedRanks(p.r)
}
