package utility

import (
   "reflect"
   "runtime"
   "strings"
)

func FuncName(test interface{}) string {
   name := runtime.FuncForPC(reflect.ValueOf(test).Pointer()).Name()
   base := strings.SplitAfter(name, ".")
   return base[len(base) - 1]
}

func NameOf(v any) string {
   t := reflect.TypeOf(v)
   if t == nil {
      return "<nil>"
   }
   if t.Kind() == reflect.Ptr {
      return t.Elem().Name()
   } else {
      return t.Name()
   }
}
