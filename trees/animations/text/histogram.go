package text

import (
   "github.com/rtheunissen/bst/utility"
   "io"
   "math"
   "strconv"
)

var maxHistogramBarWidth = 80

var maxHistogramBarWidthSoFar = 0

type Histogram struct {
   Series [2][]int
   Title string
   Height int
   Offset int
}

func (g Histogram) Print(page io.Writer) {
   //
   //
   l := g.Series[0]
   r := g.Series[1]
   h := g.Height

   // The number of rows we would ideally like to draw for each side.
   numberOfRowsL := len(l)
   numberOfRowsR := len(r)

   // Because of the offset, from the middle, we calculate how many of those
   // rows we have room to draw. As the offset increases, the image moves to
   // the right (or down), increasing the capacity of the left side.
   //
   // However, that capacity might exceed the total height.
   capacityL := int(math.Floor(float64(h)/2)) + g.Offset
   capacityR := int(math.Ceil(float64(h)/2)) - g.Offset

   l = l[max(0, min(len(l), len(l)-capacityL)):]
   l = l[:min(len(l), max(0, len(l)+capacityR))]

   r = r[min(len(r), max(0, 0-capacityL)):]
   r = r[:min(len(r), max(0, 0+capacityR))]

   // Calculate padding to keep the graphic vertically centered.
   paddingTop := 0
   paddingBot := 0

   // Do we have some empty space?
   if emptySpace := h - (len(l) + len(r)); emptySpace > 0 {
      paddingTop = max(0, min(emptySpace, capacityL-numberOfRowsL))
      paddingBot = max(0, min(emptySpace, capacityR-numberOfRowsR))
   }
   //
   Println(page)
   Println(page, " ┌ ", g.Title) // ╭

   //
   for ; paddingTop >= 0; paddingTop-- {
      Println(page, " │")
   }
   //
   for _, width := range l {
      maxHistogramBarWidthSoFar = max(maxHistogramBarWidthSoFar, width)
      Print(page, " │")
      Print(page, truncatedBar("░", width), "▏")
      Println(page)
   }
   //
   for _, width := range r {
      maxHistogramBarWidthSoFar = max(maxHistogramBarWidthSoFar, width)
      Print(page, " │")
      Print(page, truncatedBar("▓", width), "▏")
      Println(page)
   }
   //
   for ; paddingBot >= 0; paddingBot-- {
      Println(page, " │")
   }
   //
   Print(page, " └")
   Print(page, utility.Repeat("─", maxHistogramBarWidthSoFar), "┤ ")
   Print(page, strconv.Itoa(maxHistogramBarWidthSoFar))
   Println(page)
}

func truncatedBar(char string, width int) string {
   if width > maxHistogramBarWidth {
      return utility.Repeat(char, maxHistogramBarWidth) + "···"
   } else {
      return utility.Repeat(char, width)
   }
}
