package trees

import "github.com/rtheunissen/bst/types/list"

type LBSTRelaxed struct {
   WBSTRelaxed
}

func (LBSTRelaxed) New() list.List {
   return &LBSTRelaxed{
      WBSTRelaxed{
         WeightBalance: LogWeight{},
      },
   }
}

func (tree *LBSTRelaxed) Clone() list.List {
   return &LBSTRelaxed{
      *tree.WBSTRelaxed.Clone().(*WBSTRelaxed),
   }
}
func (tree LBSTRelaxed) Split(i list.Position) (list.List, list.List) {
   l, r := tree.WBSTRelaxed.Split(i)
   return &LBSTRelaxed{*l.(*WBSTRelaxed)},
          &LBSTRelaxed{*r.(*WBSTRelaxed)}
}

func (tree LBSTRelaxed) Join(that list.List) list.List {
   return &LBSTRelaxed{
      *tree.WBSTRelaxed.Join(&that.(*LBSTRelaxed).WBSTRelaxed).(*WBSTRelaxed),
   }
}