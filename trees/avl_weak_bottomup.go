package trees

import "github.com/rtheunissen/bst/types/list"

type AVLWeakBottomUp struct {
   AVLWeak
   AVLBottomUp
}

func (tree AVLWeakBottomUp) Verify() {
   tree.verifySize(tree.root, tree.size)
   tree.AVLWeak.verifyRanks(tree.root)
   tree.AVLWeak.verifyHeight(tree.root, tree.size)
}

func (AVLWeakBottomUp) New() list.List {
   return &AVLWeakBottomUp{}
}

func (tree *AVLWeakBottomUp) Clone() list.List {
   return &AVLWeakBottomUp{
      AVLBottomUp: AVLBottomUp{
         Tree: tree.Tree.Clone(),
      },
   }
}

func (tree AVLWeakBottomUp) delete(p *Node, i list.Position, x *list.Data) *Node {
   if i == p.s {
      *x = p.x
      defer tree.release(p)
      tree.share(p.l)
      tree.share(p.r)
      return tree.join(p.l, p.r, p.s)
   }
   tree.persist(&p)
   if i < p.s {
      p.s = p.s - 1
      p.l = tree.delete(p.l, i, x)
   } else {
      p.r = tree.delete(p.r, i-p.s-1, x)
   }
   return tree.rebalanceOnDelete(p)
}

func (tree *AVLWeakBottomUp) Delete(i list.Position) (x list.Data) {
   assert(i < tree.size)
   tree.root = tree.delete(tree.root, i, &x)
   tree.size = tree.size - 1
   return
}

func (tree *AVLWeakBottomUp) Insert(i list.Position, x list.Data) {
   tree.AVLBottomUp.Insert(i, x)
}

// TODO split into L and R?
func (tree AVLWeakBottomUp) rebalanceOnDelete(p *Node) *Node {
   if p.isLeaf() && tree.isTwoTwo(p) {
      tree.demote(p)
      return p
   }
   if tree.isThreeChild(p, p.r) {
      if tree.isTwoChild(p, p.l) {
         tree.demote(p)

      } else if tree.isTwoTwo(p.l) {
         tree.demote(p.l)
         tree.demote(p)

      } else if tree.isOneChild(p.l, p.l.l) {
         tree.rotateR(&p)
         tree.promote(p)
         tree.demote(p.r)

         assert(tree.isTwoChild(p, p.l))
         assert(tree.isOneChild(p, p.r))

         if p.r.l == nil {
            assert(tree.isTwoTwo(p.r))
            tree.demote(p.r)
         }
      } else {
         tree.rotateLR(&p)
         tree.promote(p)
         tree.promote(p)
         tree.demote(p.l)
         tree.demote(p.r)
         tree.demote(p.r)

         assert(tree.isTwoChild(p, p.l))
         assert(tree.isTwoChild(p, p.r))
      }
   } else if tree.isThreeChild(p, p.l) {
      if tree.isTwoChild(p, p.r) {
         tree.demote(p)

      } else if tree.isTwoTwo(p.r) {
         tree.demote(p.r)
         tree.demote(p)

      } else if tree.isOneChild(p.r, p.r.r) {
         tree.rotateL(&p)
         tree.promote(p)
         tree.demote(p.l)

         assert(tree.isOneChild(p, p.l))
         assert(tree.isTwoChild(p, p.r))

         if p.l.r == nil {
            assert(tree.isTwoTwo(p.l))
            tree.demote(p.l)
         }
      } else {
         tree.rotateRL(&p)
         tree.promote(p)
         tree.promote(p)
         tree.demote(p.l)
         tree.demote(p.l)
         tree.demote(p.r)

         assert(tree.isTwoChild(p, p.l))
         assert(tree.isTwoChild(p, p.r))
      }
   }
   return p
}

func (tree AVLWeakBottomUp) extractMin(p *Node, min **Node) *Node {
   if p.l == nil {
      *min = tree.replacedByRightSubtree(&p)
      return p
   }
   tree.persist(&p)
   p.s = p.s - 1
   p.l = tree.extractMin(p.l, min)
   return tree.rebalanceOnDelete(p)
}

