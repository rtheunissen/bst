package trees

import "github.com/rtheunissen/bst/types/list"

type AVLRelaxedBottomUp struct {
   AVLBottomUp
   AVLRelaxed
}

func (tree AVLRelaxedBottomUp) Verify() {
   tree.verifySize(tree.root, tree.size)
   tree.verifyRanks(tree.root)
}

func (AVLRelaxedBottomUp) New() list.List {
   return &AVLRelaxedBottomUp{}
}

func (tree *AVLRelaxedBottomUp) Clone() list.List {
   return &AVLRelaxedBottomUp{
      AVLBottomUp: AVLBottomUp{
         Tree: tree.Tree.Clone(),
      },
   }
}

func (tree *AVLRelaxedBottomUp) Delete(i list.Position) list.Data {
 return tree.Tree.Delete(i)
}

func (tree AVLRelaxedBottomUp) buildL(l *Node, p *Node, r *Node, sl list.Size) (root *Node) {
   if tree.rank(l) - tree.rank(r) <= 1 {
      p.l = l
      p.r = r
      p.s = sl
      p.y = uint64(tree.rank(l) + 1)
      return p
   }
   tree.persist(&l)
   l.r = tree.build(l.r, p, r, sl-l.s-1)
   return tree.balanceInsertR(l)
}

func (tree AVLRelaxedBottomUp) buildR(l *Node, p *Node, r *Node, sl list.Size) (root *Node) {
   if tree.rank(r) - tree.rank(l) <= 1 {
      p.l = l
      p.r = r
      p.s = sl
      p.y = uint64(tree.rank(r) + 1)
      return p
   }
   tree.persist(&r)
   r.s = 1 + sl + r.s
   r.l = tree.build(l, p, r.l, sl)
   return tree.balanceInsertL(r)
}

func (tree *AVLRelaxedBottomUp) build(l, p, r *Node, sl list.Size) *Node {
   if tree.rank(l) <= tree.rank(r) {
      return tree.buildR(l, p, r, sl)
   } else {
      return tree.buildL(l, p, r, sl)
   }
}

func (tree AVLRelaxedBottomUp) join(l, r *Node, sl list.Size) (p *Node) {
   if l == nil { return r }
   if r == nil { return l }
   if tree.rank(l) <= tree.rank(r) {
      return tree.build(l, tree.Tree.deleteMin(&r), r, sl)
   } else {
      return tree.build(l, tree.Tree.deleteMax(&l), r, sl-1)
   }
}

func (tree AVLRelaxedBottomUp) Join(other list.List) list.List {
   l := tree
   r := other.(*AVLRelaxedBottomUp)

   tree.share(l.root)
   tree.share(r.root)

   return &AVLRelaxedBottomUp{
      AVLBottomUp: AVLBottomUp{
         Tree: Tree{
            pool: tree.pool,
            root: tree.join(l.root, r.root, l.size),
            size: l.size + r.size,
         },
      },
   }
}

func (tree AVLRelaxedBottomUp) split(p *Node, i, s list.Size) (l, r *Node) {
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

func (tree AVLRelaxedBottomUp) Split(i list.Position) (list.List, list.List) {
   assert(i <= tree.size)
   tree.share(tree.root)
   l, r := tree.split(tree.root, i, tree.size)

   return &AVLRelaxedBottomUp{AVLBottomUp: AVLBottomUp{Tree: Tree{pool: tree.pool, root: l, size: i}}},
          &AVLRelaxedBottomUp{AVLBottomUp: AVLBottomUp{Tree: Tree{pool: tree.pool, root: r, size: tree.size - i}}}
}
