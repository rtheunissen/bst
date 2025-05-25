package trees

// ReferenceCounter is used to implement copy-on-write persistence.
//
//   "Copy-on-write, sometimes referred to as implicit sharing or shadowing,
//    is a resource-management technique used in computer programming to
//    efficiently implement a "duplicate" or "copy" operation on modifiable
//    resources. If a resource is duplicated but not modified, it is not
//    necessary to create a new resource; the resource can be shared between
//    the copy and the original. Modifications must still create a copy, hence
//    the technique: the copy operation is deferred until the first write."
//
//   "By sharing resources in this way, it is possible to significantly reduce
//    the resource consumption of unmodified copies, while adding a small
//    overhead to resource-modifying operations."
//
//    https://en.wikipedia.org/wiki/Copy-on-write
//
// A tree can be made _immutable_ by always creating a clone of the tree before
// making a modification. Anything still referencing the previous version is not
// aware of the modification and every version can be used independently. This
// technique allows one implementation to be used as both in-place and immutable
// where the immutable variant simply creates a clone before each modification.
//
// To avoid copying the entire tree, multiple trees may reference the same node,
// allowing independent trees to share common subtrees in memory. We refer to
// a node as "shared" if at least one other tree also references it. To track
// how many other trees reference a given node, we maintain a "reference count".
//
// When the reference count is zero, it means that the node is not shared and is
// therefore considered a "unique reference" because no other trees reference it.
// A unique reference can be modified without the need to copy it first, and can
// therefore be modified "in-place". In short, if a tree never shares its nodes
// then none of its nodes will ever need to be copied.
//
type ReferenceCounter struct {
	refs uint64
}

// AddReference increments the reference count.
func (rc *ReferenceCounter) AddReference() {
	rc.refs++
}

// RemoveReference decrements the reference count.
func (rc *ReferenceCounter) RemoveReference() {
	if rc.refs > 0 {
		rc.refs--
	}
}

// Removes one reference from the given node.
func (tree Tree) release(p *Node) {
	if p != nil {
		p.RemoveReference()
	}
}

// Adds a reference to the node to share it with another tree.
func (tree Tree) share(p *Node) {
	if p != nil {
		p.AddReference()
	}
}

// Determines whether a node is shared with at least one other tree.
func (tree Tree) shared(p *Node) bool {
	return (*p).refs > 0
}

// Replaces the given node with a copy only if the node shared with other trees.
func (tree *Tree) persist(p **Node) {
	assert(*p != nil)
	//
	// There is no need to copy the node if it is NOT shared with other trees.
	//
	if !tree.shared(*p) {
		return
	}
	// A copy is required to modify the node stored at the given reference.
	//
	// Only the pointers to the left and right subtrees are copied, increasing
	// their reference counts by one and thereby sharing them with another tree.
	//
	tree.share((*p).l)
	tree.share((*p).r)

	// The resulting copy is a new allocation that has no other references to it,
	// so we decrease the reference count of the original node by one.
	//
	// Allocate memory for a new node and copy the original values into it.
	//
	// Notice that this allocation uses the same arena as the original tree,
	// so all versions of a tree use the same arena and will be freed together.
	//
	*p = tree.allocate(Node{
		l: (*p).l,
		r: (*p).r,
		s: (*p).s,
		x: (*p).x,
		y: (*p).y,
	})
}
