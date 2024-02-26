package text

import (
   "io"
)

type Text string

const Clear = Text(CLEAR_SCREEN)

const Linebreak = Text("\n")

func (text Text) Print(page io.Writer)  {
   Print(page, string(text))
}

