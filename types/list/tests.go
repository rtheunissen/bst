package list

import (
   "fmt"
   "github.com/rtheunissen/bst/utility"
   "github.com/rtheunissen/bst/utility/number"
   "github.com/rtheunissen/bst/utility/number/distribution"
   "github.com/rtheunissen/bst/utility/random"
   "testing"
)

type Test func(*testing.T, List, Size, number.Distribution, random.Source)

type TestSuite struct {
   Tests         []Test
   Lists         []List
   Scale         Size
   Distributions []number.Distribution
}

func (suite TestSuite) Run(t *testing.T) {
   for _, strategy := range suite.Lists {
      strategy := strategy
      t.Run(utility.NameOf(strategy), func(t *testing.T) {
         t.Parallel()
         for _, dist := range suite.Distributions {
            t.Run(utility.NameOf(dist), func(t *testing.T) {
               for _, test := range suite.Tests {
                  t.Run(utility.FuncName(test), func(t *testing.T) {
                     for scale := Size(0); scale <= suite.Scale; scale++ {
                        t.Run(fmt.Sprint(scale), func(t *testing.T) {
                           Seeded(t, func(rng random.Source) {
                              test(t, strategy, scale, dist.New(rng.Uint64()), rng)
                           })
                        })
                     }
                  })
               }
            })
         }
      })
   }
}

func assertEqual(expected, actual Data) {
   if actual != expected {
      panic(fmt.Sprintf("expected: %d, actual: %d", expected, actual))
   }
}

func assertList(reference List, actual List) {
   i := Position(0)
   n := reference.Size()
   actual.Each(func(x Data) {
      assertEqual(reference.Select(i), x)
      i++
   })
   assertEqual(n, i)
}

func referenceFor(instance List) *Reference {
   i := 0
   v := make(Reference, instance.Size())
   instance.Each(func(data Data) {
      v[i] = data
      i++
   })
   return &v
}

func referenceOfSize(size Size, rng random.Source) List {
   ref := make(Reference, size)
   for i := Size(0); i < size; i++ {
      ref[i] = rng.Uint64()
   }
   return &ref
}

func insertToSize(impl List, size Size, position number.Distribution, rng random.Source) List {
   instance := NewSeeded(impl, rng)
   for instance.Size() < size {
      instance.Insert(position.LessThan(instance.Size()+1), rng.Uint64())
   }
   return instance
}

func fromReference(impl List, reference Reference, rng random.Source) List {
   instance := NewSeeded(impl, rng)
   for _, x := range reference {
      instance.Insert(instance.Size(), x)
   }
   return instance
}

func TestDelete(t *testing.T, impl List, size Size, position number.Distribution, rng random.Source) {
   seq := insertToSize(impl, size, position, rng)
   ref := referenceFor(seq)
   for ref.Size() > 0 {
      testDeleteInPlace(ref, seq, position)
   }
   seq.Free()
}

func TestDeletePersistent(t *testing.T, impl List, size Size, position number.Distribution, rng random.Source) {
   seq := insertToSize(impl, size, position, rng)
   ref := referenceFor(seq)
   for ref.Size() > 0 {
      testPersistentDelete(ref, seq, position)
   }
   seq.Free()
}

func testDeleteInPlace(reference *Reference, sequence List, position number.Distribution) {
   i := position.LessThan(reference.Size())
   x := reference.Delete(i)
   assertEqual(x, sequence.Delete(i))
   sequence.Verify()
   assertList(reference, sequence)
}

func TestInsert(t *testing.T, impl List, size Size, position number.Distribution, rng random.Source) {
   seq := NewSeeded(impl, rng)
   ref := referenceFor(seq)
   for ref.Size() < size {
      testInsertInPlace(ref, seq, position, rng)
   }
   seq.Free()
}

func TestInsertPersistent(t *testing.T, impl List, size Size, position number.Distribution, rng random.Source) {
   seq := NewSeeded(impl, rng)
   ref := referenceFor(seq)
   for ref.Size() < size {
      testInsertPersistent(ref, seq, position, rng)
   }
   seq.Free()
}

