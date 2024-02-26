package trees

import (
   "github.com/rtheunissen/bst/types/list"
)

type WBSTBottomUp struct {
   Tree
   WeightBalance
}

func (WBSTBottomUp) New() list.List {
   return &WBSTBottomUp{
      WeightBalance: ThreeTwo{},
   }
}

func (tree *WBSTBottomUp) Clone() list.List {
   return &WBSTBottomUp{
      WeightBalance: tree.WeightBalance,
      Tree: tree.Tree.Clone(),
   }
}
func (tree WBSTBottomUp) verifyBalance(p *Node, s list.Size) {
   if p == nil {
      return
   }
   sl := p.sizeL()
   sr := p.sizeR(s)

   invariant(tree.isBalanced(sl, sr))
   invariant(tree.isBalanced(sr, sl))

   tree.verifyBalance(p.l, sl)
   tree.verifyBalance(p.r, sr)
}

func (tree WBSTBottomUp) verifyHeight(root *Node, size list.Size) {
   invariant(root.height() <= tree.maxHeight(size))
}

func (tree WBSTBottomUp) Verify() {
   tree.verifySizes()
   tree.verifyBalance(tree.root, tree.size)
   tree.verifyHeight(tree.root, tree.size)
}


func (tree WBSTBottomUp) join(l *Node, r *Node, sl, sr list.Size) (k *Node) {
   if l == nil { return r }
   if r == nil { return l }
   if sl <= sr {
      r = tree.extractMin(r, sr, &k)
      return tree.build(l, k, r, sl, sr-1)
   } else {
      l = tree.extractMax(l, sl, &k)
      return tree.build(l, k, r, sl-1, sr)
   }
}

func (tree WBSTBottomUp) extractMin(p *Node, s list.Size, x **Node) *Node {
   tree.persist(&p)
   if p.l == nil {
      *x = p
      p = p.r
      return p
   }
   sl := p.s
   sr := s - p.s - 1

   p.l = tree.extractMin(p.l, p.s, x)
   p.s--

   if !tree.isBalanced(sl-1, sr) {
      srl := (*p).r.s
      srr := sr - (*p).r.s - 1
      //
      if tree.singleRotation(srr, srl) {
         tree.rotateL(&p)
      } else {
         tree.rotateRL(&p)
      }
   }
   return p
}

func (tree WBSTBottomUp) extractMax(p *Node, s list.Size, x **Node) *Node {
   tree.persist(&p)
   if p.r == nil {
      *x = p
      p = p.l
      return p
   }
   sl := p.s
   sr := s - p.s - 1

   p.r = tree.extractMax(p.r, sr, x)
   if !tree.isBalanced(sr-1, sl) {
      if tree.singleRotation((*p).l.s, sl-(*p).l.s-1) {
         tree.rotateR(&p)
      } else {
         tree.rotateLR(&p)
      }
   }
   return p
}


func (tree WBSTBottomUp) Join(that list.List) list.List {
   l := tree
   r := that
   tree.share(l.root)
   tree.share(r.(*WBSTBottomUp).root)
   return &WBSTBottomUp{
      WeightBalance: tree.WeightBalance,
      Tree: Tree{
         pool: tree.pool,
         root: tree.join(l.root, r.(*WBSTBottomUp).root, l.size, r.(*WBSTBottomUp).size),
         size: l.size + r.(*WBSTBottomUp).size,
      },
   }
}

func (tree WBSTBottomUp) build(l, p, r *Node, sl, sr list.Size) *Node {
   if sl <= sr { // TODO: consider == here?
      return tree.buildR(p, l, r, sl, sr)
   } else {
      return tree.buildL(p, l, r, sl, sr)
   }
}

func (tree *WBSTBottomUp) buildL(p *Node, l, r *Node, sl, sr list.Size) *Node {
   if tree.isBalanced(sr, sl) {
      p.l = l
      p.r = r
      p.s = sl
      return p
   }
   tree.persist(&l)

   sll := l.s
   slr := sl - l.s - 1

   l.r = tree.buildL(p, l.r, r, slr, sr)
   slr = 1 + sr + slr

   if !tree.isBalanced(sll, slr) {

      srr := slr - l.r.s - 1
      srl := l.r.s

      if tree.singleRotation(srr, srl) {
         tree.rotateL(&l)
      } else {
         tree.rotateRL(&l)
      }
   }
   return l
}

