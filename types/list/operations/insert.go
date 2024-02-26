package operations

import (
   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/utility/number"
)

type Insert struct {
   Scale list.Size
}

func (operation *Insert) New() list.Operation {
   return &Insert{
      Scale: operation.Scale,
   }
}

func (operation *Insert) Range() list.Size {
   return operation.Scale
}

func (operation *Insert) Valid(instance list.List) bool {
   return instance.Size() < operation.Scale
}

func (operation *Insert) Update(instance list.List, dist number.Distribution) (list.List, list.Position) {
   i := dist.LessThan(instance.Size() + 1)
   instance.Insert(i, 0)
   return instance, i
}

type InsertPersistent Insert

func (operation *InsertPersistent) New() list.Operation {
   return (*Insert)(operation).New()
}

func (operation *InsertPersistent) Range() list.Size {
   return (*Insert)(operation).Range()
}

func (operation *InsertPersistent) Valid(instance list.List) bool {
   return (*Insert)(operation).Valid(instance)
}

func (operation *InsertPersistent) Update(instance list.List, dist number.Distribution) (list.List, list.Position) {
   return (*Insert)(operation).Update(instance.Clone(), dist)
}