// TODO: rename to deleteMin and sort out conflicts with relaxed which embeds it
func (tree AVLWeakBottomUp) extractMax(p *Node, max **Node) *Node {
   if p.r == nil {
      *max = tree.replacedByLeftSubtree(&p)
      return p
   }
   tree.persist(&p)
   p.r = tree.extractMax(p.r, max)
   return tree.rebalanceOnDelete(p)
}


// Constructs a balanced tree with root `p` where all nodes in `l` are to the
// left of `p` and all nodes in `r` to the right of `p`.
//
// The rank of `r` is greater than or equal to the rank of `l`.
//
//                                    p
//                                    .       r
//                              l            ↙
//                             ↙            /\
//                            /\           /\ \
//                           /  \      ~l /  \ \
//                          /____\       /____\_\
//
//
// Follow the left spine of `r` to find a subtree that is similar in rank to `l`
// then build a new subtree with parent `p`, left subtree `l` and right `r`.
//
// To update the size of `r`, which is the eventual size of its left subtree,
// consider that the left subtree of `r` will consist of all the nodes in `l`,
// then `p`,all the nodes currently in that subtree.
//
func (tree *AVLWeakBottomUp) buildL(l, p, r *Node, sl list.Size) *Node {
   assert(tree.rank(r) >= tree.rank(l))
   if tree.rankDifference(r, l) <= 1 {
      p.l = l
      p.r = r
      p.s = sl
      p.y = uint64(tree.rank(r) + 1)
      return p
   }
   tree.persist(&r)
   r.s = r.sizeL() + sl + 1
   r.l = tree.buildL(l, p, r.l, sl)
   return tree.balanceInsertL(r)
}

// Symmetric
func (tree *AVLWeakBottomUp) buildR(l, p, r *Node, sl list.Size) *Node {
   assert(tree.rank(l) >= tree.rank(r))
   if tree.rankDifference(l, r) <= 1 {
      p.l = l
      p.r = r
      p.s = sl
      p.y = uint64(tree.rank(l) + 1)
      return p
   }
   tree.persist(&l)
   l.r = tree.buildR(l.r, p, r, l.sizeR(sl))
   return tree.balanceInsertR(l)
}

// Constructs a balanced tree with root p where all nodes of l are to the left
// of p and all nodes in r are to the right of p.
func (tree *AVLWeakBottomUp) build(l, p, r *Node, sl list.Size) *Node {
   if tree.rank(l) < tree.rank(r) {
      return tree.buildL(l, p, r, sl)
   } else {
      return tree.buildR(l, p, r, sl)
   }
}

func (tree AVLWeakBottomUp) join(l, r *Node, sl list.Size) (p *Node) {
  if l == nil { return r }
  if r == nil { return l }
  if tree.rank(l) <= tree.rank(r) {
     return tree.build(l, p, tree.extractMin(r, &p), sl)
  } else {
     return tree.build(tree.extractMax(l, &p), p, r, sl-1)
  }
}

func (tree AVLWeakBottomUp) Join(that list.List) list.List {
   tree.share(tree.root)
   tree.share(that.(*AVLWeakBottomUp).root)
   return &AVLWeakBottomUp{
      AVLBottomUp: AVLBottomUp{
         Tree: Tree{
            pool: tree.pool,
            root: tree.join(tree.root, that.(*AVLWeakBottomUp).root, tree.size),
            size: tree.size + that.(*AVLWeakBottomUp).size,
         },
      },
   }
}

func (tree AVLWeakBottomUp) split(p *Node, i, s list.Size) (l, r *Node) {
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

func (tree AVLWeakBottomUp) Split(i list.Position) (list.List, list.List) {
   assert(i <= tree.size)
   tree.share(tree.root)
   l, r := tree.split(tree.root, i, tree.size)

   return &AVLWeakBottomUp{AVLBottomUp: AVLBottomUp{Tree: Tree{pool: tree.pool, root: l, size: i}}},
          &AVLWeakBottomUp{AVLBottomUp: AVLBottomUp{Tree: Tree{pool: tree.pool, root: r, size: tree.size - i}}}
}
