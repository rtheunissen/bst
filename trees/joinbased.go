package trees

import (
   "github.com/rtheunissen/bst/types/list"
)

type Joiner interface {
   build(l, p, r *Node, sl, sr Size) *Node
   join(l, r *Node, sl, sr Size) *Node
}

type JoinBased struct {
   Tree
   Joiner
}

func (tree JoinBased) insert(p *Node, i list.Position, s Size, x *Node) *Node {
   if p == nil {
      return x
   }
   tree.persist(&p)

   sl := p.s
   sr := s - p.s - 1

   if i <= p.s {
      p.s++
      return tree.build(tree.insert(p.l, i, sl, x), p, p.r, sl+1, sr)
   } else {
      return tree.build(p.l, p, tree.insert(p.r, i-sl-1, sr, x), sl, sr+1)
   }
}

func (tree JoinBased) delete(p *Node, i list.Position, s Size, x *Data) *Node {
   sl := p.s
   sr := s - p.s - 1
   if i == p.s {
      *x = p.x
      defer tree.release(p)
      tree.share(p.l)
      tree.share(p.r)
      return tree.Tree.join(p.l, p.r, sl, sr)
   }
   tree.persist(&p)
   if i < p.s {
      p.s--
      return tree.build(tree.delete(p.l, i, sl, x), p, p.r, sl-1, sr)
   } else {
      return tree.build(p.l, p, tree.delete(p.r, i-sl-1, sr, x), sl, sr-1)
   }
}

func (tree JoinBased) split(p *Node, i, s Size) (l, r *Node) {
   if p == nil {
      return
   }
   tree.persist(&p)
   
   sl := p.s
   sr := s - p.s - 1
   
   if i <= (*p).s {
      l, r = tree.split(p.l, i, sl)
         r = tree.build(r, p, p.r, sl-i, sr)
   } else {
      l, r = tree.split(p.r, i-sl-1, sr)
         l = tree.build(p.l, p, l, sl, i-sl-1)
   }
   return l, r
}
