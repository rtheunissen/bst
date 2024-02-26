package trees

import (
   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/utility/random"
)

type TreapFingerTree struct {
   Tree
   random.Source
}

func (TreapFingerTree) New() list.List {
   return &TreapFingerTree{
      Source: random.New(random.Uint64()),
   }
}

func (tree *TreapFingerTree) Clone() list.List {
   return &TreapFingerTree{
      Tree:   tree.Tree.Clone(),
      Source: tree.Source,
   }
}

func (tree *TreapFingerTree) reverseL(p *Node, g *Node, s list.Size) *Node {
   assert(s == p.size())
   for {
      if p == nil {
         return g
      }
      tree.persist(&p)
      sl := p.s
      p.s = s - p.s - 1
      l := p.l
      p.l = g
      g = p
      p = l
      s = sl
   }
}

func (tree *TreapFingerTree) reverseR(p *Node, g *Node) *Node {
   for {
      if p == nil {
         return g
      }
      tree.persist(&p)
      r := p.r
      p.r = g
      g = p
      p = r
   }
}

func (tree *TreapFingerTree) randomRank() uint64 {
   return tree.Uint64()
}

func (tree *TreapFingerTree) rotateParentLeftOnRightSpine(p *Node) {
   tree.persist(&p.r)
   r := p.r // parent on the spine
   p.r = r.r
   r.r = p.l
   p.l = r
   p.s = p.s + r.s + 1
   // measurement(&rotations, 1)
}

func (tree *TreapFingerTree) rotateParentRightOnLeftSpine(p *Node) {
   tree.persist(&p.l)
   l := p.l
   p.l = l.l
   l.l = p.r
   p.r = l
   p.s = p.s + l.s + 1
   l.s = p.s - l.s - 1
   // measurement(&rotations, 1)
}

func (tree *TreapFingerTree) rotateRightIntoRoot(l *Node) {
   assert(l.l == nil)

   p := tree.root

   tree.appendR(&p.r, p)

   l.l = p.l
   p.l = l.r
   l.r = p.r
   p.r = nil
   l.s = p.s - l.s - 1
   p.s = p.s - l.s - 1

   tree.root = l // TODO: consider returning this, accepting p?, not tree
   // measurement(&rotations, 1)
}

func (tree *TreapFingerTree) rotateLeftIntoRoot(r *Node) {
   assert(r.r == nil)

   p := tree.root

   tree.appendL(&p.l, p)

   r.r = p.r
   p.r = r.l
   r.l = p.l
   p.l = nil
   r.s = r.s + p.s + 1
   p.s = r.s - p.s - 1

   tree.root = r
   // measurement(&rotations, 1)
}

func (tree *TreapFingerTree) rotateUpR(p *Node) *Node {
   for {
      if p.r == nil {
         if p.y > tree.root.y {
            tree.rotateLeftIntoRoot(p)
            return nil
         }
      } else {
         if p.y > p.r.y {
            tree.rotateParentLeftOnRightSpine(p)
            continue
         }
      }
      return p
   }
}

func (tree *TreapFingerTree) rotateUpL(p *Node) *Node {
   for {
      if p.l == nil {
         if p.y > tree.root.y {
            tree.rotateRightIntoRoot(p)
            return nil
         }
      } else {
         if p.y > p.l.y {
            tree.rotateParentRightOnLeftSpine(p)
            continue
         }
      }
      return p
   }
}
func (tree *TreapFingerTree) rank(p *Node) uint64 {
   if p == nil {
      return 0
   } else {
      return p.y
   }
}
func (tree *TreapFingerTree) rotateDownL(p *Node) {
   for p.r != nil && p.r.y > p.y {
      tree.persist(&p.r)
      r := p.r
      p.r = r.l
      r.l = p.l
      p.l = r
      r.s = p.s - r.s - 1
      p.s = p.s - r.s - 1
   }
   // measurement(&rotations, 1)
}

func (tree *TreapFingerTree) rotateDownR(p *Node) {
   for p.l != nil && p.l.y > p.y {
      tree.persist(&p.l)
      l := p.l
      p.l = l.r
      l.r = p.r
      p.r = l
      p.s = p.s - l.s - 1
   }
   // measurement(&rotations, 1)
}

func (tree *TreapFingerTree) setHead(p *Node) {
   tree.root.l = p
}

