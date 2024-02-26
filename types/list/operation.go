package list

import (
   "github.com/rtheunissen/bst/utility/number"
)

type Operation interface {
   New() Operation
   Update(List, number.Distribution) (List, Position)
   Valid(List) bool
   Range() Size
}