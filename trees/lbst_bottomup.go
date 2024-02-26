package trees

import (
   "github.com/rtheunissen/bst/types/list"
)

type LBSTBottomUp struct {
   WBSTBottomUp
}

func (LBSTBottomUp) New() list.List {
   return &LBSTBottomUp{
      WBSTBottomUp{
         WeightBalance: LogSize{},
      },
   }
}

func (tree *LBSTBottomUp) Clone() list.List {
   return &LBSTBottomUp{
      *tree.WBSTBottomUp.Clone().(*WBSTBottomUp),
   }
}
func (tree LBSTBottomUp) Split(i list.Position) (list.List, list.List) {
   l, r := tree.WBSTBottomUp.Split(i)
   return &LBSTBottomUp{*l.(*WBSTBottomUp)},
          &LBSTBottomUp{*r.(*WBSTBottomUp)}
}

func (tree LBSTBottomUp) Join(that list.List) list.List {
   return &LBSTBottomUp{
      *tree.WBSTBottomUp.Join(&that.(*LBSTBottomUp).WBSTBottomUp).(*WBSTBottomUp),
   }
}