package trees

import (
   "github.com/rtheunissen/bst/types/list"
)

type AVLTopDown struct {
   AVLBottomUp
}

func (tree *AVLTopDown) New() list.List {
   return &AVLTopDown{}
}

func (tree *AVLTopDown) Clone() list.List {
   return &AVLTopDown{
      AVLBottomUp{
         Tree: tree.Tree.Clone(),
      },
   }
}

func (tree *AVLTopDown) Insert(i list.Position, x list.Data) {
   tree.insert(&tree.root, i, x)
   tree.size = tree.size + 1
}

func (tree *AVLTopDown) insert(p **Node, i list.Position, x list.Data) {
   if *p == nil {
      *p = tree.allocate(Node{x: x})
      return
   }
   tree.persist(p)
   a := p
   j := i
   for {
      if i <= (*p).sizeL() {
         (*p).s = (*p).sizeL() + 1
         p = &(*p).l
         if *p == nil {
            *p = tree.allocate(Node{x: x})
            break
         }
      } else {
         i = i - ((*p).sizeL() + 1)
         p = &(*p).r
         if *p == nil {
            *p = tree.allocate(Node{x: x})
            break
         }
      }
      tree.persist(p)
      if !tree.isOneOne(*p) {
         a = p
         j = i
      }
   }
   for q := *a; q != *p; {
      tree.promote(q)
      if j <= q.sizeL() {
         q = q.l
      } else {
         j = j - (q.sizeL() + 1)
         q = q.r
      }
   }
   *a = tree.balance(*a)
}

func (tree *AVLTopDown) balance(p *Node) *Node {
   if tree.isTwoTwo(p) {
      tree.demote(p)
      return p
   }
   if tree.isThreeChild(p, p.l) {
      if tree.isTwoChild(p.r, p.r.r) {
         tree.rotateRL(&p)
         tree.promote(p)
         tree.demote(p.r)
         tree.demote(p.l)
         tree.demote(p.l)
      } else {
         tree.rotateL(&p)
         tree.demote(p.l)
         tree.demote(p.l)
      }
   } else if tree.isThreeChild(p, p.r) {
      if tree.isTwoChild(p.l, p.l.l) {
         tree.rotateLR(&p)
         tree.promote(p)
         tree.demote(p.l)
         tree.demote(p.r)
         tree.demote(p.r)
      } else {
         tree.rotateR(&p)
         tree.demote(p.r)
         tree.demote(p.r)
      }
   }
   return p
}

func (tree *AVLTopDown) Join(other list.List) list.List {
   return &AVLTopDown{
      *tree.AVLBottomUp.Join(&other.(*AVLTopDown).AVLBottomUp).(*AVLBottomUp),
   }
}

func (tree *AVLTopDown) Split(i list.Position) (list.List, list.List) {
   l, r := tree.AVLBottomUp.Split(i)
   return &AVLTopDown{*l.(*AVLBottomUp)},
          &AVLTopDown{*r.(*AVLBottomUp)}
}
