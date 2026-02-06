package trees

import (
   "github.com/rtheunissen/bst/types/list"
)

// LLRB
//
// A left-leaning red-black tree, based on Sedgewick's simplification of
// red-black trees. The key constraint is that red links (0-children in the
// rank-balanced framework) may only lean left. This reduces the number of
// cases in the balancing logic.
//
// Using the rank-balanced framework:
//
//    Red link   = 0-child (rank difference 0)
//    Black link = 1-child (rank difference 1)
//
//    Left-leaning: a node may only have a right 0-child if it also has
//                  a left 0-child (i.e., no right-leaning red links).
//
type LLRB struct {
   Tree
}

func (LLRB) New() list.List {
   return &LLRB{}
}

func (tree *LLRB) Clone() list.List {
   return &LLRB{
      Tree: tree.Tree.Clone(),
   }
}

func (tree LLRB) fixUp(p *Node) *Node {
   //
   // Right-leaning red link: rotate left to make it left-leaning.
   //
   if isRed(p, p.r) && !isRed(p, p.l) {
      tree.treeRotateL(&p)
   }
   //
   // Two consecutive left red links: rotate right.
   //
   if isRed(p, p.l) && isRed(p.l, p.l.l) {
      tree.treeRotateR(&p)
   }
   //
   // Both children red: promote (color flip).
   //
   if isRed(p, p.l) && isRed(p, p.r) {
      promote(p)
   }
   return p
}

func (tree *LLRB) insert(p *Node, i Position, x Data) *Node {
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
   return tree.fixUp(p)
}

func (tree *LLRB) Insert(i Position, x Data) {
   assert(i <= tree.size)
   tree.root = tree.insert(tree.root, i, x)
   tree.size++
}

// moveRedLeft makes p.l or one of its children red, assuming that p is red
// and both p.l and p.l.l are black.
func (tree LLRB) moveRedLeft(p *Node) *Node {
   demote(p)
   if p.r != nil && isRed(p.r, p.r.l) {
      tree.persist(&p.r)
      tree.treeRotateR(&p.r)
      tree.treeRotateL(&p)
      promote(p)
   }
   return p
}

// moveRedRight makes p.r or one of its children red, assuming that p is red
// and both p.r and p.r.l are black.
func (tree LLRB) moveRedRight(p *Node) *Node {
   demote(p)
   if p.l != nil && isRed(p.l, p.l.l) {
      tree.treeRotateR(&p)
      promote(p)
   }
   return p
}

func (tree *LLRB) deleteMin(p *Node, min **Node) *Node {
   tree.persist(&p)
   if p.l == nil {
      *min = p
      return nil
   }
   if !isRed(p, p.l) && !isRed(p.l, p.l.l) {
      p = tree.moveRedLeft(p)
   }
   p.s = p.s - 1
   p.l = tree.deleteMin(p.l, min)
   return tree.fixUp(p)
}

func (tree *LLRB) delete(p *Node, i Position, x *Data) *Node {
   tree.persist(&p)
   if i < p.s {
      if !isRed(p, p.l) && (p.l == nil || !isRed(p.l, p.l.l)) {
         p = tree.moveRedLeft(p)
      }
      p.s = p.s - 1
      p.l = tree.delete(p.l, i, x)
   } else {
      if isRed(p, p.l) {
         tree.treeRotateR(&p)
      }
      if i == p.s && p.r == nil {
         *x = p.x
         tree.release(p)
         return nil
      }
      if !isRed(p, p.r) && (p.r == nil || !isRed(p.r, p.r.l)) {
         p = tree.moveRedRight(p)
      }
      if i == p.s {
         *x = p.x
         var min *Node
         p.r = tree.deleteMin(p.r, &min)
         p.x = min.x
      } else {
         p.r = tree.delete(p.r, i - p.s - 1, x)
      }
   }
   return tree.fixUp(p)
}

func (tree *LLRB) Delete(i Position) (x Data) {
   assert(i < tree.size)
   tree.root = tree.delete(tree.root, i, &x)
   tree.size--
   return
}

