package trees

import (
   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/utility/random"
)

type TreapBottomUp struct {
   Tree
   random.Source
}

func (TreapBottomUp) New() list.List {
   return &TreapBottomUp{
      Source: random.New(random.Uint64()),
   }
}

func (tree *TreapBottomUp) Clone() list.List {
   return &TreapBottomUp{
      Tree:   tree.Tree.Clone(),
      Source: tree.Source,
   }
}

func (tree *TreapBottomUp) rank(p *Node) uint64 {
   if p == nil {
      return 0
   } else {
      return p.y
   }
}

func (tree *TreapBottomUp) insert(p *Node, i list.Position, x list.Data) *Node {
   if p == nil {
      return tree.allocate(Node{x: x, y: tree.Source.Uint64()})
   }
   tree.persist(&p)
   if i <= p.s {
      p.s = p.s + 1
      p.l = tree.insert(p.l, i, x)
      if p.l.y > p.y {
         tree.rotateR(&p)
      }
   } else {
      p.r = tree.insert(p.r, i - p.s - 1, x)
      if p.r.y > p.y {
         tree.rotateL(&p)
      }
   }
   return p
}

func (tree *TreapBottomUp) Insert(i list.Position, x list.Data) {
   assert(i <= tree.size)
   tree.root = tree.insert(tree.root, i, x)
   tree.size++
}

func (tree *TreapBottomUp) delete(p *Node, i list.Position, x *list.Data) *Node {
   if i == p.s {
      *x = p.x
      tree.share(p.l)
      tree.share(p.r)
      result := tree.join(p.l, p.r, p.s)
      tree.release(p)
      return result
   }
   tree.persist(&p)
   if i < p.s {
      p.s = p.s - 1
      p.l = tree.delete(p.l, i, x)
   } else {
      p.r = tree.delete(p.r, i - p.s - 1, x)
   }
   return p
}

func (tree *TreapBottomUp) Delete(i list.Position) (x list.Data) {
   assert(i < tree.size)
   tree.root = tree.delete(tree.root, i, &x)
   tree.size--
   return
}

func (tree *TreapBottomUp) join(l, r *Node, sl list.Size) (root *Node) {
   p := &root
   for {
      if l == nil { *p = r; return }
      if r == nil { *p = l; return }

      if tree.rank(l) >= tree.rank(r) {
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

func (tree *TreapBottomUp) Select(i list.Size) list.Data {
   assert(i < tree.size)
   return tree.lookup(tree.root, i)
}

func (tree *TreapBottomUp) Update(i list.Size, x list.Data) {
   assert(i < tree.size)
   tree.persist(&tree.root)
   tree.update(tree.root, i, x)
}

func (tree TreapBottomUp) Split(i list.Position) (list.List, list.List) {
   assert(i <= tree.size)
   l, r := tree.Tree.Split(i)
   return &TreapBottomUp{Tree: l, Source: tree.Source},
          &TreapBottomUp{Tree: r, Source: tree.Source}
}

func (tree TreapBottomUp) Join(that list.List) list.List {
   tree.share(tree.root)
   tree.share(that.(*TreapBottomUp).root)
   return &TreapBottomUp{
      Tree{
         pool: tree.pool,
         root: tree.join(tree.root, that.(*TreapBottomUp).root, tree.size),
         size: tree.size + that.(*TreapBottomUp).size,
      },
      tree.Source,
   }
}

func (tree TreapBottomUp) verifyMaxRankHeap(p *Node) {
   if p == nil {
      return
   }
   invariant(tree.rank(p) >= tree.rank(p.l))
   invariant(tree.rank(p) >= tree.rank(p.r))

   tree.verifyMaxRankHeap(p.l)
   tree.verifyMaxRankHeap(p.r)
}

func (tree TreapBottomUp) Verify() {
   tree.verifySizes()
   tree.verifyMaxRankHeap(tree.root)
}
