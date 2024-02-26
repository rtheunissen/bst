package trees

import (
   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/utility"
)

type LogSize struct {
}

func (LogSize) maxHeight(s list.Size) int {
   return int(2 * utility.Log2(s))
}

func (LogSize) isBalanced(x, y list.Size) bool {
   return utility.GreaterThanOrEqualToMSB(x, y >> 1)
}

func (LogSize) singleRotation(x, y list.Size) bool {
   return utility.GreaterThanOrEqualToMSB(x, y)
}

type LogWeight struct {
}

func (LogWeight) maxHeight(s list.Size) int {
   return int(2 * utility.Log2(s))
}

func (LogWeight) isBalanced(x, y list.Size) bool {
   return utility.GreaterThanOrEqualToMSB(x + 1, (y + 1) >> 1)
}

func (LogWeight) singleRotation(x, y list.Size) bool {
   return utility.GreaterThanOrEqualToMSB(x + 1, y + 1)
}
