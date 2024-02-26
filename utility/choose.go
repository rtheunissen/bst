package utility

func Resolve[T any](option string, options []T) (result T) {
   for _, candidate := range options {
      if NameOf(candidate) == option {
         return candidate
      }
   }
   return
}