func testInsertInPlace(reference *Reference, sequence List, position number.Distribution, rng random.Source) {
   i := position.LessThan(reference.Size() + 1)
   x := rng.Uint64()
   sequence.Insert(i, x)
   reference.Insert(i, x)
   sequence.Verify()
   assertList(reference, sequence)
}

func testInsertPersistent(reference *Reference, sequence List, position number.Distribution, rng random.Source) {
   snapshot := sequence.Clone()

   i := position.LessThan(reference.Size() + 1)
   x := rng.Uint64()

   sequence.Insert(i, x)
   sequence.Verify()
   snapshot.Verify()
   assertList(reference, snapshot)

   reference.Insert(i, x)
   assertList(reference, sequence)

   snapshot.Insert(i, x)
   snapshot.Verify()
   sequence.Verify()
   assertList(reference, snapshot)
   assertList(reference, sequence)
}

func testPersistentDelete(reference *Reference, sequence List, position number.Distribution) {
   snapshot := sequence.Clone()

   i := position.LessThan(reference.Size())
   x := sequence.Delete(i)

   sequence.Verify()
   snapshot.Verify()
   assertList(reference, snapshot)

   assertEqual(x, reference.Delete(i))
   assertList(reference, sequence)

   assertEqual(x, snapshot.Delete(i))
   snapshot.Verify()
   sequence.Verify()
   assertList(reference, snapshot)
   assertList(reference, sequence)
}

func TestInsertDelete(t *testing.T, impl List, size Size, position number.Distribution, rng random.Source) {
   seq := NewSeeded(impl, rng)
   ref := Reference{}
   for ref.Len() < size {
      testInsertInPlace(&ref, seq, position, rng)
      testInsertInPlace(&ref, seq, position, rng)
      testInsertInPlace(&ref, seq, position, rng)

      testDeleteInPlace(&ref, seq, distribution.Maximum{})
      testDeleteInPlace(&ref, seq, distribution.Maximum{})
   }
   seq.Free()
}

func TestInsertDeletePersistent(t *testing.T, impl List, size Size, position number.Distribution, rng random.Source) {
   seq := NewSeeded(impl, rng)
   ref := Reference{}
   for ref.Len() < size {
      testInsertPersistent(&ref, seq, position, rng)
      testInsertPersistent(&ref, seq, position, rng)
      testInsertPersistent(&ref, seq, position, rng)

      testPersistentDelete(&ref, seq, distribution.Uniform{}.New(rng.Uint64()))
      testPersistentDelete(&ref, seq, distribution.Uniform{}.New(rng.Uint64()))
   }
   seq.Free()
}

func TestJoinFromSplit(t *testing.T, impl List, size Size, distribution number.Distribution, rng random.Source) {
   instance := insertToSize(impl, size, distribution, rng)
   reference := referenceFor(instance)

   for i := Size(0); i < size; i++ {

      // Split the sequence and reference.
      L, R := instance.Split(i)

      // Join again, in various combinations.
      LL := L.Join(L)
      LR := L.Join(R)
      RL := R.Join(L)
      RR := R.Join(R)

      // Check invariants.
      LL.Verify()
      LR.Verify()
      RL.Verify()
      RR.Verify()

      // Check each join against a reference.
      _L, _R := reference.Split(i)

      assertList(_L.Join(_L), LL)
      assertList(_L.Join(_R), LR)
      assertList(_R.Join(_L), RL)
      assertList(_R.Join(_R), RR)
   }
   instance.Free()
}

func TestJoin(t *testing.T, impl List, size Size, distribution number.Distribution, rng random.Source) {
   if size == 0 {
      return
   }
   for i := Size(0); i < size; i++ {
      L := insertToSize(impl, rng.Uint64()%size, distribution, rng)
      R := insertToSize(impl, rng.Uint64()%size, distribution, rng)

      // Check each join against a reference.
      _L := referenceFor(L)
      _R := referenceFor(R)

      LL := L.Join(L)
      LR := L.Join(R)
      RL := R.Join(L)
      RR := R.Join(R)

      assertList(_L, L)
      assertList(_R, R)
      assertList(_L.Join(_L), LL)
      assertList(_L.Join(_R), LR)
      assertList(_R.Join(_L), RL)
      assertList(_R.Join(_R), RR)

      LL.Verify()
      LR.Verify()
      RL.Verify()
      RR.Verify()

      L.Verify()
      R.Verify()

      L.Free()
      R.Free()
   }
}

