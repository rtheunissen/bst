package trees


type BinaryTreeNode interface {
	Height() int
	TotalInternalPathLength(Size) Size
	TotalReferenceCount() Size
}

// Node
//
// A node is a simple structure that can be linked to other nodes. Each node has
// a left and right outgoing link to other nodes.
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

type Position = uint64

type Data = uint64

type Node struct {
	refs uint64 // Reference count for copy-on-write persistence.

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

	averagePathLength(p.l, depth+1, totalDepth, totalNodes)
	averagePathLength(p.r, depth+1, totalDepth, totalNodes)
}

func nodeAveragePathLength(p *Node) float64 {
	if p == nil {
		return 0
	}
	var totalDepth uint64
	var totalNodes uint64
	averagePathLength(p, 0, &totalDepth, &totalNodes)
	return float64(totalDepth) / float64(totalNodes)
}

func isLeaf(p *Node) bool {
	return p.l == nil && p.r == nil
}

func nodeMaximumPathLength(p *Node) int {
	return height(p)
}

func height(p *Node) int {
	if p == nil {
		return -1
	}
	return 1 + max(height(p.l), height(p.r))
}

// Counts the number of nodes reachable from p*, including itself.
func size(p *Node) Size {
	if p == nil {
		return 0
	} else {
		return 1 + size(p.l) + size(p.r)
	}
}

// Returns the number of nodes in the left subtree of p*.
func sizeL(p *Node) Size {
	return p.s
}

// Returns the number of nodes in the right subtree of p*, given the s of p*.
func sizeR(p *Node, s Size) Size {
	return s - p.s - 1
}

func inorder(p *Node, visit func(Data)) {
	if p == nil {
		return
	}
	inorder(p.l, visit)
	visit(p.x)
	inorder(p.r, visit)
}

func rotateL(p *Node) (r *Node) {
	// measurement(&rotations, 1)
	r = p.r
	p.r = r.l
	r.l = p
	r.s = r.s + p.s + 1
	return r
}

func rotateR(p *Node) (l *Node) {
	// measurement(&rotations, 1)
	l = p.l
	p.l = l.r
	l.r = p
	p.s = p.s - l.s - 1
	return l
}

// Rotates the LEFT subtree LEFT, then rotates the root RIGHT.
func rotateLR(p *Node) *Node {
	p.l = rotateL(p.l)
	return rotateR(p)
}

// Rotates the RIGHT subtree RIGHT, then rotates the root LEFT.
func rotateRL(p *Node) *Node {
	p.r = rotateR(p.r)
	return rotateL(p)
}

func (tree Tree) verifySize(p *Node, s Size) Size {
	if p == nil {
		return 0
	}
	sl := tree.verifySize(p.l, sizeL(p))
	sr := tree.verifySize(p.r, sizeR(p, s))

	invariant(s == sl+sr+1)
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

func (tree *Tree) update(p *Node, i Position, x Data) {
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

func insertR(p *Node, i *Position) **Node {
	*i = *i - p.s - 1
	return &p.r
}

func deleteL(p *Node) **Node {
	p.s--
	return &p.l
}

func deleteR(p *Node, i *Position) **Node {
	*i = *i - p.s - 1
	return &p.r
}

func (tree *Tree) pathLeft(p ***Node) {
	assert((**p).l != nil)
	tree.persist(&(**p).l)
	*p = insertL(**p)
}
func (tree *Tree) pathRight(p ***Node, i *Position) {
	assert((**p).r != nil)
	tree.persist(&(**p).r)
	*p = insertR(**p, i)
}
func (tree *Tree) attach(p **Node, x Data) {
	*p = tree.allocate(Node{x: x})
}
func (tree *Tree) attachL(p *Node, x Data) {
	p.s++
	p.l = tree.allocate(Node{x: x})
}

func (tree *Tree) attachLL(p *Node, x Data) {
	tree.persist(&p.l)
	p.s++
	p.l.s++
	p.l.l = tree.allocate(Node{x: x})
}
func (tree *Tree) attachRR(p *Node, x Data) {
	tree.persist(&p.r)
	p.r.r = tree.allocate(Node{x: x})
}
func (tree *Tree) attachLR(p *Node, x Data) {
	tree.persist(&p.l)
	p.s++
	p.l.r = tree.allocate(Node{x: x})
}

func (tree *Tree) attachRL(p *Node, x Data) {
	tree.persist(&p.r)
	p.r.s++
	p.r.l = tree.allocate(Node{x: x})
}

func (tree *Tree) attachR(p *Node, x Data) {
	p.r = tree.allocate(Node{x: x})
}

func pathDeletingRightIgnoringIndex(p *Node) **Node {
	return &p.r
}

func (tree Tree) treeRotateL(p **Node) {
	tree.persist(&(*p).r)
	*p = rotateL(*p)
}

func (tree Tree) treeRotateR(p **Node) {
	tree.persist(&(*p).l)
	*p = rotateR(*p)
}

func (tree Tree) treeRotateRL(p **Node) {
	tree.persist(&(*p).r)
	tree.persist(&(*p).r.l)
	*p = rotateRL(*p)
}

func (tree Tree) treeRotateLR(p **Node) {
	tree.persist(&(*p).l)
	tree.persist(&(*p).l.r)
	*p = rotateLR(*p)
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
