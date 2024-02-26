package text

import (
   "fmt"
   "github.com/rtheunissen/bst/utility"
   "strconv"
)

func Choose[T any](prompt string, options ...T) T {
   fmt.Print("\n")
   fmt.Print("\n")
   for i, option := range options {
      fmt.Printf(" %s \033[1m%s\033[22m\n",
         utility.PadLeft("("+strconv.Itoa(i+1)+")", 4), utility.NameOf(option))
   }
   // Show the prompt at the bottom of the options.
   fmt.Printf("\n \033[7m%s:\033[27m ", prompt)

   // Read the chosen index.
   choice := 1
   _, _ = fmt.Scanln(&choice)
   fmt.Println()

   // Ask again If the choice is out of range.
   if choice < 0 || choice > len(options) {
      return Choose(prompt, options...)
   }
   return options[choice - 1]
}
