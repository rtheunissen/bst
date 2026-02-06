package utility

import (
   "fmt"
   "strconv"
   "strings"
   "unicode/utf8"
)

func PadLeft(text string, length int) string {
   return Repeat(" ", length - CharacterCount(text)) + text
}

func PadRight(text string, length int) string {
   return text + Repeat(" ", length - CharacterCount(text))
}

func Centered(text, padding string, length int) string {
   s := length - CharacterCount(text)
   return Repeat(padding, (s + 1) / 2) + text + Repeat(padding, s / 2)
}

func String(value any) string {
   return fmt.Sprint(value)
}

func Repeat(s string, n int) string {
   if n <= 0 {
      return ""
   } else {
      return strings.Repeat(s, n)
   }
}

func CharacterCount(s string) int {
   return utf8.RuneCountInString(s)
}

func LengthOfLongestString(strings []string) (max int) {
   for _, s := range strings {
      if max < CharacterCount(s) {
         max = CharacterCount(s)
      }
   }
   return max
}

func ParseFloat64(s string) float64 {
   f, _ := strconv.ParseFloat(s, 64)
   return f
}


func CommaSep(number uint64) string {
   s := fmt.Sprint(number)
   n := len(s)
   if n <= 3 {
      return s
   }
   var b strings.Builder
   for i, c := range s {
      if i > 0 && (n-i)%3 == 0 {
         b.WriteByte(',')
      }
      b.WriteRune(c)
   }
   return b.String()
}
