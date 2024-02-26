package list

import (
   "fmt"
   "github.com/rtheunissen/bst/utility"
   "github.com/rtheunissen/bst/utility/number"
   "github.com/rtheunissen/bst/utility/number/distribution"
   "github.com/rtheunissen/bst/utility/random"
   "testing"
)

type Test func(*testing.T, List, Size, number.Distribution)

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
                           test(t, strategy, scale, dist.New(random.Uint64()))
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

func referenceOfSize(size Size) List {
   ref := make(Reference, size)
   for i := Size(0); i < size; i++ {
      ref[i] = random.Uint64()
   }
   return &ref
}

func insertToSize(impl List, size Size, position number.Distribution) List {
   instance := impl.New()
   for instance.Size() < size {
      instance.Insert(position.LessThan(instance.Size()+1), random.Uint64())
   }
   return instance
}

func fromReference(impl List, reference Reference) List {
   instance := impl.New()
   for _, x := range reference {
      instance.Insert(instance.Size(), x)
   }
   return instance
}

func TestDelete(t *testing.T, impl List, size Size, position number.Distribution) {
   seq := insertToSize(impl, size, position)
   ref := referenceFor(seq)
   for ref.Size() > 0 {
      testDeleteInPlace(ref, seq, position)
   }
   seq.Free()
}

func TestDeletePersistent(t *testing.T, impl List, size Size, position number.Distribution) {
   seq := insertToSize(impl, size, position)
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

func TestInsert(t *testing.T, impl List, size Size, position number.Distribution) {
   seq := impl.New()
   ref := referenceFor(seq)
   for ref.Size() < size {
      testInsertInPlace(ref, seq, position)
   }
   seq.Free()
}

func TestInsertPersistent(t *testing.T, impl List, size Size, position number.Distribution) {
   seq := impl.New()
   ref := referenceFor(seq)
   for ref.Size() < size {
      testInsertPersistent(ref, seq, position)
   }
   seq.Free()
}

func testInsertInPlace(reference *Reference, sequence List, position number.Distribution) {
   i := position.LessThan(reference.Size() + 1)
   x := random.Uint64()
   sequence.Insert(i, x)
   reference.Insert(i, x)
   sequence.Verify()
   assertList(reference, sequence)
}

func testInsertPersistent(reference *Reference, sequence List, position number.Distribution) {
   snapshot := sequence.Clone()

   i := position.LessThan(reference.Size() + 1)
   x := random.Uint64()

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

func TestInsertDelete(t *testing.T, impl List, size Size, position number.Distribution) {
   seq := impl.New()
   ref := Reference{}
   for ref.Len() < size {
      testInsertInPlace(&ref, seq, position)
      testInsertInPlace(&ref, seq, position)
      testInsertInPlace(&ref, seq, position)

      testDeleteInPlace(&ref, seq, distribution.Maximum{})
      testDeleteInPlace(&ref, seq, distribution.Maximum{})
   }
   seq.Free()
}

func TestInsertDeletePersistent(t *testing.T, impl List, size Size, position number.Distribution) {
   seq := impl.New()
   ref := Reference{}
   for ref.Len() < size {
      testInsertPersistent(&ref, seq, position)
      testInsertPersistent(&ref, seq, position)
      testInsertPersistent(&ref, seq, position)

      testPersistentDelete(&ref, seq, &distribution.Uniform{random.Uniform()}) // TODO: simplify this
      testPersistentDelete(&ref, seq, &distribution.Uniform{random.Uniform()}) // TODO: simplify this
   }
   seq.Free()
}

func TestJoinFromSplit(t *testing.T, impl List, size Size, distribution number.Distribution) {
   instance := insertToSize(impl, size, distribution)
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

func TestJoin(t *testing.T, impl List, size Size, distribution number.Distribution) {
   if size == 0 {
      return
   }
   for i := Size(0); i < size; i++ {
      L := insertToSize(impl, random.Uint64()%size, distribution)
      R := insertToSize(impl, random.Uint64()%size, distribution)

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

func TestJoinAfterInsertDelete(t *testing.T, impl List, size Size, distribution number.Distribution) {
   L := impl.New()
   R := impl.New()

   // Check each join against a reference.
   _L := referenceFor(L)
   _R := referenceFor(R)

   for _L.Size() < size {

      testInsertInPlace(_L, L, distribution)
      testInsertInPlace(_L, L, distribution)
      testDeleteInPlace(_L, L, distribution)

      testInsertInPlace(_R, R, distribution)
      testInsertInPlace(_R, R, distribution)
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

func TestSplit(t *testing.T, impl List, size Size, position number.Distribution) {
   instance := insertToSize(impl, size, position)
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

func TestSelect(t *testing.T, impl List, size Size, distribution number.Distribution) {
   ref := referenceOfSize(size).(*Reference)
   seq := fromReference(impl, *ref)

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

func TestSelectPersistent(t *testing.T, impl List, size Size, distribution number.Distribution) {
   ref := referenceOfSize(size).(*Reference)
   seq := fromReference(impl, *ref)

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

func TestSelectAfterInsert(t *testing.T, impl List, scale Size, distribution number.Distribution) {
   instance := impl.New()
   reference := referenceFor(instance)
   for reference.Size() < scale {
      testInsertInPlace(reference, instance, distribution)

      i := distribution.LessThan(reference.Size())
      assertEqual(reference.Select(i), instance.Select(i))
      instance.Verify()
   }
   instance.Free()
}

func TestSelectAfterInsertPersistent(t *testing.T, impl List, scale Size, distribution number.Distribution) {
   instance := impl.New()
   reference := referenceFor(instance)
   for reference.Size() < scale {
      testInsertPersistent(reference, instance, distribution)

      i := distribution.LessThan(reference.Size())
      assertEqual(reference.Select(i), instance.Select(i))
      instance.Verify()
   }
   instance.Free()
}

func TestUpdate(t *testing.T, impl List, size Size, distribution number.Distribution) {
   seq := insertToSize(impl, size, distribution)
   ref := referenceFor(seq)

   for s := size; s > 0; s-- {
      i := distribution.LessThan(ref.Size())
      x := random.Uint64()

      seq.Update(i, x)
      ref.Update(i, x)
      seq.Verify()
   }
   seq.Free()
}

func TestUpdatePersistent(t *testing.T, impl List, size Size, distribution number.Distribution) {
   ref := referenceOfSize(size).(*Reference)
   seq := fromReference(impl, *ref)

   for s := size; s > 0; s-- {
      x := distribution.LessThan(ref.Size())
      v := random.Uint64()

      tmp := seq.Clone()
      tmp.Update(x, v)
      seq.Verify()

      ref.Update(x, v)
      tmp.Verify()
      seq = tmp
   }
   seq.Free()
}
