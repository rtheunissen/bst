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

func (tree *RedBlackRelaxedBottomUp) Insert(i list.Position, x list.Data) {
   tree.RedBlackBottomUp.Insert(i, x)
}

func (tree *RedBlackRelaxedBottomUp) Delete(i list.Position) list.Data {
   return tree.Tree.Delete(i)
}

func (tree *RedBlackRelaxedBottomUp) Select(i list.Size) list.Data {
   return tree.Tree.Select(i)
}

func (tree *RedBlackRelaxedBottomUp) Update(i list.Size, x list.Data) {
   tree.Tree.Update(i, x)
}

func (tree RedBlackRelaxedBottomUp) join(l, r *Node, sl list.Size) (p *Node) {
   if l == nil { return r }
   if r == nil { return l }
   if tree.rank(l) < tree.rank(r) {
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

func (tree *RedBlackRelaxedBottomUp) Split(i list.Position) (list.List, list.List) {
   l, r := tree.RedBlackBottomUp.Split(i)
   return &RedBlackRelaxedBottomUp{RedBlackBottomUp: *l.(*RedBlackBottomUp)},
          &RedBlackRelaxedBottomUp{RedBlackBottomUp: *r.(*RedBlackBottomUp)}
}
