package trees

import (
   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/utility/random"
)

type Randomized struct {
   Tree
   random.Source // compare performance vs making this directly xoshiro
}

func (Randomized) New() list.List {
   return &Randomized{
      Source: random.New(random.Uint64()),
   }
}

func (tree *Randomized) Clone() list.List {
   return &Randomized{
      Tree:   tree.Tree.Clone(),
      Source: tree.Source, // TODO: a copy method? or clone?
   }
}

func (tree *Randomized) Insert(i list.Position, x list.Data) {
   assert(i <= tree.size)
   tree.insert(&tree.root, tree.size, i, x)
   tree.size++
}

func (tree *Randomized) insert(p **Node, s list.Size, i list.Position, x list.Data) {
   for {
      if random.LessThan(s + 1, tree.Source) == s {
         l, r := tree.split(*p, i)
         *p = tree.allocate(Node{x: x, s: i, l: l, r: r})
         return
      }
      tree.persist(p)
      if i > (*p).s {
         s = s - (*p).s - 1
         i = i - (*p).s - 1
         p = &(*p).r
      } else {
         s = (*p).s
    (*p).s = (*p).s + 1
         p = &(*p).l
      }
   }
}

func (tree Randomized) delete(p **Node, s list.Size, i list.Position, x *list.Data) {
   for {
      if i == (*p).s {
         defer tree.release(*p)
         tree.share((*p).l)
         tree.share((*p).r)
         *x = (*p).x
         *p = tree.join((*p).l, (*p).r, (*p).s, s - (*p).s - 1)
         return
      }
      tree.persist(p)
      if i < (*p).s {
         s = (*p).s
    (*p).s = (*p).s - 1
         p = &(*p).l
      } else {
         s = s - (*p).s - 1
         i = i - (*p).s - 1
         p = &(*p).r
      }
   }
}

func (tree *Randomized) Delete(i list.Position) (x list.Data) {
   assert(i < tree.size)
   tree.delete(&tree.root, tree.size, i, &x)
   tree.size--
   return
}

func (tree *Randomized) join(l *Node, r *Node, sl, sr Size) (root *Node) {
   p := &root
   for {
      if l == nil { *p = r; return }
      if r == nil { *p = l; return }

      if random.LessThan(sl + sr, tree.Source) < sl {
          tree.persist(&l)
          sl = sl - l.s - 1
          *p = l
           p = &l.r
           l = *p
      } else {
          tree.persist(&r)
          sr = r.s
         r.s = r.s + sl
          *p = r
           p = &r.l
           r = *p
      }
   }
}

func (tree *Randomized) Select(i Size) Data {
   assert(i < tree.size)
   return tree.lookup(tree.root, i)
}

func (tree *Randomized) Update(i Size, x Data) {
   assert(i < tree.size)
   tree.persist(&tree.root)
   tree.update(tree.root, i, x)
}

func (tree *Randomized) Split(i list.Position) (list.List, list.List) {
   l, r := tree.Tree.Split(i)
   return &Randomized{l, tree.Source},
          &Randomized{r, tree.Source}
}

func (tree *Randomized) Join(that list.List) list.List { // TODO check if benchmarks are affected by poointer receivers here
   l := tree
   r := that.(*Randomized)
   tree.share(l.root)
   tree.share(r.root)
   return &Randomized{
      Tree{
         pool: tree.pool,
         root: l.join(l.root, r.root, l.size, r.size),
         size: l.size + r.size,
      },
      tree.Source,
   }
}

func (tree *Randomized) Verify() {
   tree.verifySizes()
}
