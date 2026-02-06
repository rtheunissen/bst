package trees

import (
   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/utility"
)

// AA
//
// An AA tree, based on the balanced binary search tree by Arne Andersson.
// The AA tree is a simplification of the red-black tree where red links may
// only lean right. This is the inverse of the LLRB constraint.
//
// Using the rank-balanced framework:
//
//    Red link   = 0-child (rank difference 0)
//    Black link = 1-child (rank difference 1)
//
//    AA rule: a node may only have a right 0-child, never a left 0-child.
//    Also, no two consecutive right 0-children are allowed.
//
// Two operations maintain the invariant:
//
//    skew:  if the left child is a 0-child, rotate right.
//    split: if the right grandchild is a 0-child of the right child which
//           is itself a 0-child, rotate left and promote.
//
type AA struct {
   Tree
   RankBalanced
}

func (AA) New() list.List {
   return &AA{}
}

func (tree *AA) Clone() list.List {
   return &AA{
      Tree: tree.Tree.Clone(),
   }
}

func (tree AA) skew(p **Node) {
   if *p != nil && tree.isZeroChild(*p, (*p).l) {
      tree.treeRotateR(p)
   }
}

func (tree AA) aaSplit(p **Node) {
   if *p != nil && (*p).r != nil &&
      tree.isZeroChild(*p, (*p).r) &&
      tree.isZeroChild((*p).r, (*p).r.r) {
      tree.treeRotateL(p)
      tree.promote(*p)
   }
}

func (tree *AA) insert(p *Node, i Position, x Data) *Node {
   if p == nil {
      return tree.allocate(Node{x: x})
   }
   tree.persist(&p)
   if i <= p.s {
      p.s = p.s + 1
      p.l = tree.insert(p.l, i, x)
   } else {
      p.r = tree.insert(p.r, i - p.s - 1, x)
   }
   tree.skew(&p)
   tree.aaSplit(&p)
   return p
}

func (tree *AA) Insert(i Position, x Data) {
   assert(i <= tree.size)
   tree.root = tree.insert(tree.root, i, x)
   tree.size++
}

func (tree AA) decreaseLevel(p *Node) {
   rl := tree.rank(p.l)
   rr := tree.rank(p.r)

   shouldBe := 1 + min(rl, rr)

   if shouldBe < tree.rank(p) {
      p.y = uint64(shouldBe)
      if shouldBe < rr {
         tree.persist(&p.r)
         p.r.y = uint64(shouldBe)
      }
   }
}

func (tree AA) fixAfterDelete(p *Node) *Node {
   tree.decreaseLevel(p)
   tree.skew(&p)
   if p.r != nil {
      tree.persist(&p.r)
      tree.skew(&p.r)
      if p.r.r != nil {
         tree.persist(&p.r.r)
         tree.skew(&p.r.r)
      }
   }
   tree.aaSplit(&p)
   if p.r != nil {
      tree.persist(&p.r)
      tree.aaSplit(&p.r)
   }
   return p
}

func (tree *AA) aaDeleteMin(p *Node, min **Node) *Node {
   tree.persist(&p)
   if p.l == nil {
      *min = p
      return p.r
   }
   p.s = p.s - 1
   p.l = tree.aaDeleteMin(p.l, min)
   return tree.fixAfterDelete(p)
}

func (tree *AA) aaDeleteMax(p *Node, max **Node) *Node {
   tree.persist(&p)
   if p.r == nil {
      *max = p
      return p.l
   }
   p.r = tree.aaDeleteMax(p.r, max)
   return tree.fixAfterDelete(p)
}

func (tree *AA) delete(p *Node, i Position, x *Data) *Node {
   tree.persist(&p)
   if i < p.s {
      p.s = p.s - 1
      p.l = tree.delete(p.l, i, x)
   } else if i > p.s {
      p.r = tree.delete(p.r, i - p.s - 1, x)
   } else {
      *x = p.x
      if p.l == nil && p.r == nil {
         tree.release(p)
         return nil
      }
      if p.l == nil {
         var min *Node
         p.r = tree.aaDeleteMin(p.r, &min)
         p.x = min.x
      } else {
         var max *Node
         p.l = tree.aaDeleteMax(p.l, &max)
         p.x = max.x
         p.s = p.s - 1
      }
   }
   return tree.fixAfterDelete(p)
}

func (tree *AA) Delete(i Position) (x Data) {
   assert(i < tree.size)
   tree.root = tree.delete(tree.root, i, &x)
   tree.size--
   return
}

func (tree *AA) build(l, p, r *Node, sl Size) *Node {
   rl := tree.rank(l)
   rr := tree.rank(r)
   if utility.Distance(rl, rr) <= 1 {
      p.l = l
      p.r = r
      p.s = sl
      p.y = uint64(max(rl, rr) + 1)
      return tree.fixAfterDelete(p)
   }
   if rl < rr {
      tree.persist(&r)
      r.s = 1 + sl + r.s
      r.l = tree.build(l, p, r.l, sl)
      return tree.fixAfterDelete(r)
   } else {
      tree.persist(&l)
      l.r = tree.build(l.r, p, r, sl - l.s - 1)
      return tree.fixAfterDelete(l)
   }
}

func (tree *AA) join(l, r *Node, sl, sr Size) (p *Node) {
   if l == nil { return r }
   if r == nil { return l }
   if tree.rank(l) < tree.rank(r) || (tree.rank(l) == tree.rank(r) && sl <= sr) {
      return tree.build(l, p, tree.aaDeleteMin(r, &p), sl)
   } else {
      return tree.build(tree.aaDeleteMax(l, &p), p, r, sl - 1)
   }
}

func (tree *AA) Join(other list.List) list.List {
   tree.share(tree.root)
   tree.share(other.(*AA).root)
   return &AA{
      Tree: Tree{
         pool: tree.pool,
         root: tree.join(tree.root, other.(*AA).root, tree.size, other.(*AA).size),
         size: tree.size + other.(*AA).size,
      },
   }
}

func (tree *AA) split(p *Node, i, s Size) (l, r *Node) {
   if p == nil {
      return
   }
   tree.persist(&p)

   sl := p.s
   sr := s - p.s - 1

   if i <= p.s {
      l, r = tree.split(p.l, i, sl)
         r = tree.build(r, p, p.r, sl - i)
   } else {
      l, r = tree.split(p.r, i - sl - 1, sr)
         l = tree.build(p.l, p, l, sl)
   }
   return l, r
}

func (tree *AA) Split(i Position) (list.List, list.List) {
   assert(i <= tree.size)
   tree.share(tree.root)
   l, r := tree.split(tree.root, i, tree.size)
   return &AA{Tree: Tree{pool: tree.pool, root: l, size: i}},
          &AA{Tree: Tree{pool: tree.pool, root: r, size: tree.size - i}}
}

func (tree AA) verifyRanks(p *Node) {
   if p == nil {
      return
   }
   if isLeaf(p) {
      invariant(tree.rank(p) == 0)
   }
   // All rank differences are positive (no left 0-children allowed).
   invariant(tree.rank(p) > tree.rank(p.l))

   // Right child may be a 0-child or 1-child.
   invariant(tree.isZeroChild(p, p.r) || tree.isOneChild(p, p.r))

   // No two consecutive right 0-children.
   if tree.isZeroChild(p, p.r) {
      invariant(!tree.isZeroChild(p.r, p.r.r))
   }

   tree.verifyRanks(p.l)
   tree.verifyRanks(p.r)
}

func (tree AA) Verify() {
   tree.verifySize(tree.root, tree.size)
   tree.verifyRanks(tree.root)
}
