package list

import (
   "testing"
)

type Reference []Data

func (reference Reference) Assert(t *testing.T, instance List) {
   //
   // Check that both the reference and the instance report the same size.
   //
   if reference.Size() != instance.Size() {
      t.Fatalf("size does not match: expected %d, received %d", reference.Size(), instance.Size())
   }
   //
   // Check that every value of the instance matches the reference.
   //
   i := uint64(0)
   n := uint64(0)
   instance.Each(func(actual Data) {
      if expected := reference.Select(i); actual != expected {
         t.Fatalf("data does not match: expected %v at position %d, received %v", expected, i, actual)
      }
      i++
      n++
   })
   //
   // Check that the number of values checked matches the size of the reference.
   //
   if n != reference.Size() {
      t.Fatalf("iterator length does not match: expected %d, received %d", n, i)
   }
}

func (reference Reference) Free() {

}

func (reference Reference) New() List {
   return &Reference{}
}

func (reference Reference) Size() Size {
   return reference.Len()
}

func (reference Reference) Each(f func(Data)) {
   for _, data := range reference {
      f(data)
   }
}

func (reference Reference) Verify() {
}

func (reference Reference) Clone() List {
   return &reference
}

func (reference Reference) Len() uint64 {
   return uint64(len(reference))
}

// Returns the value at the given offset.
func (reference *Reference) Select(i uint64) (x Data) {
   return (*reference)[i]
}

// Updates the value at the given offset.
func (reference *Reference) Update(i uint64, x Data) {
   (*reference)[i] = x
}

// Insert at offset, increasing the length of the vector.
func (reference *Reference) Insert(i uint64, x Data) {
   *reference = append(*reference, 0); copy((*reference)[i+1:], (*reference)[i:]); (*reference)[i] = x
}

func (reference *Reference) Push(x Data) {
   *reference = append(*reference, x)
}

func (reference Reference) Empty() bool {
   return len(reference) == 0
}

func (reference *Reference) Pop() (x Data) {
   x = (*reference)[len(*reference) - 1]; *reference = (*reference)[:len(*reference) - 1]; return
}

// Delete and return at offset, decreasing the length of the vector.
func (reference *Reference) Delete(i uint64) (x Data) {
   x = (*reference)[i]; *reference = append((*reference)[:i], (*reference)[i+1:]...); return
}

// Split at offset, producing sequences [0,i) and [i,max).
func (reference Reference) Split(i uint64) (List, List) {
   l := make(Reference, i)
   r := make(Reference, len(reference) - int(i))
   copy(l, reference[:i])
   copy(r, reference[i:])
   return &l, &r
}

// Appends all values from another given vector to the receiver.
func (reference Reference) Join(other List) List {
   joined := make(Reference, Size(len(reference)) + other.Size())
   copy(joined, reference)
   copy(joined[len(reference):], *other.(*Reference))
   return &joined
}

func (reference Reference) Close()  {

}