package trees

import "github.com/rtheunissen/bst/types/list"

type RedBlackTopDown struct {
   RedBlackBottomUp // TODO: borrowing some things, implement top-down delete
}

func (RedBlackTopDown) New() list.List {
   return &RedBlackTopDown{}
}

func (tree *RedBlackTopDown) Clone() list.List {
   return &RedBlackTopDown{
      RedBlackBottomUp: *tree.RedBlackBottomUp.Clone().(*RedBlackBottomUp),
   }
}

func (tree *RedBlackTopDown) insert(p **Node, i list.Position, x list.Data) {
   //
   // "If the tree is empty, create a new node of rank zero containing the item
   //  to be inserted and make it the root, completing the insertion."
   //
   if *p == nil {
      tree.attach(p, x)
      return
   }
   tree.persist(p)
   //
   // "Otherwise, promote the root if 0,0."
   //
   if tree.isRedRed(*p) {
      tree.promote(*p)
   }
   //
   // This establishes the invariant for the main loop of the algorithm:
   // *p is a non-nil node that is not a 0,0-node and not a 0-child.
   //
   // The current node is black and has at least one black child.
   //
   for {
      assert(!tree.isRedRed(*p))
      //
      // "From *p, take one step down the search path..."
      //
      if i <= (*p).s {
         //
         // LEFT
         //
         if (*p).l == nil {
            tree.attachL(*p, x)
            return
         }
         // A black node with a black child is reached.
         if tree.isRedRed((*p).l) {
            tree.pathLeft(&p)
            tree.promote(*p)
            continue
         }
         if !tree.isRed(*p, (*p).l) {
             tree.pathLeft(&p)
             continue
         }
         //
         // In the remaining cases, y is a 0-child, and hence neither of its children is a 0-child
         //
         assert(tree.isRed(*p, (*p).l))
         assert(!tree.isRedRed((*p).l))

         if i <= (*p).l.s {
            if (*p).l.l == nil {
               tree.attachLL(*p, x)
               if tree.isRed((*p).l, (*p).l.l) {
                  tree.rotateR(p)
               }
               return
            }
            if !tree.isBlack((*p).l, (*p).l.l) {
                tree.pathLeft(&p)
                tree.pathLeft(&p)
                tree.promote(*p)
                continue
            }
            if !tree.isRedRed((*p).l.l) {
                tree.pathLeft(&p)
                tree.pathLeft(&p)
                continue
            }
            tree.rotateR(p)
            tree.pathLeft(&p)
            tree.promote(*p)
            continue

         } else {
            //
            // LEFT RIGHT
            //
            if (*p).l.r == nil {
               tree.attachLR(*p, x)
               if tree.isRed((*p).l, (*p).l.r) { // or is p.l.rank == 0 ?
                  tree.rotateLR(p)
               }
               return
            }
            if !tree.isRedRed((*p).l.r) {
                tree.pathLeft(&p)
                tree.pathRight(&p, &i)
                continue
            }
            if !tree.isBlack((*p).l, (*p).l.r) {
                tree.pathLeft(&p)
                tree.pathRight(&p, &i)
                tree.promote(*p)
                continue
            }
            tree.rotateLR(p)
            tree.promote(*p)
            if i <= (*p).s {
               tree.pathLeft(&p) // LRL
            } else {
               tree.pathRight(&p, &i) // LRR
            }
         }
      } else {
         if (*p).r == nil {
            tree.attachR(*p, x)
            return
         }
         if !tree.isRedRed((*p).r) && !tree.isRed(*p, (*p).r) {
             tree.pathRight(&p, &i)
             continue
         }
         if tree.isRedRed((*p).r) {
            tree.pathRight(&p, &i)
            tree.promote(*p)
            continue
         }
         if i > (*p).s + (*p).r.s + 1 {
            if (*p).r.r == nil {
               tree.attachRR(*p, x)
               if tree.isRed((*p).r, (*p).r.r) { // or is p.r.rank == 0 ?
                  tree.rotateL(p)
               }
               return
            }
            if !tree.isRedRed((*p).r.r) {
                tree.pathRight(&p, &i)
                tree.pathRight(&p, &i)
                continue
            }
            if !tree.isOneChild((*p).r, (*p).r.r) {
                tree.pathRight(&p, &i)
                tree.pathRight(&p, &i)
                tree.promote(*p)
                continue
            }
            tree.rotateL(p)
            tree.pathRight(&p, &i)
            tree.promote(*p)
            continue
         } else {
            //
            // RIGHT LEFT
            //
            if (*p).r.l == nil {
               tree.attachRL(*p, x)
               if tree.isRed((*p).r, (*p).r.l) { // or is p.l.rank == 0 ?
                  tree.rotateRL(p)
               }
               return
            }
            if !tree.isRedRed((*p).r.l) {
                tree.pathRight(&p, &i)
                tree.pathLeft(&p)
                continue
            }
            if !tree.isBlack((*p).r, (*p).r.l) {
                tree.pathRight(&p, &i)
                tree.pathLeft(&p)
                tree.promote(*p)
                continue
            }
            tree.rotateRL(p)
            tree.promote(*p)
            if i > (*p).s {
               tree.pathRight(&p, &i) // RLR
            } else {
               tree.pathLeft(&p) // RLL
            }
         }
      }
   }
}

func (tree *RedBlackTopDown) Insert(i list.Position, x list.Data) {
   assert(i <= tree.size)
   tree.size = tree.size + 1
   tree.insert(&tree.root, i, x)
}

// TODO: Top-down split?
func (tree *RedBlackTopDown) Split(i list.Position) (list.List, list.List) {
   l, r := tree.RedBlackBottomUp.Split(i)
   return &RedBlackTopDown{RedBlackBottomUp: *l.(*RedBlackBottomUp)},
          &RedBlackTopDown{RedBlackBottomUp: *r.(*RedBlackBottomUp)}
}

func (tree *RedBlackTopDown) Join(other list.List) list.List {
   return &RedBlackTopDown{
      RedBlackBottomUp: *tree.RedBlackBottomUp.Join(&other.(*RedBlackTopDown).RedBlackBottomUp).(*RedBlackBottomUp),
   }
}
