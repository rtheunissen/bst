package trees

import (
   "github.com/rtheunissen/bst/types/list"
)

type BinaryTreeNode interface {
   Height() int
   TotalInternalPathLength(list.Size) list.Size
   TotalReferenceCount() list.Size
}

// Node
//
// A node is a simple structure that can be linked to other nodes. Each node has
// a left and right outgoing link to other nodes. TODO: parent, binary tree
// The node at the top of the tree with no parent is called the _root_ node.
//
//
//                                   (P)arent
//                                 ↙     ↘
//                               (L)eft  (R)ight
//
//
// A node is a container for a unit of information.
// When multiple nodes are linked together they form a binary tree.
// This structure allows the ability to organize information.
// One type of organization is a sequence - linear, list, etc.
//
// A sequence is implied by binary search tree, where the parent appears in the
// sequence after the left node and before the right node.
//
// This can be viewed as a binary search tree ordered by sequential position.
//
// Every node tracks its 0-based position relative to the start of its sequence,
// equal to the number of nodes in its left subtree. Given the total size of a
// node, we can calculate the sizes of both subtrees without referencing them.
//
//
//          Position:     0   1   2   3   4   5   6
//
//          Sequence:    [e,  x,  a,  m,  p,  l,  e]
//
//                                    3
//          ParseAnimation:                    (m)
//                            1   ↙       ↘   1
//                           (x)             (l)
//                        0 ↙   ↘ 0       0 ↙   ↘ 0
//                       (e)     (a)     (p)     (e)
//
//
// Notice the vertical projection of the sequence onto the tree, which follows
// the in-order traversal from the root, recursively left-self-right.
//
//
// Multiple trees may share the same node, allowing independent trees to be made
// up of common subtrees shared in memory. Making a change to one tree data
// in a new tree that shares most of the previous structure. The reference count
// of a node is the number of other trees that reference it, thus zero indicates
// that a node is only referenced by its own tree and is not shared by others.

type Rank = uint64

type Size = uint64

type Data = uint64

type Node struct {
   ReferenceCounter

   l *Node // Pointers to the left and right subtrees.
   r *Node

   s Size // Size, usually of the left subtree and therefore also position.
   x Data // Data
   y Rank // Rank
}

func averagePathLength(p *Node, depth uint64, totalDepth *uint64, totalNodes *uint64) {
   if p == nil {
      return
   }
   *totalNodes = *totalNodes + 1
   *totalDepth = *totalDepth + depth

   averagePathLength(p.l, depth + 1, totalDepth, totalNodes)
   averagePathLength(p.r, depth + 1, totalDepth, totalNodes)
}

func (p *Node) AveragePathLength() float64 {
   if p == nil {
      return 0
   }
   var totalDepth uint64
   var totalNodes uint64
   averagePathLength(p, 0, &totalDepth, &totalNodes)
   return float64(totalDepth) / float64(totalNodes)
}

func (p Node) isLeaf() bool { // TODO conc can have its own that doesn't check p.r for nil
   return p.l == nil && p.r == nil
}

func (p *Node) MaximumPathLength() int {
   return p.height()
}

// make global function?
func (p *Node) height() int {
   if p == nil {
      return -1
   }
   return 1 + max(p.l.height(), p.r.height())
}

// Counts the number of nodes reachable from p*, including itself.
func (p *Node) size() list.Size {
   if p == nil {
      return 0
   } else {
      return 1 + p.l.size() + p.r.size()
   }
}

// Returns the number of nodes in the left subtree of p*.
// TODO: This is not the case for all tree implementations - should this be up to the tree? Maybe mix it in?
func (p *Node) sizeL() list.Size {
   return p.s
}

// Returns the number of nodes in the right subtree of p*, given the s of p*.
func (p *Node) sizeR(s list.Size) list.Size {
   return s - p.s - 1
}

func (p *Node) inorder(visit func(list.Data)) {
   if p == nil {
      return
   }
   p.l.inorder(visit)
   visit(p.x)
   p.r.inorder(visit)
}

func (p *Node) rotateL() (r *Node) {
   // measurement(&rotations, 1)
   r = p.r
   p.r = r.l
   r.l = p
   r.s = r.s + p.s + 1
   return r
}

func (p *Node) rotateR() (l *Node) {
   // measurement(&rotations, 1)
   l = p.l
   p.l = l.r
   l.r = p
   p.s = p.s - l.s - 1
   return l
}

// Rotates the LEFT subtree LEFT, then rotates the root RIGHT.
func (p *Node) rotateLR() *Node {
   p.l = p.l.rotateL()
   return p.rotateR()
}

// Rotates the RIGHT subtree RIGHT, then rotates the root LEFT.
func (p *Node) rotateRL() *Node {
   p.r = p.r.rotateR()
   return p.rotateL()
}

