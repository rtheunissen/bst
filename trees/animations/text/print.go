package text

import (
   "io"
)

func Print(writer io.Writer, strings ...string) {
   for _, s := range strings {
      if _, err := writer.Write([]byte(s)); err != nil {
         panic(err)
      }
   }
}

func Println(writer io.Writer, strings ...string) {
   Print(writer, strings...)
   Print(writer, "\n")
}


//
//func (screen *Writer) Flush() {
//   _, err := screen.Output.Write(screen.Buffer.Bytes())
//   if err != nil {
//      panic(err)
//   }
//}
