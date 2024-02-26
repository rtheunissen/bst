package text

import (
   "github.com/rtheunissen/bst/utility"
   "io"
   "math"
)

type FocusBar struct {
   Focus uint64
   Total uint64
}

func (f FocusBar) Print(page io.Writer) {
   if f.Total == 0 {
      return
   }
   w := int(math.Log2(float64(f.Total))) + 1
   x := int(float64(f.Focus) / float64(f.Total) * float64(w))

   if x > w {
      x = 0
   }

   Print(page, " ")
   Print(page, "Access: ")
   Print(page, utility.Repeat("░", x))
   Print(page, utility.Repeat("▓", 1))
   Print(page, utility.Repeat("░", w-x-1))
   Print(page, " ")
}
