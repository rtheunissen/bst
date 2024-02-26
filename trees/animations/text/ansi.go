package text

import (
   "fmt"
)

const CLEAR_SCREEN = "\u001B[2J"

func Bold(s string) string {
   return fmt.Sprintf("\u001B[1m%s\u001B[22m", s)
}

func Italic(s string) string {
   return fmt.Sprintf("\u001B[3m%s\u001B[23m", s)
}
