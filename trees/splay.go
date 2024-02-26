package trees

import "github.com/rtheunissen/bst/types/list"

type Splay struct {
   Tree
}

func (tree Splay) New() list.List {
   return &Splay{}
}

func (tree *Splay) Clone() list.List {
   return &Splay{Tree: tree.Tree.Clone()}
}

func (tree *Splay) linkL(p *Node, r *Node, i list.Position) (*Node, *Node, list.Size) {
   tree.persist(&p.l)
 p.s = p.s - i - 1
 r.l = p
   r = r.l
   p = p.l
   return p, r, i
}

func (tree *Splay) linkR(p *Node, l *Node, i list.Position) (*Node, *Node, list.Size) {
   tree.persist(&p.r)
   i = i - p.s - 1
 l.r = p
   l = l.r
   p = p.r
   return p, l, i
}

func (tree *Splay) splay(p *Node, i list.Position) *Node {
   tree.persist(&p)
   n := Node{s: i}
   l := &n
   r := &n
   for i != p.s {
      if i < p.s {
         if i < p.l.s {
            //
            // ROTATE RIGHT, LINK LEFT
            //
            tree.persist(&p.l)
            tree.persist(&p.l.l)
            p = p.rotateR()
          p.s = p.s - i - 1
          r.l = p
            r = r.l
            p = p.l
         } else if i > p.l.s {
            //
            // LINK LEFT, LINK RIGHT
            //
            tree.persist(&p.l)
            tree.persist(&p.l.r)
          p.s = p.s - i - 1
          r.l = p
            r = r.l
            p = p.l
            i = i - p.s - 1
          l.r = p
            l = l.r
            p = p.r
         } else {
            //
            // LINK LEFT, BREAK
            //
            tree.persist(&p.l)
          p.s = p.s - i - 1
          r.l = p
            r = r.l
            p = p.l
            break
         }
      } else {
         if i > p.s + p.r.s + 1 {
            //
            // ROTATE LEFT, LINK RIGHT
            //
            tree.persist(&p.r)
            tree.persist(&p.r.r)
            p = p.rotateL()
            i = i - p.s - 1
          l.r = p
            l = l.r
            p = p.r
         } else if i < p.s + p.r.s + 1 {
            //
            // LINK RIGHT, LINK LEFT
            //
            tree.persist(&p.r)
            tree.persist(&p.r.l)
            i = i - p.s - 1
          l.r = p
            l = l.r
            p = p.r
          p.s = p.s - i - 1
          r.l = p
            r = r.l
            p = p.l
         } else {
            //
            // LINK RIGHT, BREAK
            //
            tree.persist(&p.r)
            i = i - p.s - 1
          l.r = p
            l = l.r
            p = p.r
            break
         }
      }
   }
   l.r = p.l
   r.l = p.r
   p.r = n.l
   p.l = n.r
   p.s = n.s
   return p
}

func (tree *Splay) Splay(i list.Position) {
   tree.root = tree.splay(tree.root, i)
}

func (tree *Splay) Size() list.Size {
   return tree.size
}

// 1. Splay the node at `i`
// 2. Return the root.
func (tree *Splay) Select(i list.Position) (x list.Data) {
   assert(i < tree.size)
   tree.Splay(i)
   return tree.root.x
}

// 1. Splay the node to be updated, at position `i`.
// 2. Update the root's data.
// 3. Return the root.
func (tree *Splay) Update(i list.Position, x list.Data) {
   assert(i < tree.size)
   tree.Splay(i)
   tree.root.x = x
}

//  1. Node a new node for the Data `s`.
//  2. Split the root into the left and right subtrees of the new node, such
//     that the first `i` nodes are on the left and the rest on the right.
//  3. Replace the previous root with the new node.
func (tree *Splay) Insert(i list.Position, x list.Data) {
   assert(i <= tree.size)
   //
   //
   if i == tree.size {
      tree.root = tree.allocate(Node{x: x, s: tree.size, l: tree.splayMax(tree.root)})
      tree.size++
      return
   }
   l, r := tree.split(tree.root, tree.size, i)
   tree.root = tree.allocate(Node{x: x, s: i, l: l, r: r})
   tree.size++
}

// 1. Splay the node to be deleted, making it the root.
// 2. Replace the root by a join of its left and right subtrees.
// 3. Return the deleted node.
func (tree *Splay) Delete(i list.Position) (x list.Data) {
   assert(i < tree.size)
   tree.Splay(i)
   defer tree.release(tree.root)
   x = tree.root.x
   tree.root = tree.join(tree.root.l, tree.root.r)
   tree.size--
   return
}

func (tree *Splay) Split(i list.Position) (list.List, list.List) {
   assert(i <= tree.size)
   tree.share(tree.root)

   if i == tree.size {
      return &Splay{Tree{pool: tree.pool, root: tree.root, size: tree.size}},
         &Splay{Tree{pool: tree.pool, root: nil, size: 0}}
   }
   //
   //
   l, r := tree.split(tree.root, tree.size, i)

   return &Splay{Tree{pool: tree.pool, root: l, size: i}},
      &Splay{Tree{pool: tree.pool, root: r, size: tree.size - i}}
}

// 1. Splay the node at `i`.
// 2. Cut the left subtree of the root as l**, leaving a nil in its place.
// 3. The remaining root and right subtree is r**.
func (tree Splay) split(p *Node, s, i list.Position) (l, r *Node) {
   assert(i < s)
   p = tree.splay(p, i)
   l = p.l
   r = p
   r.l = nil
   r.s = 0
   return l, r
}

func (tree *Splay) Join(that list.List) list.List { // TODO check if benchmarks are affected by poointer receivers here
   tree.share(tree.root)
   tree.share(that.(*Splay).root)
   return &Splay{Tree{pool: tree.pool, root: tree.join(tree.root, that.(*Splay).root), size: tree.Size() + that.Size()}}
}

// TODO: why always the max?
func (tree *Splay) splayMax(l *Node) *Node {
   if l == nil { // TODO is this ever nil?
      return nil
   }
   tree.persist(&l)
   for l.r != nil {
      if l.r.r != nil {
         tree.persist(&l.r)
         tree.persist(&l.r.r)
         l.r = l.r.rotateL()
         l = l.rotateL()
      } else {
         tree.persist(&l.r)
         l = l.rotateL()
      }
   }
   return l
}

// 1. Splay the right-most node of l*, which wouldn't have a right subtree.
// 2. Set the right subtree of the splayed node to r*.
// 3. Return the splayed node.
func (tree *Splay) join(l *Node, r *Node) *Node {
   if l == nil {
      return r
   }
   l = tree.splayMax(l)
   l.r = r
   return l
}

func (tree *Splay) Verify() {
   tree.verifySizes()
}
