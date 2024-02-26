package text

import (
   "github.com/rtheunissen/bst/utility"
   "io"
   "math"
   "strconv"
)

var maxStackedHeight = 128

var maxStackedHeightSoFar = 0

type StackedHistogram struct {
   Title  string
   Series [2][]uint64
   Width  int
   Height int
}

func (g StackedHistogram) Print(page io.Writer) {
   L := g.Series[0]
   R := g.Series[1]

   Println(page, " ┌ ", g.Title)
   Println(page, " │")

   maxStackedHeightSoFar = max(maxStackedHeightSoFar, len(L), len(R))
   if g.Height < maxStackedHeightSoFar {
      g.Height = maxStackedHeightSoFar
   }
   if g.Height > maxStackedHeight {
      g.Height = maxStackedHeight
   }
   for row := 0; row <= g.Height; row++ {
      //
      // Determine the width of the left and right bars for this row.
      //
      barWidthL := 0
      barWidthR := 0
      if row < len(L) {
         barWidthL = int(math.Log2(float64(L[row])) + 1)
      }
      if row < len(R) {
         barWidthR = int(math.Log2(float64(R[row])) + 1)
      }
      Print(page, " │", utility.PadLeft(strconv.Itoa(row), 4))
      Print(page, utility.Repeat(" ", g.Width-barWidthL+1))
      if barWidthL > 0 {
         Print(page, "▕", utility.Repeat("░", barWidthL))
      } else {
         Print(page, " ")
      }
      if barWidthR > 0 {
         Print(page, utility.Repeat("▓", barWidthR), "▏")
      }
      Println(page)
   }
   Println(page, " └")
}
