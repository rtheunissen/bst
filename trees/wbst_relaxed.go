package trees

import "github.com/rtheunissen/bst/types/list"

type WBSTRelaxed struct {
   Tree
   WeightBalance
}

// Creates a new WBSTRelaxed BST from existing values.
func (WBSTRelaxed) New() list.List {
   return &WBSTRelaxed{
      WeightBalance: ThreeTwo{},
   }
}

func (tree *WBSTRelaxed) Clone() list.List {
   return &WBSTRelaxed{
      WeightBalance: tree.WeightBalance,
      Tree: tree.Tree.Clone(),
   }
}

func (tree *WBSTRelaxed) Verify() {
   tree.verifySizes()
}

func (tree *WBSTRelaxed) Delete(i list.Position) list.Data {
   return tree.Tree.Delete(i)
}

func (tree *WBSTRelaxed) insert(p **Node, s list.Size, i list.Position, x list.Data) {
   var unbalancedNode **Node    // An unbalanced node along the insertion path.
   var unbalancedSize list.Size // The size of the unbalanced node.
   var height int               // The height of the insertion so far.
   //
   // Search with increasing height until the end of the path is reached.
   //
   for {
      //
      // Attach a new node at the end of the path.
      //
      if *p == nil {
         *p = tree.allocate(Node{x: x})

         // Check if a rebuild is required.
         if height > tree.WeightBalance.maxHeight(tree.size) {
            tree.rebuild(unbalancedNode, unbalancedSize)
         }
         return
      }
      tree.persist(p)
      height++

      sl := (*p).sizeL()
      sr := (*p).sizeR(s)
      if i <= sl {
         if unbalancedNode == nil && !tree.isBalanced(sr, sl + 1) {
            unbalancedNode = p
            unbalancedSize = s + 1
         }
         p = insertL(*p)
         s = sl

      } else {
         if unbalancedNode == nil && !tree.isBalanced(sl, sr + 1) {
            unbalancedNode = p
            unbalancedSize = s + 1
         }
         p = insertR(*p, &i)
         s = sr
      }
   }
}

// Inserts a value `s` at position `i` in the tree.
func (tree *WBSTRelaxed) Insert(i list.Position, x list.Data) {
   assert(i <= tree.size)
   tree.size = tree.size + 1
   tree.insert(&tree.root, tree.size, i, x)
}

func (tree *WBSTRelaxed) balance(p *Node, s list.Size) *Node {
   if s < 4 {
      return p
   }
   if !tree.isBalanced(p.sizeL(), p.sizeR(s)) ||
      !tree.isBalanced(p.sizeR(s), p.sizeL()) {
      p = tree.partition(p, s >> 1)
   }
   p.l = tree.balance(p.l, p.sizeL())
   p.r = tree.balance(p.r, p.sizeR(s))
   return p
}

func (tree *WBSTRelaxed) rebuild(p **Node, s list.Size) {
   *p = tree.balance(*p, s)
}

func (tree *WBSTRelaxed) Split(i list.Size) (list.List, list.List) {
   assert(i <= tree.size)

   tree.share(tree.root)
   l,r := tree.split(tree.root, i)

   return &WBSTRelaxed{WeightBalance: tree.WeightBalance, Tree: Tree{pool: tree.pool, root: l, size: i}},
          &WBSTRelaxed{WeightBalance: tree.WeightBalance, Tree: Tree{pool: tree.pool, root: r, size: tree.size - i}}
}

func (tree *WBSTRelaxed) joinLr(l, r *Node, sl, sr list.Size) *Node {
   if r == nil {
      return l
   }
   if tree.isBalanced(sr, sl) { // TODO: wrong way around?
      p := tree.deleteMax(&l)
      p.l = l
      p.r = r
      p.s = sl - 1
      return p
   }
   tree.persist(&l)
   l.r = tree.join(l.r, r, sl - l.s - 1, sr)
   return l
}

func (tree *WBSTRelaxed) joinlR(l, r *Node, sl, sr list.Size) *Node {
   if l == nil {
      return r
   }
   if tree.isBalanced(sl, sr) { // TODO: wrong way around?
      p := tree.deleteMin(&r)
      p.l = l
      p.r = r
      p.s = sl
      return p
   }
   tree.persist(&r)
   r.l = tree.join(l, r.l, sl, r.s)
   r.s = sl + r.s
   return r
}

func (tree *WBSTRelaxed) join(l, r *Node, sl, sr list.Size) *Node {
   if sl > sr {
      return tree.joinLr(l, r, sl, sr)
   } else {
      return tree.joinlR(l, r, sl, sr)
   }
}

func (tree *WBSTRelaxed) Join(other list.List) list.List {
   tree.share(tree.root)
   tree.share(other.(*WBSTRelaxed).root)
   return &WBSTRelaxed{
      WeightBalance: tree.WeightBalance,
      Tree: Tree{
         pool: tree.pool,
         root: tree.join(tree.root, other.(*WBSTRelaxed).root, tree.size, other.(*WBSTRelaxed).size),
         size: tree.size + other.(*WBSTRelaxed).size,
      },
   }
}