func (tree *TreapFingerTree) setTail(p *Node) {
   tree.root.r = p
}
func (tree *TreapFingerTree) getHead() (p *Node) {
   return tree.root.l
}
func (tree *TreapFingerTree) getTail() (p *Node) {
   return tree.root.r
}

func (tree *TreapFingerTree) insertAsLast(x list.Data) {
   tree.persist(&tree.root)
   p := tree.allocate(Node{x: x, y: tree.randomRank()})
   p.r = tree.getTail()
   p = tree.rotateUpR(p)
   tree.setTail(p)
   tree.size++
}

func (tree *TreapFingerTree) insertAsFirst(x list.Data) {
   tree.persist(&tree.root)
   p := tree.allocate(Node{x: x, y: tree.randomRank()})
   p.l = tree.getHead()
   p = tree.rotateUpL(p)
   tree.setHead(p)
   tree.root.s++
   tree.size++
}

func (tree TreapFingerTree) Select(i list.Position) list.Data {
   assert(i < tree.size)
   switch {
   case i < tree.root.s:
      return tree.accessFromHead(i)
   case i > tree.root.s:
      return tree.accessFromTail(tree.size - i - 1)
   default:
      return tree.root.x
   }
}

func (tree *TreapFingerTree) Update(i list.Position, x list.Data) {
   assert(i < tree.size)
   switch {
   case i < tree.root.s:
      tree.updateFromHead(x, i)
   case i > tree.root.s:
      tree.updateFromTail(x, tree.size-i-1)
   default:
      tree.persist(&tree.root)
      tree.root.x = x
   }
}

func (tree TreapFingerTree) accessFromHead(i list.Position) list.Data {
   p := tree.getHead()
   for {
      if i == 0 {
         return p.x
      }
      if i > p.s {
         i = i - p.s - 1
         p = p.l
      } else {
         return tree.lookup(p.r, i-1)
      }
   }
}

func (tree *TreapFingerTree) updateFromTail(x list.Data, i list.Position) {
   tree.persist(&tree.root)
   tree.persist(&tree.root.r)

   p := tree.root.r
   for {
      if i == 0 {
         p.x = x
         return
      }
      if i > p.s {
         tree.persist(&p.r)
         i = i - p.s - 1
         p = p.r
      } else {
         tree.persist(&p.l)
         tree.update(p.l, p.s-i, x)
         return
      }
   }
}

func (tree *TreapFingerTree) updateFromHead(x list.Data, i list.Position) {
   tree.persist(&tree.root)
   tree.persist(&tree.root.l)

   p := tree.root.l
   for {
      if i == 0 {
         p.x = x
         return
      }
      if i > p.s {
         tree.persist(&p.l)
         i = i - p.s - 1
         p = p.l
      } else {
         tree.persist(&p.r)
         tree.update(p.r, i-1, x)
         return
      }
   }
}

func (tree TreapFingerTree) accessFromTail(i list.Position) list.Data {
   p := tree.getTail()
   for {
      if i == 0 {
         return p.x
      }
      if i > p.s {
         i = i - p.s - 1
         p = p.r
      } else {
         return tree.lookup(p.l, p.s-i)
      }
   }
}

func (tree *TreapFingerTree) insert(p **Node, i list.Position, n *Node) {
   for {
      if *p == nil {
         *p = n
         return
      }
      if (*p).y <= n.y {
         n.l, n.r = tree.Tree.split(*p, i)
         n.s = i
         *p = n
         return
      }
      tree.persist(p)
      if i <= (*p).s {
         p = insertL(*p)
      } else {
         p = insertR(*p, &i)
      }
   }
}

func (tree *TreapFingerTree) insertFromHead(x list.Data, i list.Position) {
   tree.persist(&tree.root)
   tree.persist(&tree.root.l)
   tree.root.s++
   tree.size++

   n := tree.allocate(Node{x: x, y: tree.randomRank()})
   p := tree.root.l
   for {
      //
      if i > p.s {
         tree.persist(&p.l)
         i = i - p.s - 1
         p = p.l
         continue
      }
      //
      if tree.rank(n) > tree.rank(p) {
         p.r, n.r = tree.Tree.split(p.r, i)
         n.s = p.s - i
         p.s = i
         n.l = p.l
         p.l = tree.rotateUpL(n)
         return
      }
      //
      p.s++
      tree.insert(&p.r, i, n)
      return
   }
}

