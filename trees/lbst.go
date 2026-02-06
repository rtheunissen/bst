package trees

import (
   "github.com/rtheunissen/bst/utility"
)

type LogSize struct {
}

func (LogSize) maxHeight(s Size) int {
   return int(2 * utility.Log2(s))
}

func (LogSize) isBalanced(x, y Size) bool {
   return utility.GreaterThanOrEqualToMSB(x, y >> 1)
}

func (LogSize) singleRotation(x, y Size) bool {
   return utility.GreaterThanOrEqualToMSB(x, y)
}

type LogWeight struct {
}

func (LogWeight) maxHeight(s Size) int {
   return int(2 * utility.Log2(s))
}

func (LogWeight) isBalanced(x, y Size) bool {
   return utility.GreaterThanOrEqualToMSB(x + 1, (y + 1) >> 1)
}

func (LogWeight) singleRotation(x, y Size) bool {
   return utility.GreaterThanOrEqualToMSB(x + 1, y + 1)
}
