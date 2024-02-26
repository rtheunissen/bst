//go:build !goexperiment.arenas
package trees

type Allocator struct {
}

func (tree *Tree) Free() {

}

// Clone creates a shallow copy of the tree and shares its root with the copy.
func (tree *Tree) Clone() Tree {
   tree.share(tree.root)
   return *tree
}

// Allocates memory for a new node and copies the given node into that memory.
func (tree *Tree) allocate(node Node) (allocated *Node) {
   // measurement(&allocations, 1)
   return &node
}