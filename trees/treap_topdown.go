package trees

import (
   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/utility/random"
)

type TreapTopDown struct {
   Tree
   random.Source
}

func (TreapTopDown) New() list.List {
   return &TreapTopDown{
      Source: random.New(random.Uint64()),
   }
}

func (tree *TreapTopDown) Clone() list.List {
   return &TreapTopDown{
      Tree:   tree.Tree.Clone(),
      Source: tree.Source,
   }
}

//       l* ↘                                                  ↙ r*
//
//             (a)                                       (z)
//           ↙  9  ↘                                   ↙  7  ↘
//         ○         (b)                           (y)         ○
//                 ↙  8  ↘                       ↙  6  ↘
//               ○         (c)               (x)         ○
//                       ↙  5  ↘              3
//                     ○         (d)
//                             ↙  4
//                           ○
//
//
//
//                               (a)
//                             ↙     ↘
//                           ○         (b)
//                                   ↙     ↘
//                                 ○         (z)
//                                         ↙     ↘
//                                     (y)         ○
//                                   ↙     ↘
//                               (c)         ○
//                             ↙     ↘
//                           ○         (d)
//                                   ↙     ↘
//                                 ○         (x)
//
//
func (tree *TreapTopDown) join(l, r *Node, sl list.Size) (root *Node) {
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

func (tree *TreapTopDown) rank(p *Node) uint64 {
   if p == nil {
      return 0
   } else {
      return p.y
   }
}
func (tree *TreapTopDown) build(l, p, r *Node, sl, sr list.Size) (root *Node) {
   if tree.rank(p) >= tree.rank(l) && tree.rank(p) >= tree.rank(r) {
      p.l = l
      p.r = r
      p.s = sl
      return p
   }
   if tree.rank(l) > tree.rank(r) {
      l.r = tree.build(l.r, p, r, sl-l.s-1, sr)
      return l
   } else {
      r.l = tree.build(l, p, r.l, sl, r.s)
      r.s = r.s + sl + 1
      return r
   }
}

func (tree TreapTopDown) delete(p **Node, i list.Position, x *list.Data) {
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
    (*p).s = (*p).s - 1
         p = &(*p).l
      } else {
         i = i - (*p).s - 1
         p = &(*p).r
      }
   }
}

func (tree *TreapTopDown) insert(p **Node, i list.Position, x list.Data) {
   y := tree.Source.Uint64()
   for {
      if tree.rank(*p) <= y {
         l, r := tree.split(*p, i)
         *p = tree.allocate(Node{x: x, y: y, s: i, l: l, r: r})
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

func (tree *TreapTopDown) Insert(i list.Position, x list.Data) {
   assert(i <= tree.size)
   tree.insert(&tree.root, i, x)
   tree.size++
}

func (tree *TreapTopDown) Delete(i list.Position) (x list.Data) {
   assert(i < tree.size)
   tree.delete(&tree.root, i, &x)
   tree.size--
   return
}

func (tree TreapTopDown) Split(i list.Position) (list.List, list.List) {
   assert(i <= tree.size)
   l, r := tree.Tree.Split(i)
   return &TreapTopDown{Tree: l, Source: tree.Source},
          &TreapTopDown{Tree: r, Source: tree.Source}
}

func (tree *TreapTopDown) Select(i list.Size) list.Data {
   assert(i < tree.size)
   return tree.lookup(tree.root, i)
}

func (tree *TreapTopDown) Update(i list.Size, x list.Data) {
   assert(i < tree.size)
   tree.persist(&tree.root)
   tree.update(tree.root, i, x)
}

func (tree TreapTopDown) Join(that list.List) list.List {
   tree.share(tree.root)
   tree.share(that.(*TreapTopDown).root)
   return &TreapTopDown{
      Tree{
         pool: tree.pool,
         root: tree.join(tree.root, that.(*TreapTopDown).root, tree.size),
         size: tree.size + that.(*TreapTopDown).size,
      },
      tree.Source,
   }
}

func (tree TreapTopDown) verifyMaxRankHeap(p *Node) {
   if p == nil {
      return
   }
   invariant(tree.rank(p) >= tree.rank(p.l))
   invariant(tree.rank(p) >= tree.rank(p.r))

   tree.verifyMaxRankHeap(p.l)
   tree.verifyMaxRankHeap(p.r)
}

func (tree TreapTopDown) Verify() {
   tree.verifySizes()
   tree.verifyMaxRankHeap(tree.root)
}
