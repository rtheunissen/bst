package trees

import "github.com/rtheunissen/bst/types/list"

type AVLWeakTopDown struct {
   AVLWeakBottomUp
}

func (AVLWeakTopDown) New() list.List {
   return &AVLWeakTopDown{}
}

func (tree *AVLWeakTopDown) Clone() list.List {
   return &AVLWeakTopDown{
      AVLWeakBottomUp{
         AVLBottomUp: AVLBottomUp{
            Tree: tree.Tree.Clone(),
         },
      },
   }
}

func (tree *AVLWeakTopDown) dissolve(p **Node, x *Data) {
   old := *p
   tree.share(old.l)
   tree.share(old.r)
   *x = old.x
   *p = tree.join(old.l, old.r, old.s, old.s)
   tree.release(old)
}

func (tree *AVLWeakTopDown) Update(i Size, x Data) {
   tree.Tree.Update(i, x)
}

func (tree *AVLWeakTopDown) Select(i Size) Data {
   return tree.Tree.Select(i)
}

// This top-down insertion algorithm was translated and paraphrased from the
// _Deletion Without Rebalancing in Binary Search Trees_ paper referenced above.
func (tree *AVLWeakTopDown) insert(p **Node, i Position, x Data) {
  //
  // "If the tree is empty, create a new node containing the item to be inserted
  //  and make it the root, completing the insertion."
  //
  if *p == nil {
     tree.attach(p, x)
     return
  }
  tree.persist(p)
  //
  // "Otherwise, promote the root if it is 1,1."
  //
  if isOneOne(*p) {
     promote(*p)
  }
  // "This establishes the invariant for the main loop for the algorithm:
  //  *p is a non-nil node that is not a 1,1-node."
  //
  for {
     assert(!isOneOne(*p))

     // "From *p, take one step down the search path..."
     //
     if i <= (*p).s {
        //
        // LEFT
        //
        // "If the next node on the search path is nil, replace it by a new
        //  node containing the item to be inserted, completing the insertion.
        //
        //  The new node cannot be a 0-child since the parent is not a 1,1-node
        //  and hence has positive rank."
        //
        if (*p).l == nil {
           tree.attachL(*p, x)
           return
        }
        //
        // "If the next node on the search path is not a 1,1-node, continue."
        //
        if !isOneOne((*p).l) {
           tree.pathLeft(&p)
           continue
        }
        // "If the next node on the search path is not a 1-child, promote it,
        //  then continue to the next step."
        //
        if !isOneChild(*p, (*p).l) {
           tree.pathLeft(&p)
           promote(*p)
           continue
        }
        // "In the remaining cases, the next node is a 1,1-node and a 1-child."
        //
        // "From this node, take one further step down the search path..."
        //
        if i <= (*p).l.s {
           //
           // LEFT LEFT
           //
           // "If this node is nil, replace it by a new node containing the
           //  item to be inserted. If the new node and its parent are both
           //  left children, or, symmetrically, both right children, do a
           //  rotate step, completing the insertion."
           //
           if (*p).l.l == nil {
              tree.attachLL(*p, x)
              tree.treeRotateR(p)
              promote(*p)
              demote((*p).r)
              return
           }
           //
           // "If the new node is a right child and its parent a left child, or
           //  symmetrically if the new node is a left child and its parent a
           //  right child, do a double rotate step, completing the insertion."
           //
           //  ^That is not the case here because we know this is a left-left.
           //
           // "If this node is not a 1,1-node, continue with both search steps."
           //
           if !isOneOne((*p).l.l) {
              tree.pathLeft(&p)
              tree.pathLeft(&p)
              continue
           }
           // "Otherwise promote the new node and its parent, making its parent
           //  a 0-child, then do a rotate or double rotate step to make all
           //  rank differences positive."
           tree.treeRotateR(p)
           promote(*p)
           demote((*p).r)
           tree.pathLeft(&p)
           promote(*p)
           continue

        } else {
           //
           // LEFT RIGHT
           //
           // "If the new node is a right child and its parent a left child, or
           //  symmetrically if the new node is a left child and its parent a
           //  right child, do a double rotate step, completing the insertion."
           //
           // ^That is the case here because we know this is a left-right step,
           //  which requires a double rotation, follows by the right and left
           //  steps down the search path after the rotation.
           //
           if (*p).l.r == nil {
              tree.attachLR(*p, x)
              tree.treeRotateLR(p)
              promote(*p)
              demote((*p).r)
              return
           }
           //
           // "If this node is not a 1,1-node, continue with both search steps."
           //
           if !isOneOne((*p).l.r) {
              tree.pathLeft(&p)
              tree.pathRight(&p, &i)
              continue
           }
           // "Otherwise promote the new node and its parent, making its parent
           //  a 0-child, then do a rotate or double rotate step to make all
           //  rank differences positive."
           //
           tree.treeRotateLR(p)
           promote(*p)
           promote(*p)
           demote((*p).r)
           //
           // "If a double rotation is done, take one further step down the
           //  search path after the rotation. Ths completes the step."
           //
           if i <= (*p).s {
              tree.pathLeft(&p) // LRL
           } else {
              tree.pathRight(&p, &i) // LRR
           }
        }
     } else {
        //
        // RIGHT
        //
        // Comments follow symmetrically from above.
        //
        if (*p).r == nil {
           tree.attachR(*p, x)
           return
        }
        if !isOneOne((*p).r) {
           tree.pathRight(&p, &i)
           continue
        }
        if !isOneChild(*p, (*p).r) {
           tree.pathRight(&p, &i)
           promote(*p)
           continue
        }

        if i > (*p).s+(*p).r.s+1 {
           //
           // RIGHT RIGHT
           //
           if (*p).r.r == nil {
              tree.attachRR(*p, x)
              tree.treeRotateL(p)
              promote(*p)
              demote((*p).l)
              return
           }
           if !isOneOne((*p).r.r) {
              tree.pathRight(&p, &i)
              tree.pathRight(&p, &i)
              continue
           }
           tree.treeRotateL(p)
           promote(*p)
           demote((*p).l)
           tree.pathRight(&p, &i)
           promote(*p)
           continue

        } else {
           //
           // RIGHT LEFT
           //
           if (*p).r.l == nil {
              tree.attachRL(*p, x)
              tree.treeRotateRL(p)
              demote((*p).l)
              promote(*p)
              return
           }
           if !isOneOne((*p).r.l) {
              tree.pathRight(&p, &i)
              tree.pathLeft(&p)
              continue
           }
           tree.treeRotateRL(p)
           promote(*p)
           promote(*p)
           demote((*p).l)

           if i > (*p).s {
              tree.pathRight(&p, &i) // RLR
           } else {
              tree.pathLeft(&p) // RLL
           }
        }
     }
  }
}

