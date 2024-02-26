//go:build goexperiment.arenas
package trees

import (
   arenas "arena"
)

// Arena is a binary search tree node allocator that uses a memory arena.
//
// Using an arena significantly reduces the overhead and impact of the garbage
// collector and improves the locality of nodes that belong to the same tree.
//
type Arena struct {
   *arenas.Arena
}

type Allocator = Arena

func (tree *Tree) Free() {
   if tree.pool.Arena != nil {
      tree.pool.Arena.Free()
      tree.pool.Arena = nil
   }
}

// Clone creates a shallow copy of the tree and shares its root with the copy.
func (tree *Tree) Clone() Tree {
   if tree.pool.Arena == nil {
      tree.pool.Arena = arenas.NewArena()
   }
   tree.share(tree.root)
   return *tree
}

// Allocates memory for a new node and copies the given node into that memory.
func (tree *Tree) allocate(node Node) (allocated *Node) {
   if tree.pool.Arena == nil {
      tree.pool.Arena = arenas.NewArena()
   }
   // measurement(&allocations, 1)
   allocated = arenas.New[Node](tree.pool.Arena)
   *allocated = node
   return
}