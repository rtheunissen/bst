package text

import (
   "github.com/rtheunissen/bst/utility"
   "io"
)

type Table struct {
   Title   string
   Columns []string
   Labels  []string
   Values  [][]string
}

const Padding = 4

func (table Table) Print(page io.Writer) {
   //
   //
   columnWidth := utility.LengthOfLongestString(table.Columns)

   for _, values := range table.Values {
      columnWidth = max(columnWidth, utility.LengthOfLongestString(values))
   }

   //
   maxLabelWidth := utility.CharacterCount(table.Title)
   maxLabelWidth = max(maxLabelWidth, utility.LengthOfLongestString(table.Labels))

   //
   // TITLE
   //
   Print(page, utility.Repeat(" ", Padding))
   Print(page, Bold(utility.PadRight(table.Title, maxLabelWidth)))

   //
   // COLUMN HEADERS
   //
   for _, columnLabel := range table.Columns {
      Print(page, utility.Repeat(" ", Padding))
      Print(page, utility.PadLeft(columnLabel, columnWidth))
   }
   Println(page)
   Print(page, utility.Repeat(" ", Padding), utility.Repeat("─", maxLabelWidth))
   Print(page, utility.Repeat("─", (columnWidth+Padding)*len(table.Columns)))
   Println(page)

   //
   // ROWS
   //
   for row, values := range table.Values {
      //
      // LABELS
      //
      Print(page, utility.Repeat(" ", Padding))
      Print(page, Italic(utility.PadRight(table.Labels[row], maxLabelWidth)))

      //
      // VALUES
      //
      for _, value := range values {
         Print(page, utility.Repeat(" ", Padding))
         Print(page, utility.PadLeft(value, columnWidth))
      }
      Println(page)
   }
   Println(page)
}