func (tree LLRB) balanceInsertL(p *Node) *Node {
   if isRed(p, p.l) {
      if isRed(p, p.r) {
         if isRed(p.l, p.l.l) || isRed(p.l, p.l.r) {
            promote(p)
         }
      } else {
         if isRed(p.l, p.l.l) {
            tree.treeRotateR(&p)
         } else if isRed(p.l, p.l.r) {
            tree.treeRotateLR(&p)
         }
      }
   }
   return tree.fixUp(p)
}

func (tree LLRB) balanceInsertR(p *Node) *Node {
   if isRed(p, p.r) {
      if isRed(p, p.l) {
         if isRed(p.r, p.r.r) || isRed(p.r, p.r.l) {
            promote(p)
         }
      } else {
         if isRed(p.r, p.r.r) {
            tree.treeRotateL(&p)
         } else if isRed(p.r, p.r.l) {
            tree.treeRotateRL(&p)
         }
      }
   }
   return tree.fixUp(p)
}

func (tree *LLRB) build(l, p, r *Node, sl Size) *Node {
   if rank(l) == rank(r) {
      p.l = l
      p.r = r
      p.s = sl
      p.y = uint64(rank(l))
      if r == nil || hasZeroChild(l) || hasZeroChild(r) {
         promote(p)
      }
      return tree.fixUp(p)
   }
   if rank(l) < rank(r) {
      tree.persist(&r)
      r.s = 1 + sl + r.s
      r.l = tree.build(l, p, r.l, sl)
      return tree.balanceInsertL(r)
   } else {
      tree.persist(&l)
      l.r = tree.build(l.r, p, r, sl - l.s - 1)
      return tree.balanceInsertR(l)
   }
}

func (tree *LLRB) deleteMax(p *Node, max **Node) *Node {
   tree.persist(&p)
   if isRed(p, p.l) {
      tree.treeRotateR(&p)
   }
   if p.r == nil {
      *max = p
      return p.l
   }
   if !isRed(p, p.r) && (p.r.l == nil || !isRed(p.r, p.r.l)) {
      p = tree.moveRedRight(p)
   }
   p.r = tree.deleteMax(p.r, max)
   return tree.fixUp(p)
}

func (tree *LLRB) join(l, r *Node, sl, sr Size) (p *Node) {
   if l == nil { return r }
   if r == nil { return l }
   if rank(l) < rank(r) || (rank(l) == rank(r) && sl <= sr) {
      return tree.build(l, p, tree.deleteMin(r, &p), sl)
   } else {
      return tree.build(tree.deleteMax(l, &p), p, r, sl - 1)
   }
}

func (tree *LLRB) Join(other list.List) list.List {
   tree.share(tree.root)
   tree.share(other.(*LLRB).root)
   return &LLRB{
      Tree: Tree{
         pool: tree.pool,
         root: tree.join(tree.root, other.(*LLRB).root, tree.size, other.(*LLRB).size),
         size: tree.size + other.(*LLRB).size,
      },
   }
}

func (tree *LLRB) split(p *Node, i, s Size) (l, r *Node) {
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

func (tree *LLRB) Split(i Position) (list.List, list.List) {
   assert(i <= tree.size)
   tree.share(tree.root)
   l, r := tree.split(tree.root, i, tree.size)
   return &LLRB{Tree: Tree{pool: tree.pool, root: l, size: i}},
          &LLRB{Tree: Tree{pool: tree.pool, root: r, size: tree.size - i}}
}

func (tree LLRB) verifyLeftLeaning(p *Node) {
   if p == nil {
      return
   }
   // No right-leaning red links: if right is red, left must also be red.
   invariant(!isRed(p, p.r) || isRed(p, p.l))

   tree.verifyLeftLeaning(p.l)
   tree.verifyLeftLeaning(p.r)
}

func (tree LLRB) Verify() {
   tree.verifySize(tree.root, tree.size)
   verifyRedBlackRanks(tree.root)
   tree.verifyLeftLeaning(tree.root)
   verifyRedBlackHeight(tree.root, tree.size)
}
