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

func (tree *AVLTopDown) Insert(i Position, x Data) {
   tree.insert(&tree.root, i, x)
   tree.size = tree.size + 1
}

func (tree *AVLTopDown) insert(p **Node, i Position, x Data) {
   if *p == nil {
      *p = tree.allocate(Node{x: x})
      return
   }
   tree.persist(p)
   a := p
   j := i
   for {
      if i <= sizeL(*p) {
         (*p).s = sizeL(*p) + 1
         p = &(*p).l
         if *p == nil {
            *p = tree.allocate(Node{x: x})
            break
         }
      } else {
         i = i - (sizeL(*p) + 1)
         p = &(*p).r
         if *p == nil {
            *p = tree.allocate(Node{x: x})
            break
         }
      }
      tree.persist(p)
      if !isOneOne(*p) {
         a = p
         j = i
      }
   }
   for q := *a; q != *p; {
      promote(q)
      if j <= sizeL(q) {
         q = q.l
      } else {
         j = j - (sizeL(q) + 1)
         q = q.r
      }
   }
   *a = tree.balance(*a)
}

func (tree *AVLTopDown) balance(p *Node) *Node {
   if isTwoTwo(p) {
      demote(p)
      return p
   }
   if isThreeChild(p, p.l) {
      if isTwoChild(p.r, p.r.r) {
         tree.treeRotateRL(&p)
         promote(p)
         demote(p.r)
         demote(p.l)
         demote(p.l)
      } else {
         tree.treeRotateL(&p)
         demote(p.l)
         demote(p.l)
      }
   } else if isThreeChild(p, p.r) {
      if isTwoChild(p.l, p.l.l) {
         tree.treeRotateLR(&p)
         promote(p)
         demote(p.l)
         demote(p.r)
         demote(p.r)
      } else {
         tree.treeRotateR(&p)
         demote(p.r)
         demote(p.r)
      }
   }
   return p
}

func (tree *AVLTopDown) Join(other list.List) list.List {
   return &AVLTopDown{
      *tree.AVLBottomUp.Join(&other.(*AVLTopDown).AVLBottomUp).(*AVLBottomUp),
   }
}

func (tree *AVLTopDown) Split(i Position) (list.List, list.List) {
   l, r := tree.AVLBottomUp.Split(i)
   return &AVLTopDown{*l.(*AVLBottomUp)},
          &AVLTopDown{*r.(*AVLBottomUp)}
}
