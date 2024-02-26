package trees

import (
   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/utility"
)

// Conc
//
// https://www.youtube.com/watch?v=o0NR9GrcHQo
//
// https://api.semanticscholar.org/CorpusID:21267485
// http://aleksandar-prokopec.com/publications/conc-trees/
//
// https://github.com/axel22/conc-trees/blob/master/src/main/scala/scala/reactive/core/Conc.scala
//
// Prokopec, A., & Odersky, M. (2015). Conc-Trees for Functional and Parallel Programming. LCPC.
//
type Conc struct {
   Tree
}

func (tree Conc) Count(p *Node) list.Size {
   if p.isLeaf() {
      return 1
   } else {
      return tree.Count(p.l) + tree.Count(p.r)
   }
}

func (Conc) New() list.List {
   return &Conc{}
}

func (tree *Conc) Clone() list.List {
   return &Conc{
      Tree: tree.Tree.Clone(),
   }
}

// Creates a leaf node containing the data `s`.
func (tree *Conc) asLeaf(x list.Data) *Node {
   return tree.allocate(Node{
      x: x,
      s: 1,
   })
}

// This is the node constructor from the paper.
//
// Creates a <> node with subtrees `l` and `r`, caching the size and height of.
// Note that we store the height in the unused data field `s`.
func (tree *Conc) link(l, r *Node) *Node {
   return tree.allocate(Node{
      y: max(l.y, r.y) + 1,
      s: l.s + r.s,
      l: l,
      r: r,
   })
}

// This is the <> (conc) function from the paper.
//
// Creates a reduced and balanced concatenation of two subtrees.
func (tree *Conc) balanced(l, r *Node) *Node {
   if l == nil {
      return r
   }
   if r == nil {
      return l
   }
   return tree.concat(l, r)
}

func (tree *Conc) concat(l, r *Node) *Node {
   if l.y > r.y && ((l.y - r.y) > 1) {
      if l.l.y >= l.r.y {
         return tree.link(l.l, tree.concat(l.r, r))
      }
      rr := tree.concat(l.r.r, r)
      if l.y-rr.y < 3 {
         return tree.link(tree.link(l.l, l.r.l), rr)
      } else {
         return tree.link(l.l, tree.link(l.r.l, rr))
      }
   }
   if r.y > l.y && ((r.y - l.y) > 1) {
      if r.r.y >= r.l.y {
         return tree.link(tree.concat(l, r.l), r.r)
      }
      ll := tree.concat(l, r.l.l)
      if r.y-ll.y < 3 {
         return tree.link(ll, tree.link(r.l.r, r.r))
      } else {
         return tree.link(tree.link(ll, r.l.r), r.r)
      }
   }
   return tree.link(l, r)
}

func (tree Conc) Select(i list.Position) (x list.Data) {
   assert(i < tree.size)
   return tree.lookup(tree.root, i).x
}

func (tree Conc) lookup(p *Node, i list.Position) *Node {
   if p.isLeaf() {
      assert(i == 0)
      return p
   }
   if i < p.l.s {
      return tree.lookup(p.l, i)
   } else {
      return tree.lookup(p.r, i-p.l.s)
   }
}

func (tree *Conc) Update(i list.Position, x list.Data) {
   assert(i < tree.size)
   tree.root = tree.update(tree.root, i, x)
}

func (tree *Conc) update(p *Node, i list.Position, x list.Data) *Node {
   if p.isLeaf() {
      return tree.asLeaf(x)
   }
   if i < p.l.s {
      return tree.link(tree.update(p.l, i, x), p.r)
   } else {
      return tree.link(p.l, tree.update(p.r, i-p.l.s, x))
   }
}

func (tree *Conc) Insert(i list.Position, x list.Data) {
   assert(i <= tree.size)
   tree.size++
   if tree.root == nil {
      tree.root = tree.asLeaf(x)
   } else {
      tree.root = tree.insert(tree.root, i, x)
   }
}

func (tree *Conc) insert(p *Node, i list.Position, x list.Data) *Node {
   if p.isLeaf() {
      if i == 0 {
         return tree.link(tree.asLeaf(x), p)
      } else {
         return tree.link(p, tree.asLeaf(x))
      }
   }
   if i < p.l.s {
      return tree.balanced(tree.insert(p.l, i, x), p.r)
   } else {
      return tree.balanced(p.l, tree.insert(p.r, i-p.l.s, x))
   }
}

func (tree *Conc) Delete(i list.Position) (x list.Data) {
   assert(i < tree.size)
   tree.size--
   tree.root = tree.delete(tree.root, i, &x)
   return
}

func (tree *Conc) delete(p *Node, i list.Position, x *list.Data) *Node {
   if p.isLeaf() {
      *x = p.x
      return nil
   }
   if i < p.l.s {
      return tree.balanced(tree.delete(p.l, i, x), p.r)
   } else {
      return tree.balanced(p.l, tree.delete(p.r, i-p.l.s, x))
   }
}

func (tree Conc) Split(i list.Position) (list.List, list.List) {
   assert(i <= tree.size)
   if i == 0 {
      return &Conc{Tree: Tree{pool: tree.pool}},
         &Conc{Tree: Tree{pool: tree.pool, root: tree.root, size: tree.size}}
   } else {
      l, r := tree.split(tree.root, i)
      return &Conc{Tree: Tree{pool: tree.pool, root: l, size: i}}, &Conc{Tree: Tree{pool: tree.pool, root: r, size: tree.size - i}}
   }
}

func (tree Conc) split(p *Node, i list.Position) (*Node, *Node) {
   if p.isLeaf() {
      if i == 0 {
         return nil, p
      } else {
         return p, nil
      }
   }
   if i < p.l.s {
      l, r := tree.split(p.l, i)
      return l, tree.balanced(r, p.r)
   } else {
      l, r := tree.split(p.r, i-p.l.s)
      return tree.balanced(p.l, l), r
   }
}

func (tree Conc) Join(other list.List) list.List {
   return &Conc{
      Tree: Tree{
         pool: tree.pool,
         root: tree.join(other.(*Conc)),
         size: tree.size + other.Size(),
      },
   }
}

func (tree *Conc) join(other *Conc) *Node {
   return tree.balanced(tree.root, other.root)
}

func (tree Conc) Each(visit func(list.Data)) {
   tree.inorder(tree.root, visit)
}

func (tree Conc) inorder(p *Node, visit func(list.Data)) {
   if p == nil {
      return
   }
   if p.isLeaf() {
      visit(p.x)
   } else {
      tree.inorder(p.l, visit)
      tree.inorder(p.r, visit)
   }
}

func (tree Conc) Verify() {
   tree.verifyLeaves(tree.root)
   tree.verifyHeights(tree.root)
   tree.verifySizes(tree.root)
}

func (tree Conc) verifyHeights(p *Node) {
   if p == nil {
      return
   }
   if p.isLeaf() {
      invariant(p.y == 0)
   } else {
      invariant(utility.Distance(p.l.y, p.r.y) <= 1)
   }
   tree.verifyHeights(p.l)
   tree.verifyHeights(p.r)
}

func (tree Conc) verifyLeaves(p *Node) {
   if p == nil {
      return
   }
   invariant((p.l == nil) == (p.r == nil))
   tree.verifyLeaves(p.l)
   tree.verifyLeaves(p.r)
}

func (tree Conc) verifySizes(p *Node) {
   if p == nil {
      return
   }
   invariant(p.s == Conc{}.Count(p))
   tree.verifySizes(p.l)
   tree.verifySizes(p.r)
}
