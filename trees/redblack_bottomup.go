package trees

import "github.com/rtheunissen/bst/types/list"

type RedBlackBottomUp struct {
   Tree
   RedBlack
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
   tree.verifyRanks(tree.root)
   tree.verifyHeight(tree.root, tree.size)
}

func (tree *RedBlackBottomUp) Delete(i list.Position) (x list.Data) {
   assert(i < tree.size)
   tree.size = tree.size - 1
   tree.root = tree.delete(tree.root, i, &x)
   return x
}

func (tree *RedBlackBottomUp) delete(p *Node, i list.Position, x *list.Data) *Node {
   if i == p.sizeL() {
      defer tree.release(p)
      tree.share(p.l)
      tree.share(p.r)
      *x = p.x
      return tree.join(p.l, p.r, p.s)
   }
   tree.persist(&p)
   if i < p.sizeL() {
      p.s = p.sizeL() - 1
      p.l = tree.delete(p.l, i, x)
      return tree.balanceDeleteL(p)
   } else {
      p.r = tree.delete(p.r, i-p.s-1, x)
      return tree.balanceDeleteR(p)
   }
}

func (tree RedBlackBottomUp) balanceDeleteL(p *Node) *Node {
   if tree.isZeroChild(p, p.l) {
      assert(tree.isOneChild(p.l, p.l.l))
      assert(tree.isOneChild(p.l, p.l.r))
      return p
   }
   if tree.isOneChild(p, p.l) {
      return p
   }
   if tree.isZeroChild(p, p.r) {
      assert(tree.isZeroChild(p, p.r))
      assert(tree.isTwoChild(p, p.l))
      assert(tree.isOneOne(p.r))
      tree.rotateL(&p)
      assert(tree.isZeroChild(p, p.l))
      assert(tree.isOneChild(p, p.r))
      if tree.isZeroChild(p.l.r, p.l.r.r) {
         assert(tree.isOneChild(p.l, p.l.r))
         assert(tree.isTwoChild(p.l, p.l.l))
         tree.rotateL(&p.l)
         tree.promote(p.l)
         tree.demote(p.l.l)
         return p
      }
      if tree.isZeroChild(p.l.r, p.l.r.l) {
         assert(tree.isOneChild(p.l, p.l.r))
         assert(tree.isTwoChild(p.l, p.l.l))
         tree.rotateRL(&p.l)
         tree.promote(p.l)
         tree.demote(p.l.l)
         return p
      }
      assert(tree.isOneChild(p.l, p.l.r))
      assert(tree.isTwoChild(p.l, p.l.l))
      assert(tree.isOneChild(p.l.r, p.l.r.r))
      assert(tree.isOneChild(p.l.r, p.l.r.l))
      tree.demote(p.l)
      return p
   } else {
      assert(tree.isOneChild(p, p.r))
      assert(tree.isTwoChild(p, p.l))
      if tree.isZeroChild(p.r, p.r.r) {
         tree.rotateL(&p)
         tree.promote(p)
         tree.demote(p.l)
         return p
      }
      if tree.isZeroChild(p.r, p.r.l) {
         tree.rotateRL(&p)
         tree.promote(p)
         tree.demote(p.l)
         return p
      }
      assert(tree.isOneOne(p.r))
      tree.demote(p)
      return p
   }
}

