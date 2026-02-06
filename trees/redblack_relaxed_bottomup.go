package trees

import "github.com/rtheunissen/bst/types/list"

type RedBlackRelaxedBottomUp struct {
   RedBlackBottomUp
   RedBlackRelaxed
}

func (tree RedBlackRelaxedBottomUp) Verify() {
   tree.Tree.verifySize(tree.root, tree.size)
   tree.RedBlackRelaxed.verifyRanks(tree.root)
   tree.RedBlackRelaxed.verifyHeight(tree.root)
}

func (tree RedBlackRelaxedBottomUp) New() list.List {
   return &RedBlackRelaxedBottomUp{}
}

func (tree *RedBlackRelaxedBottomUp) Clone() list.List {
   return &RedBlackRelaxedBottomUp{
      RedBlackBottomUp: *tree.RedBlackBottomUp.Clone().(*RedBlackBottomUp),
   }
}

func (tree *RedBlackRelaxedBottomUp) Insert(i Position, x Data) {
   tree.RedBlackBottomUp.Insert(i, x)
}

func (tree *RedBlackRelaxedBottomUp) Delete(i Position) Data {
   return tree.Tree.Delete(i)
}

func (tree *RedBlackRelaxedBottomUp) Select(i Size) Data {
   return tree.Tree.Select(i)
}

func (tree *RedBlackRelaxedBottomUp) Update(i Size, x Data) {
   tree.Tree.Update(i, x)
}

func (tree RedBlackRelaxedBottomUp) join(l, r *Node, sl, sr Size) (p *Node) {
   if l == nil { return r }
   if r == nil { return l }
   if tree.rank(l) < tree.rank(r) || (tree.rank(l) == tree.rank(r) && sl <= sr) {
      return tree.build(l, tree.Tree.deleteMin(&r), r, sl)
   } else {
      return tree.build(l, tree.Tree.deleteMax(&l), r, sl-1)
   }
}

func (tree *RedBlackRelaxedBottomUp) Join(other list.List) list.List {
   return &RedBlackRelaxedBottomUp{
      RedBlackBottomUp: RedBlackBottomUp{
         Tree: tree.Tree.joinWith(
            other.(*RedBlackRelaxedBottomUp).Tree, tree.join,
         ),
      },
   }
}

func (tree *RedBlackRelaxedBottomUp) Split(i Position) (list.List, list.List) {
   l, r := tree.RedBlackBottomUp.Split(i)
   return &RedBlackRelaxedBottomUp{RedBlackBottomUp: *l.(*RedBlackBottomUp)},
          &RedBlackRelaxedBottomUp{RedBlackBottomUp: *r.(*RedBlackBottomUp)}
}
