package trees

import "github.com/rtheunissen/bst/types/list"

type RedBlackRelaxedTopDown struct {
   RedBlackTopDown
   RedBlackRelaxed
}

func (tree RedBlackRelaxedTopDown) Verify() {
   tree.Tree.verifySize(tree.root, tree.size)
   tree.RedBlackRelaxed.verifyRanks(tree.root)
   tree.RedBlackRelaxed.verifyHeight(tree.root)
}

func (RedBlackRelaxedTopDown) New() list.List {
   return &RedBlackRelaxedTopDown{}
}

// TODO: make all clone syntax the exact same to avoid inconsistency in results
func (tree *RedBlackRelaxedTopDown) Clone() list.List {
   return &RedBlackRelaxedTopDown{
      RedBlackTopDown: *tree.RedBlackTopDown.Clone().(*RedBlackTopDown),
   }
}

func (tree *RedBlackRelaxedTopDown) Insert(i list.Position, x list.Data) {
   tree.RedBlackTopDown.Insert(i, x)
}

func (tree *RedBlackRelaxedTopDown) Delete(i list.Position) (x list.Data) {
  return tree.Tree.Delete(i)
}

func (tree *RedBlackRelaxedTopDown) Select(i list.Size) list.Data {
   return tree.Tree.Select(i)
}

func (tree *RedBlackRelaxedTopDown) Update(i list.Size, x list.Data) {
   tree.Tree.Update(i, x)
}

func (tree RedBlackRelaxedTopDown) join(l, r *Node, sl list.Size) (p *Node) {
   if l == nil { return r }
   if r == nil { return l }
   if tree.rank(l) < tree.rank(r) {
      return tree.build(l, tree.RedBlackTopDown.Tree.deleteMin(&r), r, sl)
   } else {
      return tree.build(l, tree.RedBlackTopDown.Tree.deleteMax(&l), r, sl-1)
   }
}

func (tree RedBlackRelaxedTopDown) Join(other list.List) list.List {
   return &RedBlackRelaxedTopDown{
      RedBlackTopDown: RedBlackTopDown{
         RedBlackBottomUp{
            Tree: tree.Tree.joinWith(other.(*RedBlackRelaxedTopDown).Tree, tree.join),
         },
      },
   }
}

func (tree RedBlackRelaxedTopDown) Split(i list.Position) (list.List, list.List) {
   l, r := tree.RedBlackTopDown.Split(i)
   return &RedBlackRelaxedTopDown{RedBlackTopDown: *l.(*RedBlackTopDown)},
          &RedBlackRelaxedTopDown{RedBlackTopDown: *r.(*RedBlackTopDown)}
}