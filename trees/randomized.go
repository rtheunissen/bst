package trees

import (
   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/utility/random"
)

type Randomized struct {
   Tree
   random.Xoshiro256
}

func (Randomized) New() list.List {
   return &Randomized{
      Xoshiro256: random.NewXoshiro256(random.Uint64()),
   }
}

func (tree *Randomized) Clone() list.List {
   return &Randomized{
      Tree:       tree.Tree.Clone(),
      Xoshiro256: tree.Xoshiro256,
   }
}

func (tree *Randomized) Insert(i list.Position, x list.Data) {
   assert(i <= tree.size)
   tree.insert(&tree.root, tree.size, i, x)
   tree.size++
}

func (tree *Randomized) insert(p **Node, s list.Size, i list.Position, x list.Data) {
   assert(tree.Xoshiro256 != random.Xoshiro256{}) // The generator must be seeded.
   for {
      if random.LessThan(s + 1, &tree.Xoshiro256) == s {
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

// The join draws random values, so this must not operate on a copy of the tree.
func (tree *Randomized) delete(p **Node, s list.Size, i list.Position, x *list.Data) {
   for {
      if i == (*p).s {
         defer tree.release(*p)
         tree.shareSubtrees(*p)
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
   assert(tree.Xoshiro256 != random.Xoshiro256{}) // The generator must be seeded.
   p := &root
   for {
      if l == nil { *p = r; return }
      if r == nil { *p = l; return }

      if random.LessThan(sl + sr, &tree.Xoshiro256) < sl {
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
   return &Randomized{l, tree.Xoshiro256},
          &Randomized{r, tree.Xoshiro256.Fork()}
}

func (tree *Randomized) Join(that list.List) list.List {
   l := tree
   r := that.(*Randomized)
   tree.share(l.root)
   tree.share(r.root)
   //
   // The join draws from the generator of the joined tree, not of this tree,
   // so that joining does not modify either of the trees being joined.
   //
   joined := &Randomized{
      Tree: Tree{
         pool: tree.pool,
         size: l.size + r.size,
      },
      Xoshiro256: tree.Xoshiro256.Fork(),
   }
   joined.root = joined.join(l.root, r.root, l.size, r.size)
   return joined
}

func (tree *Randomized) Verify() {
   tree.verifySizes()
}