func (tree *AVLWeakTopDown) Insert(i Position, x Data) {
   tree.insert(&tree.root, i, x)
   tree.size = tree.size + 1
}

func (tree *AVLWeakTopDown) Delete(i Position) (x Data) {
   assert(i < tree.size)
   x = tree.delete(&tree.root, i)
   tree.size--
   return
}

func (tree AVLWeakTopDown) join(l, r *Node, sl, sr Size) (p *Node) {
   if l == nil { return r }
   if r == nil { return l }
   if rank(l) < rank(r) || (rank(l) == rank(r) && sl <= sr) {
      return tree.build(l, tree.deleteMin(&r), r, sl)
   } else {
      return tree.build(l, tree.deleteMax(&l, sl), r, sl-1)
   }
}

func (tree AVLWeakTopDown) Join(other list.List) list.List {
   l := tree
   r := other.(*AVLWeakTopDown)
   tree.share(l.root)
   tree.share(r.root)
   return &AVLWeakTopDown{
      AVLWeakBottomUp{
         AVLBottomUp: AVLBottomUp{
            Tree: Tree{
               pool: tree.pool,
               root: tree.join(l.root, r.root, l.size, r.size),
               size: l.size + r.size,
            },
         },
      },
   }
}


func (tree AVLWeakTopDown) split(p *Node, i, s Size) (l, r *Node) {
   if p == nil {
      return
   }
   tree.persist(&p)

   sl := p.s
   sr := s - p.s - 1

   if i <= (*p).s {
      l, r = tree.split(p.l, i, sl)
         r = tree.build(r, p, p.r, sl-i)
   } else {
      l, r = tree.split(p.r, i-sl-1, sr)
         l = tree.build(p.l, p, l, sl)
   }
   return l, r
}


func (tree AVLWeakTopDown) Split(i Position) (list.List, list.List) {
   assert(i <= tree.size)
   tree.share(tree.root)
   l, r := tree.split(tree.root, i, tree.size)
   return &AVLWeakTopDown{AVLWeakBottomUp{AVLBottomUp: AVLBottomUp{Tree: Tree{pool: tree.pool, root: l, size: i}}}},
          &AVLWeakTopDown{AVLWeakBottomUp{AVLBottomUp: AVLBottomUp{Tree: Tree{pool: tree.pool, root: r, size: tree.size - i}}}}
}