func TestJoinAfterInsertDelete(t *testing.T, impl List, size Size, distribution number.Distribution, rng random.Source) {
   L := NewSeeded(impl, rng)
   R := NewSeeded(impl, rng)

   // Check each join against a reference.
   _L := referenceFor(L)
   _R := referenceFor(R)

   for _L.Size() < size {

      testInsertInPlace(_L, L, distribution, rng)
      testInsertInPlace(_L, L, distribution, rng)
      testDeleteInPlace(_L, L, distribution)

      testInsertInPlace(_R, R, distribution, rng)
      testInsertInPlace(_R, R, distribution, rng)
      testDeleteInPlace(_R, R, distribution)

      LL := L.Join(L)
      LR := L.Join(R)
      RL := R.Join(L)
      RR := R.Join(R)

      // Check each join against a reference.
      assertList(_L.Join(_L), LL)
      assertList(_L.Join(_R), LR)
      assertList(_R.Join(_L), RL)
      assertList(_R.Join(_R), RR)

      // Check invariants.
      L.Verify()
      R.Verify()

      LL.Verify()
      LR.Verify()
      RL.Verify()
      RR.Verify()
   }
   L.Free()
   R.Free()
}

func TestSplit(t *testing.T, impl List, size Size, position number.Distribution, rng random.Source) {
   instance := insertToSize(impl, size, position, rng)
   reference := referenceFor(instance)

   for i := Size(0); i <= reference.Size(); i++ {

      // Split
      L, R := instance.Split(i)

      // Check invariants.
      instance.Verify()
      L.Verify()
      R.Verify()

      // Compare against a reference.
      _L, _R := reference.Split(i)
      assertList(_L, L)
      assertList(_R, R)
      assertList(reference, instance)

      L.Verify()
      R.Verify()

      // Delete all values from L and R after the split.
      for L.Size() > 0 { L.Delete(0) }
      for R.Size() > 0 { R.Delete(0) }

      instance.Verify()
      assertList(reference, instance)
   }
   instance.Free()
}

func TestSelect(t *testing.T, impl List, size Size, distribution number.Distribution, rng random.Source) {
   ref := referenceOfSize(size, rng).(*Reference)
   seq := fromReference(impl, *ref, rng)

   for s := size; s > 0; s-- {
      i := distribution.LessThan(ref.Size())

      assertEqual(ref.Select(i), seq.Select(i))
      seq.Verify()
      ref.Select(i)

      assertEqual(ref.Select(i), seq.Select(i))
      seq.Verify()
   }
   seq.Free()
}

func TestSelectPersistent(t *testing.T, impl List, size Size, distribution number.Distribution, rng random.Source) {
   ref := referenceOfSize(size, rng).(*Reference)
   seq := fromReference(impl, *ref, rng)

   for s := size; s > 0; s-- {
      i := distribution.LessThan(ref.Size())

      tmp := seq.Clone()

      assertEqual(ref.Select(i), tmp.Select(i))
      seq.Verify()
      tmp.Verify()

      ref.Select(i)

      assertEqual(ref.Select(i), seq.Select(i))
      seq.Verify()
      tmp.Verify()

      seq = tmp
   }
   seq.Free()
}

func TestSelectAfterInsert(t *testing.T, impl List, scale Size, distribution number.Distribution, rng random.Source) {
   instance := NewSeeded(impl, rng)
   reference := referenceFor(instance)
   for reference.Size() < scale {
      testInsertInPlace(reference, instance, distribution, rng)

      i := distribution.LessThan(reference.Size())
      assertEqual(reference.Select(i), instance.Select(i))
      instance.Verify()
   }
   instance.Free()
}