func (tree TreapFingerTree) split(i list.Position) (Tree, Tree) {
   assert(i <= tree.size)
   tree.share(tree.root)
   if i == 0 {
      return Tree{pool: tree.pool},
         Tree{pool: tree.pool, root: tree.root, size: tree.size}
   }
   if i == tree.size {
      return Tree{pool: tree.pool, root: tree.root, size: tree.size},
         Tree{pool: tree.pool}
   }
   if i <= tree.root.s {
      return tree.splitFromHead(i)
   } else {
      return tree.splitFromTail(i)
   }
}

func (tree TreapFingerTree) Split(i list.Position) (list.List, list.List) {
   assert(i <= tree.size)
   l, r := tree.split(i)
   return &TreapFingerTree{Tree: l, Source: tree.Source},
      &TreapFingerTree{Tree: r, Source: tree.Source}
}

func (tree TreapFingerTree) splitFromHead(i list.Position) (Tree, Tree) {
   assert(i <= tree.size)
   assert(i <= tree.root.s)

   tree.persist(&tree.root)

   p := tree.root
   d := i
   for d > (p.l.s + 1) {
      d = d - (p.l.s + 1)
      tree.persist(&p.l)
      p = p.l
   }
   tree.persist(&p.l)
   g := p.l
   p.l = nil
   p = g
   g = p.l

   sl := d - 1
   sr := p.s - sl

   l, r := tree.Tree.split(p.r, sl)

   L := Tree{pool: tree.pool, root: p, size: i}
   R := Tree{pool: tree.pool, root: tree.root, size: tree.size - i}

   L.root.s = i - d
   L.root.l = tree.root.l
   L.root.r = tree.reverseR(l, nil)

   R.root.s = tree.root.s - i
   R.root.r = tree.root.r
   R.root.l = tree.reverseL(r, g, sr)

   return L, R
}

func (tree TreapFingerTree) splitFromTail(i list.Position) (Tree, Tree) {
   assert(i < tree.size)
   assert(i > tree.root.s)

   tree.persist(&tree.root)

   p := tree.root
   d := tree.size - i

   for d > (p.r.s + 1) {
      d = d - (p.r.s + 1)
      tree.persist(&p.r)
      p = p.r
   }
   tree.persist(&p.r)
   g := p.r
   p.r = nil
   p = g
   g = p.r

   sr := d - 1
   sl := p.s - sr

   l, r := tree.Tree.split(p.l, sl)

   L := Tree{pool: tree.pool}
   R := Tree{pool: tree.pool}

   R.root = p
   R.size = tree.size - i
   R.root.s = sr
   R.root.l = tree.reverseL(r, nil, sr)
   R.root.r = tree.root.r

   L.root = tree.root
   L.size = i
   L.root.l = tree.getHead()
   L.root.r = tree.reverseR(l, g)

   return L, R
}

func (tree *TreapFingerTree) insertFromTail(x list.Data, i list.Position) {
   tree.persist(&tree.root)
   tree.persist(&tree.root.r)

   tree.size++

   n := tree.allocate(Node{x: x, y: tree.randomRank()})
   p := tree.root.r
   for {
      //
      if i > p.s {
         tree.persist(&p.r)
         i = i - p.s - 1
         p = p.r
         continue
      }
      //
      if tree.rank(n) > tree.rank(p) {

         n.l, p.l = tree.Tree.split(p.l, p.s-i)
         n.s = p.s - i
         p.s = i
         n.r = p.r
         p.r = tree.rotateUpR(n)
         return
      }
      //
      p.s++
      tree.insert(&p.l, p.s-i-1, n)
      return
   }
}

func (tree *TreapFingerTree) Insert(i list.Position, x list.Data) {
   assert(i <= tree.size)
   if tree.root == nil {
      tree.root = tree.allocate(Node{x: x, y: tree.randomRank()})
      tree.size = 1
      return
   }
   if i <= tree.root.s {
      if i == 0 {
         tree.insertAsFirst(x)
      } else {
         tree.insertFromHead(x, i-1)
      }
   } else {
      if i == tree.size {
         tree.insertAsLast(x)
      } else {
         tree.insertFromTail(x, tree.size-i-1)
      }
   }
}