func (tree AVLWeakTopDown) rebalanceOnDelete(p *Node) *Node {
   if isThreeChild(p, p.r) {
      if isOneChild(p.l, p.l.l) {
         tree.treeRotateR(&p)
         promote(p)
         demote(p.r)

         assert(isTwoChild(p, p.l))
         assert(isOneChild(p, p.r))

         if p.r.l == nil {
            assert(isTwoTwo(p.r))
            demote(p.r)
         }
      } else {
         tree.treeRotateLR(&p)
         promote(p)
         promote(p)
         demote(p.l)
         demote(p.r)
         demote(p.r)

         assert(isTwoChild(p, p.l))
         assert(isTwoChild(p, p.r))
      }
   } else if isThreeChild(p, p.l) {
      if isOneChild(p.r, p.r.r) {
         tree.treeRotateL(&p)
         promote(p)
         demote(p.l)

         assert(isOneChild(p, p.l))
         assert(isTwoChild(p, p.r))

         if p.l.r == nil {
            assert(isTwoTwo(p.l))
            demote(p.l)
         }
      } else {
         tree.treeRotateRL(&p)
         promote(p)
         promote(p)
         demote(p.l)
         demote(p.l)
         demote(p.r)

         assert(isTwoChild(p, p.l))
         assert(isTwoChild(p, p.r))
      }
   }
   return p
}

// "Deletion of a unary node converts the child that replaces it
//   into a 2- or 3-child; the latter violates the rank rule."
func (tree AVLWeakTopDown) rebalanceAfterDissolve(g **Node, p **Node) {
   //
   // "Deletion of a leaf may convert its parent, previously a 1,2 node
   //  into a 2,2 leaf, violating the rank rule. In this case we begin
   //  by demoting the parent, which may make it a 3-child."
   //
   if isLeaf(*p) && isTwoTwo(*p) {
      demote(*p)
   }
   *g = tree.rebalanceOnDelete(*g)
   *p = tree.rebalanceOnDelete(*p)
}

//
// "In a deletion, if the current node is 2,2 or it is 1,2 and its 1-child
//   is 2,2, we can force a reset on the next search step by demoting the
//   current node in the former case, or the current node and its 1-child
//   in the latter, and rebalancing top-down from the safe node."
//
func (tree AVLWeakTopDown) resetSafeNode(p *Node) bool {
   if isTwoTwo(p) {
      demote(p)
      return true
   } else if isTwoChild(p, p.l) && isTwoTwo(p.r) {
      assert(isOneChild(p, p.r))
      tree.persist(&p.r)
      demote(p)
      demote(p.r)
      return true
   } else if isTwoChild(p, p.r) && isTwoTwo(p.l) {
      assert(isOneChild(p, p.l))
      tree.persist(&p.l)
      demote(p)
      demote(p.l)
      return true
   }
   return false // Could not reset the safe node.
}

func (tree AVLWeakTopDown) delete(p **Node, i Position) (x Data) {
   if (*p).s == i {
     tree.dissolve(p, &x)
     return
   }
   g := p
   for {
      tree.persist(p)
      if tree.resetSafeNode(*p) {
         *g = tree.rebalanceOnDelete(*g)
      }
      if i < (*p).s {
         deleteL(*p)
         if (*p).l.s == i {
            tree.dissolve(&(*p).l, &x)
            tree.rebalanceAfterDissolve(g, p)
            return
         }
         g = p
         p = &(*p).l

      } else {
         deleteR(*p, &i)
         if (*p).r.s == i {
            tree.dissolve(&(*p).r, &x)
            tree.rebalanceAfterDissolve(g, p)
            return
         }
         g = p
         p = &(*p).r
      }
   }
}
func (tree AVLWeakTopDown) deleteMax(p **Node, s Size) (max *Node) {
   g := p
   if (*p).r == nil {
      return tree.replacedByLeftSubtree(p)
   }
   for {
      tree.persist(p)
      if tree.resetSafeNode(*p) {
         *g = tree.rebalanceOnDelete(*g)
      }
      r := pathDeletingRightIgnoringIndex(*p)
      if (*p).r.r == nil {
         max = tree.replacedByLeftSubtree(r)
         tree.rebalanceAfterDissolve(g, p)
         return
      }
      g = p
      p = r
   }
}

func (tree AVLWeakTopDown) deleteMin(p **Node) (min *Node) {
   g := p
   if (*p).l == nil {
     return tree.replacedByRightSubtree(p)
   }
   for {
     tree.persist(p)
      if tree.resetSafeNode(*p) {
         *g = tree.rebalanceOnDelete(*g)
      }
     l := deleteL(*p)
     if (*p).l.l == nil {
        min = tree.replacedByRightSubtree(l)
        tree.rebalanceAfterDissolve(g, p)
        return
     }
     g = p
     p = l
   }
}
