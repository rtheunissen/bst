package trees

import (
   "testing"

   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/utility"
   "github.com/rtheunissen/bst/utility/random"
)

// A fingerprint records every node that is reachable from the root of a tree,
// along with all of its fields other than its reference count. Comparing the
// fingerprints of a tree before and after an operation on another tree detects
// any modification of the tree by that operation, even one that preserves the
// values of the tree, such as a change of rank or of shape.
type fingerprint []struct {
   p, l, r *Node
   s       Size
   x       Data
   y       Rank
}

func fingerprintOf(instance list.List) (f fingerprint) {
   var visit func(p *Node)
   visit = func(p *Node) {
      if p == nil {
         return
      }
      f = append(f, struct {
         p, l, r *Node
         s       Size
         x       Data
         y       Rank
      }{p, p.l, p.r, p.s, p.x, p.y})
      visit(p.l)
      visit(p.r)
   }
   visit(instance.(interface{ Root() *Node }).Root())
   return
}

func (f fingerprint) equal(other fingerprint) bool {
   if len(f) != len(other) {
      return false
   }
   for i := range f {
      if f[i] != other[i] {
         return false
      }
   }
   return true
}

// Modifies a list in-place by a random operation at a random position.
func modify(instance list.List, rng random.Source) list.List {
   n := instance.Size()
   switch random.LessThan(7, rng) {
   case 0, 1, 2:
      instance.Insert(random.LessThan(n + 1, rng), rng.Uint64())
   case 3, 4:
      if n > 0 {
         instance.Delete(random.LessThan(n, rng))
      }
   case 5:
      if n > 0 {
         instance.Update(random.LessThan(n, rng), rng.Uint64())
      }
   case 6:
      l, r := instance.Split(random.LessThan(n + 1, rng))
      return l.Join(r)
   }
   return instance
}

// TestPersistenceNeverShared checks that a tree that never shares its nodes
// never marks any of them as shared, because shared nodes are copied when they
// are modified, which would be unnecessary for a tree that is never shared.
func TestPersistenceNeverShared(t *testing.T) {
   for _, impl := range implementations {
      impl := impl
      t.Run(utility.NameOf(impl), func(t *testing.T) {
         t.Parallel()
         list.Seeded(t, func(rng random.Source) {
            instance := list.NewSeeded(impl, rng)
            for step := 0; step < 5000; step++ {
               n := instance.Size()
               switch random.LessThan(5, rng) {
               case 0, 1:
                  instance.Insert(random.LessThan(n + 1, rng), rng.Uint64())
               case 2, 3:
                  if n > 0 {
                     instance.Delete(random.LessThan(n, rng))
                  }
               case 4:
                  if n > 0 {
                     instance.Update(random.LessThan(n, rng), rng.Uint64())
                  }
               }
            }
            var visit func(p *Node)
            visit = func(p *Node) {
               if p == nil {
                  return
               }
               if p.refs > 0 {
                  t.Fatalf("a node is marked as shared but the tree was never shared")
               }
               visit(p.l)
               visit(p.r)
            }
            visit(instance.(interface{ Root() *Node }).Root())
            instance.Free()
         })
      })
   }
}

// TestPersistenceSnapshots modifies a tree in-place while it shares its nodes
// with many snapshots taken along the way, and also modifies copies of those
// snapshots, then checks that no snapshot was modified in any way.
func TestPersistenceSnapshots(t *testing.T) {
   for _, impl := range implementations {
      impl := impl
      t.Run(utility.NameOf(impl), func(t *testing.T) {
         t.Parallel()
         list.Seeded(t, func(rng random.Source) {
            live := list.NewSeeded(impl, rng)

            var snapshots []list.List
            var fingerprints []fingerprint

            for step := 0; step < 2000; step++ {
               if step % 25 == 0 {
                  snapshot := live.Clone()
                  snapshots = append(snapshots, snapshot)
                  fingerprints = append(fingerprints, fingerprintOf(snapshot))
               }
               live = modify(live, rng)
               //
               // Modify a copy of a random snapshot.
               //
               if step % 10 == 0 {
                  branch := snapshots[random.LessThan(uint64(len(snapshots)), rng)].Clone()
                  for k := 0; k < 5; k++ {
                     branch = modify(branch, rng)
                  }
                  branch.Verify()
               }
               if step % 25 == 24 {
                  for k := range snapshots {
                     if !fingerprints[k].equal(fingerprintOf(snapshots[k])) {
                        t.Fatalf("snapshot %d of %d was modified by step %d", k, len(snapshots), step)
                     }
                  }
               }
            }
            live.Verify()
            for _, snapshot := range snapshots {
               snapshot.Verify()
            }
            live.Free() // Every version shares the memory of the live tree.
         })
      })
   }
}

// TestPersistenceJoinAndSplit checks that neither join nor split modifies the
// trees that they are given, including when a tree is joined with itself, and
// that their results remain valid when they are modified in-place.
func TestPersistenceJoinAndSplit(t *testing.T) {
   for _, impl := range implementations {
      impl := impl
      t.Run(utility.NameOf(impl), func(t *testing.T) {
         t.Parallel()
         list.Seeded(t, func(rng random.Source) {
            //
            // Every result allocates from the memory of the left tree, which is
            // not empty, so both trees can be freed after every trial.
            //
            grow := func() list.List {
               instance := list.NewSeeded(impl, rng)
               for n := 1 + random.LessThan(64, rng); instance.Size() < n; {
                  instance = modify(instance, rng)
               }
               return instance
            }
            for trial := 0; trial < 200; trial++ {
               L := grow()
               R := grow()

               l := fingerprintOf(L)
               r := fingerprintOf(R)

               LR := L.Join(R)
               LL := L.Join(L)
               A, B := L.Split(random.LessThan(L.Size() + 1, rng))

               for _, result := range []list.List{LR, LL, A, B} {
                  result.Verify()
                  for k := 0; k < 5; k++ {
                     result = modify(result, rng)
                  }
                  result.Verify()
               }
               if !l.equal(fingerprintOf(L)) || !r.equal(fingerprintOf(R)) {
                  t.Fatalf("trial %d: join or split modified a tree that it was given", trial)
               }
               L.Verify()
               R.Verify()
               L.Free()
               R.Free()
            }
         })
      })
   }
}