func (tree *TreapFingerTree) deleteFirst(x *list.Data) {
   defer tree.release(tree.root.l)
   *x = tree.root.l.x
   tree.persist(&tree.root)
   tree.share(tree.root.l.r)
   tree.root.l = tree.reverseL(tree.root.l.r, tree.root.l.l, tree.root.l.s)
   tree.root.s--
}

func (tree *TreapFingerTree) deleteLast(x *list.Data) {
   defer tree.release(tree.root.r)
   *x = tree.root.r.x
   tree.persist(&tree.root)
   tree.share(tree.root.r.l)
   tree.root.r = tree.reverseR(tree.root.r.l, tree.root.r.r)
}

func (tree TreapFingerTree) delete(p **Node, i list.Position, x *list.Data) {
   for {
      if i == (*p).s {
         defer tree.release(*p)
         tree.share((*p).l)
         tree.share((*p).r)
         *x = (*p).x
         *p = tree.join((*p).l, (*p).r, (*p).s)
         return
      }
      tree.persist(p)
      if i < (*p).s {
         p = deleteL(*p)
      } else {
         p = deleteR(*p, &i)
      }
   }
}
func (tree *TreapFingerTree) deleteFromHead(i list.Position, x *list.Data) {
   if i == 0 {
      tree.deleteFirst(x)
      return
   }
   tree.persist(&tree.root)
   tree.persist(&tree.root.l)
   tree.root.s--
   p := tree.root.l
   for i > p.s+1 {
      tree.persist(&p.l)
      i = i - p.s - 1
      p = p.l
   }
   if i < p.s+1 {
      tree.delete(&p.r, i-1, x)
      p.s--
      return
   }
   g := p.l
   defer tree.release(g)
   tree.share(p.r)
   tree.share(g.r)
   *x = g.x
   p.r = tree.join(p.r, g.r, p.s)
   p.l = g.l
   p.s = p.s + g.s
   tree.rotateDownL(p)
}

func (tree *TreapFingerTree) join(l, r *Node, sl list.Size) (root *Node) {
   assert(sl == l.size())
   p := &root
   for {
      if l == nil {
         *p = r
         return
      }
      if r == nil {
         *p = l
         return
      }
      if l.y >= r.y {
         tree.persist(&l)
         sl = sl - l.s - 1
         *p = l
         p = &l.r
         l = *p
      } else {
         tree.persist(&r)
         r.s = r.s + sl
         *p = r
         p = &r.l
         r = *p
      }
   }
}

func (tree *TreapFingerTree) deleteFromTail(i list.Position, x *list.Data) {
   if i == tree.size-1 {
      tree.deleteLast(x)
      return
   }
   i = tree.size - i - 1
   tree.persist(&tree.root)
   tree.persist(&tree.root.r)
   p := tree.root.r
   for i > p.s + 1 {
      tree.persist(&p.r)
      i = i - p.s - 1
      p = p.r
   }
   if i < p.s+1 {
      tree.delete(&p.l, p.s-i, x)
      p.s--
      return
   }
   g := p.r
   defer tree.release(g)
   tree.share(g.l)
   tree.share(p.l)
   *x = g.x
   p.l = tree.join(g.l, p.l, g.s)
   p.r = g.r
   p.s = p.s + g.s
   tree.rotateDownR(p)
}

func (tree *TreapFingerTree) reverseL2(p *Node, g *Node) *Node {
   s := list.Size(0)
   for {
      if p == nil {
         return g
      }
      tree.persist(&p)
      s = s + p.s + 1
      p.s = s - p.s - 1
      l := p.l
      p.l = g
      g = p
      p = l
   }
}
func (tree *TreapFingerTree) deleteRoot(v *list.Data) {
   tree.persist(&tree.root)
   *v = tree.root.x

   // To treap
   tree.root.l = tree.reverseL2(tree.root.l, nil)
   tree.root.r = tree.reverseR(tree.root.r, nil)

   // Dissolve root
   tree.root = tree.join(tree.root.l, tree.root.r, tree.root.s)

   if tree.root == nil {
      return
   }
   tree.root.l = tree.reverseL(tree.root.l, nil, tree.root.s)
   tree.root.r = tree.reverseR(tree.root.r, nil)
}

