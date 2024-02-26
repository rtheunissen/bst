package trees

import (
   "golang.org/x/exp/constraints"
)

func measurement[T constraints.Integer | constraints.Float](addr *T, delta T) {
   *addr = *addr + delta
}

type Measurement interface {
   Reset()
   Measure(BinaryTree) any
}

type PartitionCount struct {

}

var partitionCount uint64 = 0

func (PartitionCount) Reset()  {
   partitionCount = 0
}

func (PartitionCount) Measure(BinaryTree) any {
   return partitionCount
}

type PartitionDepth struct {

}
var partitionDepth uint64 = 0

func (PartitionDepth) Reset()  {
   partitionDepth = 0
}

func (PartitionDepth) Measure(BinaryTree) any {
   return partitionDepth
}

type MaximumPathLength struct {

}

func (MaximumPathLength) Reset()  {
}

func (MaximumPathLength) Measure(tree BinaryTree) any {
   return tree.Root().MaximumPathLength()
}

type AveragePathLength struct {
}

func (accumulator *AveragePathLength) Measure(tree BinaryTree) any {
   return tree.Root().AveragePathLength()
}
func (AveragePathLength) Reset()  {
}
var allocations uint64 = 0

type Allocations struct {
}

func (Allocations) Reset()  {
   allocations = 0
}

func (Allocations) Measure(BinaryTree) any {
   return allocations
}

var rotations uint64 = 0

type Rotations struct {
}

func (accumulator *Rotations) Reset()  {
   rotations = 0
}

func (accumulator *Rotations) Measure(BinaryTree) any {
   return rotations
}