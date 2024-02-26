package text

import (
   "github.com/rtheunissen/bst/utility"
   "io"
)

type BarChart struct {
   Title  string
   Width  int
   Labels []string
   Values []string
   Floats []float64
}

func (chart BarChart) Print(page io.Writer) {
   var labelWidth = utility.LengthOfLongestString(chart.Labels)
   var valueWidth = utility.LengthOfLongestString(chart.Values)

   barPadding := 8
   labelMargin := 4
   maxBarWidth := chart.Width - valueWidth - barPadding

   var maxValue float64
   for _, value := range chart.Floats {
      if maxValue < value {
         maxValue = value
      }
   }
   Println(page, utility.Repeat(" ", labelWidth+labelMargin), "  ", Bold(chart.Title))
   Println(page, utility.Repeat(" ", labelWidth+labelMargin), " ┍", utility.Repeat("━", chart.Width))

   for i, label := range chart.Labels {

      x := int(chart.Floats[i]/maxValue*float64(maxBarWidth-1)) + 1

      Print(page, utility.Repeat(" ", labelMargin), Italic(utility.PadRight(label, labelWidth)), " │")
      Print(page, utility.Repeat("▓", x))
      Print(page, utility.PadLeft(chart.Values[i], chart.Width-x-1))
      Println(page)
   }
   Println(page, utility.Repeat(" ", labelWidth+labelMargin), " └", utility.Repeat("─", chart.Width))
   Println(page)
}

type SortedByBestFirst BarChart

func (s SortedByBestFirst) Len() int {
   return len(s.Labels)
}
func (s SortedByBestFirst) Swap(i, j int) {
   s.Labels[i], s.Labels[j] = s.Labels[j], s.Labels[i]
   s.Floats[i], s.Floats[j] = s.Floats[j], s.Floats[i]
   s.Values[i], s.Values[j] = s.Values[j], s.Values[i]
}
func (s SortedByBestFirst) Less(i, j int) bool {
   return s.Floats[i] < s.Floats[j]
}
