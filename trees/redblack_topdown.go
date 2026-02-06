package trees

import "github.com/rtheunissen/bst/types/list"

type RedBlackTopDown struct {
   RedBlackBottomUp
}

func (RedBlackTopDown) New() list.List {
   return &RedBlackTopDown{}
}

func (tree *RedBlackTopDown) Clone() list.List {
   return &RedBlackTopDown{
      RedBlackBottomUp: *tree.RedBlackBottomUp.Clone().(*RedBlackBottomUp),
   }
}

func (tree *RedBlackTopDown) insert(p **Node, i Position, x Data) {
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
   if isRedRed(*p) {
      promote(*p)
   }
   //
   // This establishes the invariant for the main loop of the algorithm:
   // *p is a non-nil node that is not a 0,0-node and not a 0-child.
   //
   // The current node is black and has at least one black child.
   //
   for {
      assert(!isRedRed(*p))
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
         if isRedRed((*p).l) {
            tree.pathLeft(&p)
            promote(*p)
            continue
         }
         if !isRed(*p, (*p).l) {
             tree.pathLeft(&p)
             continue
         }
         //
         // In the remaining cases, y is a 0-child, and hence neither of its children is a 0-child
         //
         assert(isRed(*p, (*p).l))
         assert(!isRedRed((*p).l))

         if i <= (*p).l.s {
            if (*p).l.l == nil {
               tree.attachLL(*p, x)
               if isRed((*p).l, (*p).l.l) {
                  tree.treeRotateR(p)
               }
               return
            }
            if !isBlack((*p).l, (*p).l.l) {
                tree.pathLeft(&p)
                tree.pathLeft(&p)
                promote(*p)
                continue
            }
            if !isRedRed((*p).l.l) {
                tree.pathLeft(&p)
                tree.pathLeft(&p)
                continue
            }
            tree.treeRotateR(p)
            tree.pathLeft(&p)
            promote(*p)
            continue

         } else {
            //
            // LEFT RIGHT
            //
            if (*p).l.r == nil {
               tree.attachLR(*p, x)
               if isRed((*p).l, (*p).l.r) { // or is p.l.rank == 0 ?
                  tree.treeRotateLR(p)
               }
               return
            }
            if !isRedRed((*p).l.r) {
                tree.pathLeft(&p)
                tree.pathRight(&p, &i)
                continue
            }
            if !isBlack((*p).l, (*p).l.r) {
                tree.pathLeft(&p)
                tree.pathRight(&p, &i)
                promote(*p)
                continue
            }
            tree.treeRotateLR(p)
            promote(*p)
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
         if !isRedRed((*p).r) && !isRed(*p, (*p).r) {
             tree.pathRight(&p, &i)
             continue
         }
         if isRedRed((*p).r) {
            tree.pathRight(&p, &i)
            promote(*p)
            continue
         }
         if i > (*p).s + (*p).r.s + 1 {
            if (*p).r.r == nil {
               tree.attachRR(*p, x)
               if isRed((*p).r, (*p).r.r) { // or is p.r.rank == 0 ?
                  tree.treeRotateL(p)
               }
               return
            }
            if !isRedRed((*p).r.r) {
                tree.pathRight(&p, &i)
                tree.pathRight(&p, &i)
                continue
            }
            if !isOneChild((*p).r, (*p).r.r) {
                tree.pathRight(&p, &i)
                tree.pathRight(&p, &i)
                promote(*p)
                continue
            }
            tree.treeRotateL(p)
            tree.pathRight(&p, &i)
            promote(*p)
            continue
         } else {
            //
            // RIGHT LEFT
            //
            if (*p).r.l == nil {
               tree.attachRL(*p, x)
               if isRed((*p).r, (*p).r.l) { // or is p.l.rank == 0 ?
                  tree.treeRotateRL(p)
               }
               return
            }
            if !isRedRed((*p).r.l) {
                tree.pathRight(&p, &i)
                tree.pathLeft(&p)
                continue
            }
            if !isBlack((*p).r, (*p).r.l) {
                tree.pathRight(&p, &i)
                tree.pathLeft(&p)
                promote(*p)
                continue
            }
            tree.treeRotateRL(p)
            promote(*p)
            if i > (*p).s {
               tree.pathRight(&p, &i) // RLR
            } else {
               tree.pathLeft(&p) // RLL
            }
         }
      }
   }
}

func (tree *RedBlackTopDown) Insert(i Position, x Data) {
   assert(i <= tree.size)
   tree.size = tree.size + 1
   tree.insert(&tree.root, i, x)
}

func (tree *RedBlackTopDown) Split(i Position) (list.List, list.List) {
   l, r := tree.RedBlackBottomUp.Split(i)
   return &RedBlackTopDown{RedBlackBottomUp: *l.(*RedBlackBottomUp)},
          &RedBlackTopDown{RedBlackBottomUp: *r.(*RedBlackBottomUp)}
}

func (tree *RedBlackTopDown) Join(other list.List) list.List {
   return &RedBlackTopDown{
      RedBlackBottomUp: *tree.RedBlackBottomUp.Join(&other.(*RedBlackTopDown).RedBlackBottomUp).(*RedBlackBottomUp),
   }
}
