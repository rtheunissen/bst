package trees

import "github.com/rtheunissen/bst/types/list"

type RedBlackBottomUp struct {
   Tree
}

func (RedBlackBottomUp) New() list.List {
   return &RedBlackBottomUp{}
}

func (tree *RedBlackBottomUp) Clone() list.List {
   return &RedBlackBottomUp{
      Tree: tree.Tree.Clone(),
   }
}

func (tree RedBlackBottomUp) Verify() {
   tree.verifySize(tree.root, tree.size)
   verifyRedBlackRanks(tree.root)
   verifyRedBlackHeight(tree.root, tree.size)
}

func (tree *RedBlackBottomUp) Delete(i Position) (x Data) {
   assert(i < tree.size)
   tree.size = tree.size - 1
   tree.root = tree.delete(tree.root, tree.size + 1, i, &x)
   return x
}

func (tree *RedBlackBottomUp) delete(p *Node, s Size, i Position, x *Data) *Node {
   sl := sizeL(p)
   sr := sizeR(p, s)

   if i == sl {
      tree.share(p.l)
      tree.share(p.r)
      *x = p.x
      result := tree.join(p.l, p.r, sl, sr)
      tree.release(p)
      return result
   }
   tree.persist(&p)
   if i < sl {
      p.s = sl - 1
      p.l = tree.delete(p.l, sl, i, x)
      return tree.balanceDeleteL(p)
   } else {
      p.r = tree.delete(p.r, sr, i-p.s-1, x)
      return tree.balanceDeleteR(p)
   }
}

func (tree RedBlackBottomUp) balanceDeleteL(p *Node) *Node {
   if isZeroChild(p, p.l) {
      assert(isOneChild(p.l, p.l.l))
      assert(isOneChild(p.l, p.l.r))
      return p
   }
   if isOneChild(p, p.l) {
      return p
   }
   if isZeroChild(p, p.r) {
      assert(isZeroChild(p, p.r))
      assert(isTwoChild(p, p.l))
      assert(isOneOne(p.r))
      tree.treeRotateL(&p)
      assert(isZeroChild(p, p.l))
      assert(isOneChild(p, p.r))
      if isZeroChild(p.l.r, p.l.r.r) {
         assert(isOneChild(p.l, p.l.r))
         assert(isTwoChild(p.l, p.l.l))
         tree.treeRotateL(&p.l)
         promote(p.l)
         demote(p.l.l)
         return p
      }
      if isZeroChild(p.l.r, p.l.r.l) {
         assert(isOneChild(p.l, p.l.r))
         assert(isTwoChild(p.l, p.l.l))
         tree.treeRotateRL(&p.l)
         promote(p.l)
         demote(p.l.l)
         return p
      }
      assert(isOneChild(p.l, p.l.r))
      assert(isTwoChild(p.l, p.l.l))
      assert(isOneChild(p.l.r, p.l.r.r))
      assert(isOneChild(p.l.r, p.l.r.l))
      demote(p.l)
      return p
   } else {
      assert(isOneChild(p, p.r))
      assert(isTwoChild(p, p.l))
      if isZeroChild(p.r, p.r.r) {
         tree.treeRotateL(&p)
         promote(p)
         demote(p.l)
         return p
      }
      if isZeroChild(p.r, p.r.l) {
         tree.treeRotateRL(&p)
         promote(p)
         demote(p.l)
         return p
      }
      assert(isOneOne(p.r))
      demote(p)
      return p
   }
}

func (tree RedBlackBottomUp) balanceDeleteR(p *Node) *Node {
   if isZeroChild(p, p.r) {
      assert(isOneChild(p.r, p.r.r))
      assert(isOneChild(p.r, p.r.l))
      return p
   }
   if isOneChild(p, p.r) {
      return p
   }
   if isZeroChild(p, p.l) {
      assert(isZeroChild(p, p.l))
      assert(isTwoChild(p, p.r))
      assert(isOneOne(p.l))
      tree.treeRotateR(&p)
      assert(isZeroChild(p, p.r))
      assert(isOneChild(p, p.l))
      if isZeroChild(p.r.l, p.r.l.l) {
         assert(isOneChild(p.r, p.r.l))
         assert(isTwoChild(p.r, p.r.r))
         tree.treeRotateR(&p.r)
         promote(p.r)
         demote(p.r.r)
         return p
      }
      if isZeroChild(p.r.l, p.r.l.r) {
         assert(isOneChild(p.r, p.r.l))
         assert(isTwoChild(p.r, p.r.r))
         tree.treeRotateLR(&p.r)
         promote(p.r)
         demote(p.r.r)
         return p
      }
      assert(isOneChild(p.r, p.r.l))
      assert(isTwoChild(p.r, p.r.r))
      assert(isOneChild(p.r.l, p.r.l.l))
      assert(isOneChild(p.r.l, p.r.l.r))
      demote(p.r)
      return p
   } else {
      assert(isOneChild(p, p.l))
      assert(isTwoChild(p, p.r))
      if isZeroChild(p.l, p.l.l) {
         tree.treeRotateR(&p)
         promote(p)
         demote(p.r)
         return p
      }
      if isZeroChild(p.l, p.l.r) {
         tree.treeRotateLR(&p)
         promote(p)
         demote(p.r)
         return p
      }
      assert(isOneOne(p.l))
      demote(p)
      return p
   }
}

