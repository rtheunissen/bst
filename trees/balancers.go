package trees

import (
   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/utility"
   "github.com/rtheunissen/bst/utility/number"
   "math"
)

type Balancer interface {
   Restore(Tree) Tree
   Verify(Tree)
}

func partition(p *Node, i uint64) *Node {
   assert(i < p.size())
   // measurement(&partitionCount, 1)

   n := Node{s: i}
   l := &n
   r := &n
   for i != p.s {
      // measurement(&partitionDepth, 1)
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
   l.r = p.l
   r.l = p.r
   p.l = n.r
   p.r = n.l
   p.s = n.s
   return p
}

type Median struct{}

func (balancer Median) balance(p *Node, s list.Size) *Node {
   if s <= 2 {
      return p
   }
   if !balancer.isBalanced(p, s) {
      p = partition(p, s >> 1)
   }
   p.l = balancer.balance(p.l, p.sizeL())
   p.r = balancer.balance(p.r, p.sizeR(s))
   return p
}

func (balancer Median) Restore(tree Tree) Tree {
   tree.root = balancer.balance(tree.root, tree.size)
   return tree
}

func (balancer Median) Verify(tree Tree) {
   balancer.verify(tree.root, tree.size)
}

// -1 <= L - R <= 1
func (balancer Median) verify(p *Node, s list.Size) {
   if p == nil {
      return
   }
   invariant(utility.Distance(p.sizeL(), p.sizeR(s)) <= 1)

   balancer.verify(p.l, p.sizeL())
   balancer.verify(p.r, p.sizeR(s))
}

func (Median) isBalanced(p *Node, s list.Size) bool {
   sl := p.sizeL()
   sr := p.sizeR(s)
   return sl + 1 >= sr &&
          sr + 1 >= sl
}

type Height struct{}

func (balancer Height) balance(p *Node, s list.Size) *Node {
   if s <= 2 {
      return p
   }
   if !balancer.isBalanced(p, s) {
      p = partition(p, s >> 1)
   }
   p.l = balancer.balance(p.l, p.sizeL())
   p.r = balancer.balance(p.r, p.sizeR(s))
   return p
}

func (balancer Height) Restore(tree Tree) Tree {
   tree.root = balancer.balance(tree.root, tree.size)
   return tree
}

func (Height) isBalanced(p *Node, s list.Size) bool {
   sl := p.sizeL()
   sr := p.sizeR(s)
   return utility.GreaterThanOrEqualToMSB(sl + 1, sr) &&
          utility.GreaterThanOrEqualToMSB(sr + 1, sl)
}

func (balancer Height) Verify(tree Tree) {
   balancer.verify(tree.root, tree.size)
}

// A node is height-balanced when the difference between the height of its
// subtrees is no greater than 1, and both subtrees are also height-balanced.
//
// invariant(p.height() <= FloorLog2(s))
func (balancer Height) verify(p *Node, s list.Size) (height int) {
   if p == nil {
      return
   }
   heightL := balancer.verify(p.l, p.sizeL())
   heightR := balancer.verify(p.r, p.sizeR(s))

   invariant(utility.Distance(heightL, heightR) <= 1)

   return 1 + max(heightL, heightR)
}

type Log struct{}

func (balancer Log) balance(p *Node, s list.Size) *Node {
   if s <= 3 {
      return p
   }
   if !balancer.balanced(p, s) {
      p = partition(p, s >> 1)
   }
   p.l = balancer.balance(p.l, p.sizeL())
   p.r = balancer.balance(p.r, p.sizeR(s))
   return p
}

func (balancer Log) balanced(p *Node, s list.Size) bool {
   sl := p.sizeL()
   sr := p.sizeR(s)
   return utility.GreaterThanOrEqualToMSB(sl + 1, (sr + 1) >> 1) &&
          utility.GreaterThanOrEqualToMSB(sr + 1, (sl + 1) >> 1)
}

func (balancer Log) Restore(tree Tree) Tree {
   tree.root = balancer.balance(tree.root, tree.size)
   return tree
}

func (balancer Log) Verify(tree Tree) {
   balancer.verify(tree.root, tree.size)
}

// -1 <= ⌊log₂(L)⌋ - ⌊log₂(R)⌋ <= 1
func (balancer Log) verify(p *Node, s list.Size) {
   if p == nil {
      return
   }
   sl := p.sizeL()
   sr := p.sizeR(s)

   invariant(utility.Distance(utility.Log2(sl + 1), utility.Log2(sr + 1)) <= 1)

   balancer.verify(p.l, sl)
   balancer.verify(p.r, sr)
}





type Weight struct{}

func (balancer Weight) balance(p *Node, s list.Size) *Node {
   if s <= 3 {
      return p
   }
   if !balancer.isBalanced(p.sizeL(), p.sizeR(s)) {
      p = partition(p, s >> 1)
   }
   p.l = balancer.balance(p.l, p.sizeL())
   p.r = balancer.balance(p.r, p.sizeR(s))
   return p
}

func (balancer Weight) isBalanced(x, y list.Size) bool {
   return (x + 1) >= (y + 1) >> 1 &&
          (y + 1) >= (x + 1) >> 1
}

func (balancer Weight) Restore(tree Tree) Tree {
   tree.root = balancer.balance(tree.root, tree.size)
   return tree
}

func (balancer Weight) Verify(tree Tree) {
   balancer.verify(tree.root, tree.size)
}

func (balancer Weight) verify(p *Node, s list.Size) {
   if p == nil {
      return
   }
   sl := p.sizeL()
   sr := p.sizeR(s)

   invariant((sl + 1) >= (sr + 1) / 2)
   invariant((sr + 1) >= (sl + 1) / 2)

   balancer.verify(p.l, sl)
   balancer.verify(p.r, sr)
}





type Cost struct{}

func (balancer Cost) Restore(tree Tree) Tree {
   tree.root = balancer.balance(tree.root, tree.size)
   return tree
}

func (balancer Cost) balance(p *Node, s list.Size) *Node {
   if s <= 2 {
      return p
   }
   if !balancer.isBalanced(p, s) {
      p = partition(p, s >> 1)
   }
   p.l = balancer.balance(p.l, p.sizeL())
   p.r = balancer.balance(p.r, p.sizeR(s))
   return p
}

func (Cost) isBalanced(p *Node, s list.Size) bool {
   if p.sizeL() >= p.sizeR(s) {
     return p.sizeR(s) >= p.l.sizeL() && p.sizeR(s) >= p.l.sizeR(p.sizeL())
   } else {
     return p.sizeL() >= p.r.sizeR(p.sizeR(s)) && p.sizeL() >= p.r.sizeL()
   }
}

func (balancer Cost) Verify(tree Tree) {
   balancer.verify(tree.root, tree.size)
}

func (balancer Cost) verify(p *Node, s list.Size) (height int) {
   if p == nil {
      return
   }
   invariant(p.l == nil || p.sizeR(s) >= p.l.sizeL())
   invariant(p.l == nil || p.sizeR(s) >= p.l.sizeR(p.sizeL()))

   invariant(p.r == nil || p.sizeL() >= p.r.sizeL())
   invariant(p.r == nil || p.sizeL() >= p.r.sizeR(p.sizeR(s)))

   heightL := balancer.verify(p.l, p.sizeL())
   heightR := balancer.verify(p.r, p.sizeR(s))

   height = 1 + max(heightL, heightR)

   invariant(height <= int(1.44 * math.Log2(float64(s + 2)) - 0.328)) // Knuth?

   return height
}

type DSW struct {
}

func (balancer DSW) Verify(tree Tree) {
   invariant(tree.root.height() == int(utility.Log2(tree.size)))
}

func (balancer DSW) Restore(tree Tree) Tree {
   tree.root = balancer.toTree(balancer.toVine(tree.root), tree.size)
   return tree
}

func (balancer DSW) toVine(p *Node) (vine *Node) {
   n := Node{}
   l := &n
   for p != nil {
      for p.l != nil {
         p = p.rotateR()
      }
      l.r = p
      l = l.r
      p = p.r
   }
   return n.r
}

func (balancer DSW) toTree(vine *Node, size list.Size) *Node {
   m := list.Size(1 << utility.Log2(size + 1) - 1)
   p := balancer.compress(vine, size - m)
   for m > 1 {
       m = m >> 1
       p = balancer.compress(p, m)
   }
   return p
}

func (balancer DSW) compress(p *Node, k list.Size) *Node {
   n := Node{}
   l := &n
   n.r = p
   for ; k > 0; k-- {
      l.r = p.rotateL()
      l = l.r
      p = l.r
   }
   return n.r
}

func (Tree) Vine(size list.Size) Tree {
   t := Tree{}
   n := Node{}
   p := &n
   for t.size = 0; t.size < size; t.size++ {
      p.r = t.allocate(Node{})
      p = p.r
   }
   t.root = n.r
   return t
}

func (Tree) WorstCaseMedianVine(size list.Size) Tree {
   assert(size > 0)
   t := Tree{}
   n := Node{}
   p := &n
   for t.size = 0; t.size < (size-1)/2+1; t.size++ {
      p.r = t.allocate(Node{})
      p = p.r
   }
   for ; t.size < size; t.size++ {
      p.l = t.allocate(Node{})
      p.s = size - t.size
      p = p.l
   }
   t.root = n.r
   return t
}

func (tree Tree) Randomize(access number.Distribution) Tree {
   tree.root = tree.randomize(access, tree.root, tree.size)
   return tree
}

func (tree Tree) randomize(access number.Distribution, p *Node, s list.Size) *Node {
   assert(p.size() == s)
   if p == nil {
      return nil
   }
   p = tree.partition(p, access.LessThan(s))
   p.l = tree.randomize(access, p.l, p.sizeL())
   p.r = tree.randomize(access, p.r, p.sizeR(s))
   return p
}
