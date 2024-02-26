package text

import (
   "io"
)

type Graphic interface {
   Print(page io.Writer)
}

type Graphics []Graphic

func (graphics Graphics) Print(page io.Writer) {
  for _, g := range graphics {
     g.Print(page)
  }
}