func (tree RedBlackBottomUp) balanceDeleteR(p *Node) *Node {
   if tree.isZeroChild(p, p.r) {
      assert(tree.isOneChild(p.r, p.r.r))
      assert(tree.isOneChild(p.r, p.r.l))
      return p
   }
   if tree.isOneChild(p, p.r) {
      return p
   }
   if tree.isZeroChild(p, p.l) {
      assert(tree.isZeroChild(p, p.l))
      assert(tree.isTwoChild(p, p.r))
      assert(tree.isOneOne(p.l))
      tree.rotateR(&p)
      assert(tree.isZeroChild(p, p.r))
      assert(tree.isOneChild(p, p.l))
      if tree.isZeroChild(p.r.l, p.r.l.l) {
         assert(tree.isOneChild(p.r, p.r.l))
         assert(tree.isTwoChild(p.r, p.r.r))
         tree.rotateR(&p.r)
         tree.promote(p.r)
         tree.demote(p.r.r)
         return p
      }
      if tree.isZeroChild(p.r.l, p.r.l.r) {
         assert(tree.isOneChild(p.r, p.r.l))
         assert(tree.isTwoChild(p.r, p.r.r))
         tree.rotateLR(&p.r)
         tree.promote(p.r)
         tree.demote(p.r.r)
         return p
      }
      assert(tree.isOneChild(p.r, p.r.l))
      assert(tree.isTwoChild(p.r, p.r.r))
      assert(tree.isOneChild(p.r.l, p.r.l.l))
      assert(tree.isOneChild(p.r.l, p.r.l.r))
      tree.demote(p.r)
      return p
   } else {
      assert(tree.isOneChild(p, p.l))
      assert(tree.isTwoChild(p, p.r))
      if tree.isZeroChild(p.l, p.l.l) {
         tree.rotateR(&p)
         tree.promote(p)
         tree.demote(p.r)
         return p
      }
      if tree.isZeroChild(p.l, p.l.r) {
         tree.rotateLR(&p)
         tree.promote(p)
         tree.demote(p.r)
         return p
      }
      assert(tree.isOneOne(p.l))
      tree.demote(p)
      return p
   }
}

func (tree *RedBlackBottomUp) Insert(i list.Position, x list.Data) {
   assert(i <= tree.size)
   tree.size = tree.size + 1
   tree.root = tree.insert(tree.root, i, x)
   return
}

func (tree *RedBlackBottomUp) insert(p *Node, i list.Position, x list.Data) *Node {
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
   if tree.isZeroChild(p, p.l) {
      if tree.isZeroChild(p, p.r) {
         if tree.isZeroChild(p.l, p.l.l) || tree.isZeroChild(p.l, p.l.r) {
            tree.promote(p)
            return p
         }
      } else {
         if tree.isZeroChild(p.l, p.l.l) {
            tree.rotateR(&p)
            return p
         }
         if tree.isZeroChild(p.l, p.l.r) {
            tree.rotateLR(&p)
            return p
         }
      }
   }
   return p
}

func (tree RedBlackBottomUp) balanceInsertR(p *Node) *Node {
   if tree.isZeroChild(p, p.r) {
      if tree.isZeroChild(p, p.l) {
         if tree.isZeroChild(p.r, p.r.r) || tree.isZeroChild(p.r, p.r.l) {
            tree.promote(p)
            return p
         }
      } else {
         if tree.isZeroChild(p.r, p.r.r) {
            tree.rotateL(&p)
            return p
         }
         if tree.isZeroChild(p.r, p.r.l) {
            tree.rotateRL(&p)
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

func (tree *RedBlackBottomUp) build(l, p, r *Node, sl list.Size) *Node {
  if tree.rank(l) == tree.rank(r) {
     p.l = l
     p.r = r
     p.s = sl
     p.y = uint64(tree.rank(l))
     if r == nil || tree.hasZeroChild(l) || tree.hasZeroChild(r) {
        tree.promote(p)
     }
     return p
  }
  if tree.rank(l) < tree.rank(r) {
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

func (tree *RedBlackBottomUp) join(l, r *Node, sl list.Size) (p *Node) {
  if l == nil { return r }
  if r == nil { return l }
  if tree.rank(l) < tree.rank(r) {
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

func (tree RedBlackBottomUp) split(p *Node, s, i list.Size) (l, r *Node) {
   if p == nil {
      return
   }
   tree.persist(&p)
   if i <= p.sizeL() {
      l, r = tree.split(p.l, p.sizeL(), i)
      r, l = tree.build(r, p, p.r, p.sizeL()-i), l
   } else {
      l, r = tree.split(p.r, p.sizeR(s), i-p.sizeL()-1)
      l, r = tree.build(p.l, p, l, p.sizeL()), r
   }
   return l, r
}

func (tree *RedBlackBottomUp) Split(i list.Position) (list.List, list.List) {
   assert(i <= tree.size)
   tree.share(tree.root)
   l, r := tree.split(tree.root, tree.size, i)
   return &RedBlackBottomUp{Tree: Tree{pool: tree.pool, root: l, size: i}},
          &RedBlackBottomUp{Tree: Tree{pool: tree.pool, root: r, size: tree.size - i}}
}
