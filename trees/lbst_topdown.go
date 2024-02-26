package trees

import (
   "github.com/rtheunissen/bst/types/list"
)


type LBSTTopDown struct {
   WBSTTopDown
}

func (LBSTTopDown) New() list.List {
   return &LBSTTopDown{
      WBSTTopDown{
         WeightBalance: LogWeight{},
      },
   }
}

func (tree *LBSTTopDown) Clone() list.List {
   return &LBSTTopDown{
      *tree.WBSTTopDown.Clone().(*WBSTTopDown),
   }
}
func (tree LBSTTopDown) Split(i list.Position) (list.List, list.List) {
   l, r := tree.WBSTTopDown.Split(i)
   return &LBSTTopDown{*l.(*WBSTTopDown)},
          &LBSTTopDown{*r.(*WBSTTopDown)}
}

func (tree LBSTTopDown) Join(that list.List) list.List {
   return &LBSTTopDown{
      *tree.WBSTTopDown.Join(&that.(*LBSTTopDown).WBSTTopDown).(*WBSTTopDown),
   }
}