func (tree Tree) verifySize(p *Node, s list.Size) list.Size {
   if p == nil {
      return 0
   }
   sl := tree.verifySize(p.l, p.sizeL())
   sr := tree.verifySize(p.r, p.sizeR(s))

   invariant(s == sl + sr + 1)
   return s
}

func (tree *Tree) replacedByRightSubtree(p **Node) *Node {
   tree.persist(p)
   r := *p
   *p = (*p).r
   return r
}

func (tree *Tree) replacedByLeftSubtree(p **Node) *Node {
   tree.persist(p)
   l := *p
   *p = (*p).l
   return l
}

func (tree *Tree) deleteMin2(r *Node) (root, min *Node) {
   n := Node{}
   l := &n
   for {
      tree.persist(&r)
      if r.l == nil {
         l.l = r.r
         return n.l, r
      }
      r.s = r.s - 1
      l.l = r
      l = l.l
      r = r.l
   }
}

func (tree *Tree) deleteMin(p **Node) (min *Node) {
   *p, min = tree.deleteMin2(*p)
   return
}
func (tree *Tree) deleteMax2(p *Node) (root, min *Node) {
   n := Node{}
   r := &n
   for {
      tree.persist(&p)
      if p.r == nil {
         r.r = p.l
         return n.r, p
      }
      r.r = p
      r = r.r
      p = p.r
   }
}
func (tree *Tree) deleteMax(p **Node) (max *Node) {
   *p, max = tree.deleteMax2(*p)
   return
}

func (tree *Tree) update(p *Node, i list.Position, x list.Data) {
   for {
      if i == p.s {
         p.x = x
         return
      }
      if i < p.s {
         tree.persist(&p.l)
         p = p.l
      } else {
         tree.persist(&p.r)
         i = i - p.s - 1
         p = p.r
      }
   }
}

func insertL(p *Node) **Node {
   p.s++
   return &p.l
}

func insertR(p *Node, i *list.Position) **Node {
   *i = *i - p.s - 1
   return &p.r
}

func deleteL(p *Node) **Node {
   p.s--
   return &p.l
}

func deleteR(p *Node, i *list.Position) **Node {
   *i = *i - p.s - 1
   return &p.r
}

// TODO: these are nuts
func (tree *Tree) pathLeft(p ***Node) {
   assert((**p).l != nil)
   tree.persist(&(**p).l)
   *p = insertL(**p)
}
func (tree *Tree) pathRight(p ***Node, i *list.Position) {
   assert((**p).r != nil)
   tree.persist(&(**p).r)
   *p = insertR(**p, i)
}
func (tree *Tree) attach(p **Node, x list.Data) {
   *p = tree.allocate(Node{x: x})
}
func (tree *Tree) attachL(p *Node, x list.Data) {
   p.s++
   p.l = tree.allocate(Node{x: x})
}

func (tree *Tree) attachLL(p *Node, x list.Data) {
   tree.persist(&p.l)
   p.s++
   p.l.s++
   p.l.l = tree.allocate(Node{x: x})
}
func (tree *Tree) attachRR(p *Node, x list.Data) {
   tree.persist(&p.r)
   p.r.r = tree.allocate(Node{x: x})
}
func (tree *Tree) attachLR(p *Node, x list.Data) {
   tree.persist(&p.l)
   p.s++
   p.l.r = tree.allocate(Node{x: x})
}

func (tree *Tree) attachRL(p *Node, x list.Data) {
   tree.persist(&p.r)
   p.r.s++
   p.r.l = tree.allocate(Node{x: x})
}

func (tree *Tree) attachR(p *Node, x list.Data) {
   p.r = tree.allocate(Node{x: x})
}

func pathDeletingRightIgnoringIndex(p *Node) **Node {
   return &p.r
}

func (tree Tree) rotateL(p **Node) {
   tree.persist(&(*p).r)
   *p = (*p).rotateL()
}

func (tree Tree) rotateR(p **Node) {
   tree.persist(&(*p).l)
   *p = (*p).rotateR()
}

func (tree Tree) rotateRL(p **Node) {
   tree.persist(&(*p).r)
   tree.persist(&(*p).r.l)
   *p = (*p).rotateRL()
}

func (tree Tree) rotateLR(p **Node) {
   tree.persist(&(*p).l)
   tree.persist(&(*p).l.r)
   *p = (*p).rotateLR()
}

func (tree *Tree) appendR(p **Node, n *Node) {
   for *p != nil {
      tree.persist(p)
      p = &(*p).r
   }
   *p = n
}

func (tree *Tree) appendL(p **Node, n *Node) {
   for *p != nil {
      tree.persist(p)
      p = &(*p).l
   }
   *p = n
}