func TestSelectAfterInsertPersistent(t *testing.T, impl List, scale Size, distribution number.Distribution, rng random.Source) {
   instance := NewSeeded(impl, rng)
   reference := referenceFor(instance)
   for reference.Size() < scale {
      testInsertPersistent(reference, instance, distribution, rng)

      i := distribution.LessThan(reference.Size())
      assertEqual(reference.Select(i), instance.Select(i))
      instance.Verify()
   }
   instance.Free()
}

func TestUpdate(t *testing.T, impl List, size Size, distribution number.Distribution, rng random.Source) {
   seq := insertToSize(impl, size, distribution, rng)
   ref := referenceFor(seq)

   for s := size; s > 0; s-- {
      i := distribution.LessThan(ref.Size())
      x := rng.Uint64()

      seq.Update(i, x)
      ref.Update(i, x)
      seq.Verify()
      assertList(ref, seq)
   }
   seq.Free()
}

func TestUpdatePersistent(t *testing.T, impl List, size Size, distribution number.Distribution, rng random.Source) {
   ref := referenceOfSize(size, rng).(*Reference)
   seq := fromReference(impl, *ref, rng)

   for s := size; s > 0; s-- {
      i := distribution.LessThan(ref.Size())
      x := rng.Uint64()

      tmp := seq.Clone()
      tmp.Update(i, x)
      seq.Verify()
      assertList(ref, seq)

      ref.Update(i, x)
      tmp.Verify()
      assertList(ref, tmp)
      seq = tmp
   }
   seq.Free()
}

// TestPersistentVersions keeps several versions alive at the same time, each
// derived from another by a random operation, either in-place or on a copy.
// Every version is compared to its own reference after every operation, so an
// operation on one version that modifies another version is detected, even if
// the two versions only share some of their nodes.
func TestPersistentVersions(t *testing.T, impl List, size Size, position number.Distribution, rng random.Source) {
   const capacity = 4
   if size == 0 {
      return
   }
   //
   // Every version allocates from the memory of the first version, which is
   // allocated before any other version is derived from it, so that all of the
   // versions can be freed together by freeing the first version.
   //
   first := NewSeeded(impl, rng)
   first.Insert(0, 0)
   versions := []List{first}
   references := []*Reference{{0}}

   for step := Size(0); step < size; step++ {
      k := random.LessThan(uint64(len(versions)), rng)
      version := versions[k]
      reference := references[k]
      //
      // Either modify the version in-place, or modify a copy as a new version.
      //
      copied := rng.Uint64() & 1 == 0
      if copied {
         version = version.Clone()
         reference = reference.Clone().(*Reference)
      }
      switch n := reference.Size(); random.LessThan(6, rng) {
      case 0, 1:
         if n < 2*size {
            i := position.LessThan(n + 1)
            x := rng.Uint64()
            version.Insert(i, x)
            reference.Insert(i, x)
         }
      case 2:
         if n > 0 {
            i := position.LessThan(n)
            assertEqual(reference.Delete(i), version.Delete(i))
         }
      case 3:
         if n > 0 {
            i := position.LessThan(n)
            x := rng.Uint64()
            version.Update(i, x)
            reference.Update(i, x)
         }
      case 4:
         i := position.LessThan(n + 1)
         L, R := version.Split(i)
         _L, _R := reference.Split(i)
         version, reference = L, _L.(*Reference)
         versions = append(versions, R)
         references = append(references, _R.(*Reference))
      case 5:
         j := random.LessThan(uint64(len(versions)), rng)
         if n + references[j].Size() <= 2*size {
            version = version.Join(versions[j])
            reference = reference.Join(references[j]).(*Reference)
         }
      }
      if copied {
         versions = append(versions, version)
         references = append(references, reference)
      } else {
         versions[k] = version
         references[k] = reference
      }
      //
      // Discard random versions to stay within capacity.
      //
      for len(versions) > capacity {
         j := random.LessThan(uint64(len(versions)), rng)
         versions = append(versions[:j], versions[j+1:]...)
         references = append(references[:j], references[j+1:]...)
      }
      version.Verify()
      for j := range versions {
         assertList(references[j], versions[j])
      }
   }
   for j := range versions {
      versions[j].Verify()
      assertList(references[j], versions[j])
   }
   first.Free()
}
