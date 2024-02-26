package trees

import (
   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/utility/random"
   "math/bits"
)

type Zip struct {
   Tree
   random.Source
}

func (Zip) New() list.List {
   return &Zip{
      Source: random.New(random.Uint64()),
   }
}

func (tree *Zip) Clone() list.List {
   return &Zip{
      Tree:   tree.Tree.Clone(),
      Source: tree.Source, // TODO: copy?
   }
}

func (tree *Zip) randomRank() uint64 {
   return uint64(bits.LeadingZeros64(tree.Uint64()))
}

func (tree *Zip) unzip(p *Node, i list.Position, l, r **Node) {
   for p != nil {
      tree.persist(&p)
      if i <= p.s {
         *r = p
         p.s = p.s - i
         r = &p.l
         p = *r
      } else {
         *l = p
         i = i - p.s - 1
         l = &p.r
         p = *l
      }
   }
   *l = nil
   *r = nil
}

// When the new node's rank is greater than the rank of the current node,
// we know for sure that we can insert the new node at the current level.
//
// Otherwise, the new rank is less than or equal to the current rank.
//
//   When branching LEFT: if the ranks are equal, a split at the current
//   node would make it the right child of the new node, where an equal
//   rank would be valid. Keep searching if the new rank is less than.
//
//   When branching RIGHT: if the ranks are equal, a split at the current
//   node would make it the left child of the new node, where an equal
//   rank would NOT be valid.
func (tree *Zip) Insert(i list.Position, x list.Data) {
   tree.size++

   p := &tree.root                                      // parent, pointer
   n := tree.allocate(Node{x: x, y: tree.randomRank()}) // new node

   for *p != nil {
      if n.y > (*p).y { // New rank is greater, insert here.
         break
      }
      if i <= (*p).s {
         if n.y == (*p).y { // Branching left and ranks are equal.
            break
         }
         tree.persist(p)
         (*p).s = (*p).s + 1 // Increase the size of the left subtree.
         p = &(*p).l         // Path left.
      } else {
         tree.persist(p)
         i = i - ((*p).s + 1) // Skip the current node and left subtree.
         p = &(*p).r          // Path right.
      }
   }
   assert(tree.rank(n) >= tree.rank(*p))
   tree.unzip(*p, i, &n.l, &n.r) // Unzip the path into the new node.
   n.s = i                       // Set the size of the left subtree.
   *p = n                        // Write the new node to the path.
}

func (tree *Zip) rank(p *Node) uint64 {
   if p == nil {
      return 0
   } else {
      return p.y
   }
}

func (tree *Zip) zip(l, r *Node, sl list.Size) (root *Node) {
   assert(sl == l.size())
   p := &root
   for {
      if l == nil {
         *p = r
         return
      }
      if r == nil {
         *p = l
         return
      }

      if tree.rank(l) >= tree.rank(r) {
         tree.persist(&l)
         sl = sl - l.s - 1 //l.sizeR(sl)
         *p = l
         p = &l.r
         l = *p
      } else {
         tree.persist(&r)
         r.s = r.s + sl
         *p = r
         p = &r.l
         r = *p
      }
   }
}

func (tree *Zip) delete(p **Node, i list.Position, x *list.Data) {
   for {
      if i == (*p).s {
         defer tree.release(*p)
         tree.share((*p).l)
         tree.share((*p).r)
         *x = (*p).x
         *p = tree.zip((*p).l, (*p).r, (*p).s)
         return
      }
      tree.persist(p)
      if i < (*p).s {
         (*p).s = (*p).s - 1 // Decrease the size of the left subtree.
         p = &(*p).l         // Path left.
      } else {
         i = i - ((*p).s + 1) // Skip the current node and left subtree.
         p = &(*p).r          // Path right.
      }
   }
}

func (tree *Zip) Delete(i list.Position) (x list.Data) {
   assert(i < tree.size)
   tree.delete(&tree.root, i, &x)
   tree.size--
   return
}

func (tree Zip) split(i list.Size) (Tree, Tree) {
   assert(i <= tree.size)
   tree.share(tree.root)
   l, r := tree.Tree.split(tree.root, i)

   return Tree{pool: tree.pool, root: l, size: i},
          Tree{pool: tree.pool, root: r, size: tree.size - i}
}

func (tree *Zip) Split(i list.Position) (list.List, list.List) {
   l, r := tree.split(i)

   return &Zip{l, tree.Source},
      &Zip{r, tree.Source}
}

func (tree *Zip) Select(i list.Size) list.Data {
   assert(i < tree.size)
   return tree.lookup(tree.root, i)
}

func (tree *Zip) Update(i list.Size, x list.Data) {
   assert(i < tree.size)
   tree.persist(&tree.root)
   tree.update(tree.root, i, x)
}

func (tree *Zip) Join(that list.List) list.List {
   tree.share(tree.root)
   tree.share(that.(*Zip).root)

   root := tree.zip(tree.root, that.(*Zip).root, tree.size)
   size := tree.size + that.(*Zip).size

   return &Zip{Tree{pool: tree.pool, root: root, size: size}, tree.Source}
}

func (tree *Zip) verifyRanks(p *Node) {
   if p == nil {
      return
   }
   invariant(p.l == nil || p.y > p.l.y)
   invariant(p.r == nil || p.y > p.r.y || p.y == p.r.y)

   tree.verifyRanks(p.l)
   tree.verifyRanks(p.r)
}

func (tree *Zip) Verify() {
   tree.verifySizes()
   tree.verifyRanks(tree.root)
}