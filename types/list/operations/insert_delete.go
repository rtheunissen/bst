package operations

import (
   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/utility/number"
)

type InsertDelete struct {
   Scale list.Size   // The maximum size of the list.
   Steps list.Size   // The number of steps to take in total.
   steps list.Size   // The number of steps taken so far.
}

func (operation *InsertDelete) New() list.Operation {
   return &InsertDelete{
      Scale: operation.Scale,
      Steps: operation.Steps,
      steps: 0,
   }
}

func (operation *InsertDelete) Range() list.Size {
   return operation.Steps
}

func (operation *InsertDelete) Valid(instance list.List) bool {
   return operation.steps <= operation.Steps
}

func (operation *InsertDelete) Update(instance list.List, position number.Distribution) (list.List, list.Position) {
   operation.steps++
   //
   // Keep inserting until the scale is reached, then alternate
   // between insertion and reflected deletion.
   //
   if instance.Size() < operation.Scale || operation.steps % 2 == 0 {
      i := position.LessThan(instance.Size() + 1)
      instance.Insert(i, 0)
      return instance, i
   } else {
      i := position.LessThan(instance.Size())
      instance.Delete(instance.Size() - i - 1)
      return instance, i
   }

}

type InsertDeletePersistent InsertDelete

func (operation *InsertDeletePersistent) New() list.Operation {
   return (*InsertDelete)(operation).New()
}

func (operation *InsertDeletePersistent) Range() list.Size {
   return (*InsertDelete)(operation).Range()
}

func (operation *InsertDeletePersistent) Valid(instance list.List) bool {
   return (*InsertDelete)(operation).Valid(instance)
}

func (operation *InsertDeletePersistent) Update(instance list.List, dist number.Distribution) (list.List, list.Position) {
   return (*InsertDelete)(operation).Update(instance.Clone(), dist)
}