func (tree *WBSTBottomUp) buildR(p *Node, l, r *Node, sl, sr list.Size) *Node {
   if tree.isBalanced(sl, sr) {
      p.l = l
      p.r = r
      p.s = sl
      return p
   }
   tree.persist(&r)

   srl := r.s
   srr := sr - r.s - 1

   r.l = tree.buildR(p, l, r.l, sl, srl)
   r.s = 1 + sl + srl

   if !tree.isBalanced(srr, r.s) {
      if tree.singleRotation(r.l.s, r.s-r.l.s-1) {
         tree.rotateR(&r)
      } else {
         tree.rotateLR(&r)
      }
   }
   return r
}

func (tree *WBSTBottomUp) split(p *Node, i, s list.Size) (l, r *Node) {
   if p == nil {
      return
   }
   tree.persist(&p)

   sl := p.s
   sr := s - p.s - 1

   if i <= (*p).s {
      l, r = tree.split(p.l, i, sl)
      r = tree.build(r, p, p.r, sl-i, sr)
   } else {
      l, r = tree.split(p.r, i-sl-1, sr)
      l = tree.build(p.l, p, l, sl, i-sl-1)
   }
   return l, r
}

func (tree *WBSTBottomUp) Split(i list.Position) (list.List, list.List) {
   tree.share(tree.root)
   l, r := tree.split(tree.root, i, tree.size)

   return &WBSTBottomUp{WeightBalance: tree.WeightBalance, Tree: Tree{pool: tree.pool, root: l, size: i}},
          &WBSTBottomUp{WeightBalance: tree.WeightBalance, Tree: Tree{pool: tree.pool, root: r, size: tree.size - i}}
}


func (tree *WBSTBottomUp) insert(p *Node, s list.Size, i list.Position, x list.Data) *Node {
   if p == nil {
      return tree.allocate(Node{x: x})
   }
   tree.persist(&p)
   sl := p.s
   sr := s - p.s - 1

   assert(tree.isBalanced(sl, sr))
   assert(tree.isBalanced(sr, sl))

   if i <= p.s {
      p.l = tree.insert(p.l, sl, i, x)
      p.s = p.s + 1

      if !tree.isBalanced(sr, sl+1) {
         if !tree.singleRotation((*p).l.s, p.s-(*p).l.s-1) {
            tree.rotateLR(&p)
         } else {
            tree.rotateR(&p)
         }
      }
   } else {
      p.r = tree.insert(p.r, sr, i-sl-1, x)

      if !tree.isBalanced(sl, sr+1) {
         if !tree.singleRotation(sr+1-(*p).r.s-1, (*p).r.s) {
            tree.rotateRL(&p)
         } else {
            tree.rotateL(&p)
         }
      }
   }
   return p
}

func (tree *WBSTBottomUp) delete(p *Node, s list.Size, i list.Position, x *list.Data) *Node {
   sl := p.s
   sr := s - p.s - 1

   assert(tree.isBalanced(sl, sr))
   assert(tree.isBalanced(sr, sl))

   if i == p.s {
      defer tree.release(p)
      *x = p.x
      if p.l == nil { return p.r }
      if p.r == nil { return p.l }

      tree.persist(&p)
      if sl > sr {
         var max *Node
         p.l = tree.extractMax(p.l, sl, &max)
         p.x = max.x
         p.s--
      } else {
         var min *Node
         p.r = tree.extractMin(p.r, sr, &min)
         p.x = min.x
      }
      return p
   }
   tree.persist(&p)
   if i < p.s {
      p.l = tree.delete(p.l, sl, i, x)
      p.s--
      if !tree.isBalanced(sl-1, sr) {
         srl := (*p).r.s
         srr := sr - (*p).r.s - 1
         if tree.singleRotation(srr, srl) {
            tree.rotateL(&p)
         } else {
            tree.rotateRL(&p)
         }
      }
   } else {
      p.r = tree.delete(p.r, sr, i-sl-1, x)
      if !tree.isBalanced(sr-1, sl) {
         sll := (*p).l.s
         slr := sl - (*p).l.s - 1
         if tree.singleRotation(sll, slr) {
            tree.rotateR(&p)
         } else {
            tree.rotateLR(&p)
         }
      }
   }
   return p
}

func (tree *WBSTBottomUp) Select(i list.Size) list.Data {
   assert(i < tree.size)
   return tree.lookup(tree.root, i)
}

func (tree *WBSTBottomUp) Update(i list.Size, x list.Data) {
   assert(i < tree.size)
   tree.persist(&tree.root)
   tree.update(tree.root, i, x)
}

func (tree *WBSTBottomUp) Insert(i list.Position, x list.Data) {
   assert(i <= tree.size)
   tree.root = tree.insert(tree.root, tree.size, i, x)
   tree.size++
}

func (tree *WBSTBottomUp) Delete(i list.Position) (x list.Data) {
   assert(i < tree.size)
   tree.root = tree.delete(tree.root, tree.size, i, &x)
   tree.size--
   return
}