func (tree *TreapFingerTree) Delete(i list.Position) (v list.Data) {
   assert(i < tree.size)
   switch {
   case i < tree.root.s:
      tree.deleteFromHead(i, &v)
      tree.size--
   case i > tree.root.s:
      tree.deleteFromTail(i, &v)
      tree.size--
   default:
      tree.deleteRoot(&v)
      tree.size--
   }
   return
}

func (tree *TreapFingerTree) joinUp(o *TreapFingerTree) *Node {
   tree.persist(&tree.root)
   tree.persist(&o.root)

   l := tree.root.r
   r := o.root.l
   s := list.Size(0) // size of p

   var p *Node
   for {
      if r == nil {
         tree.root.r = o.root.r
         o.root.l = p
         o.root.r = l
         o.root.s = s

         tree.appendR(&tree.root.r, tree.rotateUpR(o.root))
         return tree.root
      }
      if l == nil {
         o.root.l = tree.root.l
         o.root.s = tree.size + o.root.s
         tree.root.l = r
         tree.root.r = p
         tree.root.s = s

         tree.appendL(&o.root.l, tree.rotateUpL(tree.root))
         return o.root
      }
      if l.y < r.y { // TODO: how does <= affect things? Should we prefer larger size?
         tree.persist(&l)
         s = s + l.s + 1
         g := l.r
         l.r = p
         p = l
         l = g
      } else {
         tree.persist(&r)
         s = s + r.s + 1
         r.s = s - r.s - 1
         g := r.l
         r.l = p
         p = r
         r = g
      }
   }
}

func (tree *TreapFingerTree) Join(that list.List) list.List {
   if tree.Size() == 0 {
      return that.Clone() // TODO: can we avoid this?
   }
   if that.Size() == 0 {
      return tree.Clone() // TODO: can we avoid this?
   }
   l := tree.Clone().(*TreapFingerTree) // TODO: can we avoid this?
   r := that.Clone().(*TreapFingerTree) // TODO: can we avoid this?

   return &TreapFingerTree{
      Tree: Tree{
         pool: tree.pool,
         root: l.joinUp(r),
         size: l.size + r.size,
      },
      Source: tree.Source,
   }
}

func (tree TreapFingerTree) eachFromHead(p *Node, visit func(list.Data)) {
   if p == nil {
      return
   }
   visit(p.x)
   p.r.inorder(visit)
   tree.eachFromHead(p.l, visit)
}

func (tree TreapFingerTree) eachFromTail(p *Node, visit func(list.Data)) {
   if p == nil {
      return
   }
   tree.eachFromTail(p.r, visit)
   p.l.inorder(visit)
   visit(p.x)
}

func (tree TreapFingerTree) inorder(p *Node, visit func(list.Data)) {
   if p == nil {
      return
   }
   tree.eachFromHead(p.l, visit)
   visit(p.x)
   tree.eachFromTail(p.r, visit)
}

func (tree TreapFingerTree) Each(visit func(list.Data)) {
   tree.inorder(tree.root, visit)
}

func (tree TreapFingerTree) verifyRanks() {
   if tree.root == nil {
      return
   }
   l := tree.root.l
   r := tree.root.r
   for ; l != nil; l = l.l {
      TreapTopDown{}.verifyMaxRankHeap(l.r)
      invariant(tree.rank(l) >= tree.rank(l.r))
      invariant(tree.rank(l) <= tree.rank(l.l) || l.l == nil)
   }
   for ; r != nil; r = r.r {
      TreapTopDown{}.verifyMaxRankHeap(r.l)
      invariant(tree.rank(r) >= tree.rank(r.l))
      invariant(tree.rank(r) <= tree.rank(r.r) || r.r == nil)
   }
   invariant(tree.rank(tree.root) >= tree.rank(l))
   invariant(tree.rank(tree.root) >= tree.rank(r))
}

func (tree TreapFingerTree) verifyPositions() {
   if tree.root == nil {
      return
   }
   // The root's size must be equal to the size of the left subtree.
   invariant(tree.root.s == tree.getHead().size())

   // Verify internal positions along the spines.
   for l := tree.getHead(); l != nil; l = l.l {
      tree.verifySize(l.r, l.s)
   }
   for r := tree.getTail(); r != nil; r = r.r {
      tree.verifySize(r.l, r.s)
   }
}

func (tree TreapFingerTree) Verify() {
   invariant(tree.size == tree.root.size())
   tree.verifyPositions()
   tree.verifyRanks()
}