func (tree *RedBlackBottomUp) Insert(i Position, x Data) {
   assert(i <= tree.size)
   tree.size = tree.size + 1
   tree.root = tree.insert(tree.root, i, x)
   return
}

func (tree *RedBlackBottomUp) insert(p *Node, i Position, x Data) *Node {
   if p == nil {
      return tree.allocate(Node{x: x})
   }
   tree.persist(&p)
   if i <= p.s {
      p.s = p.s + 1
      p.l = tree.insert(p.l, i, x)
      return tree.balanceInsertL(p)
   } else {
      p.r = tree.insert(p.r, i-p.s-1, x)
      return tree.balanceInsertR(p)
   }
}

func (tree RedBlackBottomUp) balanceInsertL(p *Node) *Node {
   if isZeroChild(p, p.l) {
      if isZeroChild(p, p.r) {
         if isZeroChild(p.l, p.l.l) || isZeroChild(p.l, p.l.r) {
            promote(p)
            return p
         }
      } else {
         if isZeroChild(p.l, p.l.l) {
            tree.treeRotateR(&p)
            return p
         }
         if isZeroChild(p.l, p.l.r) {
            tree.treeRotateLR(&p)
            return p
         }
      }
   }
   return p
}

func (tree RedBlackBottomUp) balanceInsertR(p *Node) *Node {
   if isZeroChild(p, p.r) {
      if isZeroChild(p, p.l) {
         if isZeroChild(p.r, p.r.r) || isZeroChild(p.r, p.r.l) {
            promote(p)
            return p
         }
      } else {
         if isZeroChild(p.r, p.r.r) {
            tree.treeRotateL(&p)
            return p
         }
         if isZeroChild(p.r, p.r.l) {
            tree.treeRotateRL(&p)
            return p
         }
      }
   }
   return p
}


func (tree *RedBlackBottomUp) deleteMin(p *Node, min **Node) *Node {
   tree.persist(&p)
   if p.l == nil {
      *min = p
      return p.r
   }
   p.s = p.s - 1
   p.l = tree.deleteMin(p.l, min)
   return tree.balanceDeleteL(p)
}


func (tree *RedBlackBottomUp) deleteMax(p *Node, max **Node) *Node {
   tree.persist(&p)
   if p.r == nil {
      *max = p
      return p.l
   }
   p.r = tree.deleteMax(p.r, max)
   return tree.balanceDeleteR(p)
}

func (tree *RedBlackBottomUp) build(l, p, r *Node, sl Size) *Node {
  if rank(l) == rank(r) {
     p.l = l
     p.r = r
     p.s = sl
     p.y = uint64(rank(l))
     if r == nil || hasZeroChild(l) || hasZeroChild(r) {
        promote(p)
     }
     return p
  }
  if rank(l) < rank(r) {
     tree.persist(&r)
     r.s = 1 + sl + r.s
     r.l = tree.build(l, p, r.l, sl)
     return tree.balanceInsertL(r)
  } else {
     tree.persist(&l)
     l.r = tree.build(l.r, p, r, sl-l.s-1)
     return tree.balanceInsertR(l)
  }
}

func (tree *RedBlackBottomUp) join(l, r *Node, sl, sr Size) (p *Node) {
  if l == nil { return r }
  if r == nil { return l }
  if rank(l) < rank(r) || (rank(l) == rank(r) && sl <= sr) {
     return tree.build(l, p, tree.deleteMin(r, &p), sl)
  } else {
     return tree.build(tree.deleteMax(l, &p), p, r, sl-1)
  }
}

func (tree *RedBlackBottomUp) Join(other list.List) list.List {
  return &RedBlackBottomUp{
     Tree: tree.Tree.joinWith(other.(*RedBlackBottomUp).Tree, tree.join),
  }
}

func (tree RedBlackBottomUp) split(p *Node, s, i Size) (l, r *Node) {
   if p == nil {
      return
   }
   tree.persist(&p)
   if i <= sizeL(p) {
      l, r = tree.split(p.l, sizeL(p), i)
      r, l = tree.build(r, p, p.r, sizeL(p)-i), l
   } else {
      l, r = tree.split(p.r, sizeR(p, s), i-sizeL(p)-1)
      l, r = tree.build(p.l, p, l, sizeL(p)), r
   }
   return l, r
}

func (tree *RedBlackBottomUp) Split(i Position) (list.List, list.List) {
   assert(i <= tree.size)
   tree.share(tree.root)
   l, r := tree.split(tree.root, tree.size, i)
   return &RedBlackBottomUp{Tree: Tree{pool: tree.pool, root: l, size: i}},
          &RedBlackBottomUp{Tree: Tree{pool: tree.pool, root: r, size: tree.size - i}}
}
