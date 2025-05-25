package trees

import (
	"github.com/rtheunissen/bst/types/list"
	"github.com/rtheunissen/bst/utility/random"
)

type BinaryTree interface {
   Root() *Node
   Size() list.Size

   InteriorHeightsAlongTheSpines() [2][]int
   ExteriorHeightsAlongTheSpines() [2][]int
   NodesPerLevel() [2][]list.Size
}

type Tree struct {
   pool Allocator
   root *Node
   size list.Size
}

// Deletes the node at position `i` from the tree.
// Returns the data that was in the deleted value.
func (tree *Tree) Delete(i list.Position) list.Data {
   assert(i < tree.size)
   x := tree.delete(&tree.root, tree.size, i)
   tree.size--
   return x
}
func (tree *Tree) DeleteRoot() list.Data {
   x := tree.delete(&tree.root, tree.size, tree.root.s)
   tree.size--
   return x
}

func (tree *Tree) Select(i list.Size) list.Data {
   assert(i < tree.size)
   return tree.lookup(tree.root, i)
}

func (tree *Tree) Update(i list.Size, x list.Data) {
   assert(i < tree.size)
   tree.persist(&tree.root)
   tree.update(tree.root, i, x)
}

func (tree *Tree) insert(p **Node, s list.Size, i list.Position, x list.Data) {
   for {
      if *p == nil {
         *p = tree.allocate(Node{x: x})
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

func (tree *Tree) Insert(i list.Size, x list.Data) {
   tree.insert(&tree.root, tree.size, i, x)
   tree.size++
}

func (tree Tree) Size() list.Size {
   return tree.size
}

func (tree Tree) Root() *Node {
   return tree.root
}

func (tree Tree) lookup(p *Node, i list.Size) list.Data {
   for {
      if i == p.s {
         return p.x
      }
      if i < p.s {
         p = p.l
      } else {
         i = i - p.s - 1
         p = p.r
      }
   }
}

func (tree Tree) Each(visit func(list.Data)) {
   tree.root.inorder(visit)
}

func (tree *Tree) join(l, r *Node, sl, sr list.Size) *Node {
  if l == nil {
     return r
  }
  if r == nil {
     return l
  }
  if sl <= sr {
     tree.persist(&l)
     tree.persist(&r)
     p := tree.deleteMin(&r)
     p.l = l
     p.r = r
     p.s = sl
     return p
  } else {
     tree.persist(&l)
     tree.persist(&r)
     p := tree.deleteMax(&l)
     p.r = r
     p.l = l
     p.s = sl - 1
     return p
  }
}

func (tree *Tree) partition(p *Node, i uint64) *Node {
   assert(i < p.size())
   // measurement(&partitionCount, 1)

   n := Node{s: i}
   l := &n
   r := &n
   for i != p.s {
      // measurement(&partitionDepth, 1)
      tree.persist(&p)
      if i < p.s {
       p.s = p.s - i - 1
       r.l = p
         r = r.l
         p = p.l
      } else {
         i = i - p.s - 1
       l.r = p
         l = l.r
         p = p.r
      }
   }
   tree.persist(&p)
   l.r = p.l
   r.l = p.r
   p.l = n.r
   p.r = n.l
   p.s = n.s
   return p
}

func (tree *Tree) dissolveHibbard(p *Node, s list.Size) *Node {
   if p.r == nil {
      return p.l
   }
   r := tree.deleteMin(&p.r)
   r.l = p.l
   r.r = p.r
   r.s = p.s
   r.y = p.y
   return r
}

func (tree *Tree) dissolveRandom(p *Node, s list.Size) *Node {
   if p.l == nil {
      return p.r
   }
   if p.r == nil {
      return p.l
   }
   if random.LessThan(p.sizeL() + p.sizeR(s), random.Uniform()) < p.sizeL() {
      l := tree.deleteMax(&p.l)
      l.r = p.r
      l.l = p.l
      l.s = p.s - 1
      l.y = p.y
      return l
   } else {
      r := tree.deleteMin(&p.r)
      r.l = p.l
      r.r = p.r
      r.s = p.s
      r.y = p.y
      return r
   }
}

func (tree *Tree) dissolveSymmetric(p *Node, s list.Size) *Node {
   if p.l == nil {
      return p.r
   }
   if p.r == nil {
      return p.l
   }
   if random.Uint64() & 1 == 0 {
      l := tree.deleteMax(&p.l)
      l.r = p.r
      l.l = p.l
      l.s = p.s - 1
      l.y = p.y
      return l
   } else {
      r := tree.deleteMin(&p.r)
      r.l = p.l
      r.r = p.r
      r.s = p.s
      r.y = p.y
      return r
   }
}

func (tree *Tree) dissolveKnuth(p *Node, s list.Size) *Node {
   if p.l == nil {
      return p.r
   }
   if p.r == nil {
      return p.l
   }
   l := tree.deleteMax(&p.l)
   l.r = p.r
   l.l = p.l
   l.s = p.s - 1
   l.y = p.y
   return l
}

func (tree *Tree) dissolvePreferSmallerSubtree(p *Node, s list.Size) *Node {
   if p.l == nil {
      return p.r
   }
   if p.r == nil {
      return p.l
   }
   if p.sizeL() > p.sizeR(s) {
      r := tree.deleteMin(&p.r)
      r.l = p.l
      r.r = p.r
      r.s = p.s
      r.y = p.y
      return r
   } else {
      l := tree.deleteMax(&p.l)
      l.r = p.r
      l.l = p.l
      l.s = p.s - 1
      l.y = p.y
      return l
   }
}

func (tree *Tree) dissolvePreferLargerSubtree(p *Node, s list.Size) *Node {
   if p.l == nil {
      return p.r
   }
   if p.r == nil {
      return p.l
   }
   if p.sizeL() < p.sizeR(s) {
      r := tree.deleteMin(&p.r)
      r.l = p.l
      r.r = p.r
      r.s = p.s
      r.y = p.y
      return r
   } else {
      l := tree.deleteMax(&p.l)
      l.r = p.r
      l.l = p.l
      l.s = p.s - 1
      l.y = p.y
      return l
   }
}

func (tree *Tree) dissolve(p *Node, s list.Size) *Node {
   //return tree.dissolveHibbard(p, s)
   //return tree.dissolveKnuth(p, s)
   //return tree.dissolveSymmetric(p, s)
   //return tree.dissolveRandom(p, s)
   //return tree.dissolvePreferSmallerSubtree(p, s)
   return tree.dissolvePreferLargerSubtree(p, s)
}

func (tree *Tree) delete(p **Node, s list.Size, i list.Size) (x list.Data) {
   for {
      tree.persist(p)
      if i == (*p).s {
          x = (*p).x
         *p = tree.dissolve(*p, s)
         return x
      }
      if i < (*p).s {
         s = (*p).s
    (*p).s = (*p).s - 1
         p = &(*p).l
      } else {
         i = i - (*p).s - 1
         s = s - (*p).s - 1
         p = &(*p).r
      }
   }
}

func (tree *Tree) split(p *Node, i uint64) (*Node, *Node) {
   n := Node{}
   l := &n
   r := &n
   for p != nil {
      tree.persist(&p)
      if i <= p.s {
       p.s = p.s - i
       r.l = p
         r = r.l
         p = p.l
      } else {
         i = i - p.s - 1
       l.r = p
         l = l.r
         p = p.r
      }
   }
   l.r = nil
   r.l = nil
   return n.r, n.l
}

func (tree *Tree) Split(i list.Size) (Tree, Tree) {
   assert(i <= tree.size)
   tree.share(tree.root)
   l, r := tree.split(tree.root, i)
   return Tree{pool: tree.pool, root: l, size: i}, // TODO: new arenas?
          Tree{pool: tree.pool, root: r, size: tree.size - i}
}

func (tree *Tree) Join(other Tree) Tree {
   tree.share(tree.root)
   tree.share(other.root)
   return Tree{
      pool: tree.pool,
      size: tree.size + other.size,
      root: tree.join(tree.root, other.root, tree.size, other.size),
   }
}

func (tree Tree) joinWith(other Tree, join func(l, r *Node, sl list.Size) *Node) Tree {
   tree.share(tree.root)
   tree.share(other.root)
   return Tree{
      pool: tree.pool, // TODO: remove?
      size: tree.size + other.size,
      root: join(tree.root, other.root, tree.size),
   }
}

func (tree Tree) verifySizes() {
   tree.verifySize(tree.root, tree.size)
}

func (tree Tree) InteriorHeightsAlongTheSpines() (h [2][]int) {
   if tree.root == nil {
      return
   }
   //
   for l := tree.root.l; l != nil; l = l.l {
      h[0] = append(h[0], l.r.height()+1)
   }
   for r := tree.root.r; r != nil; r = r.r {
      h[1] = append(h[1], r.l.height()+1)
   }

   // Reverse the left spine.
   i := 0
   j := len(h[0]) - 1
   for i < j {
      h[0][i], h[0][j] = h[0][j], h[0][i]
      i++
      j--
   }
   return
}

func (tree Tree) ExteriorHeightsAlongTheSpines() (h [2][]int) {
   if tree.root == nil {
      return
   }
   //
   for l := tree.root.l; l != nil; l = l.l {
      h[0] = append(h[0], l.height())
   }
   for r := tree.root.r; r != nil; r = r.r {
      h[1] = append(h[1], r.height())
   }

   // Reverse the left spine.
   i := 0
   j := len(h[0]) - 1
   for i < j {
      h[0][i], h[0][j] = h[0][j], h[0][i]
      i++
      j--
   }
   return
}

func (tree Tree) countNodesPerLevel(p *Node, counter *[]list.Size, level int) {
   if p == nil {
      return
   }
   // Add more levels to the counter as needed on the way down.
   if len(*counter) <= level {
      *counter = append(*counter, 0)
   }
   (*counter)[level]++
   tree.countNodesPerLevel(p.l, counter, level+1)
   tree.countNodesPerLevel(p.r, counter, level+1)
}

func (tree Tree) NodesPerLevel() (weights [2][]list.Size) {
   if tree.root == nil {
      return
   }
   tree.countNodesPerLevel(tree.root.l, &weights[0], 0)
   tree.countNodesPerLevel(tree.root.r, &weights[1], 0)
   return
}
