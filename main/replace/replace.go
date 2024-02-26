package main

import (
   "flag"
   "io/fs"
   "os"
   "path/filepath"
   "strings"
)

func main() {
   directory := flag.String("dir", ".", "directory to start from")
   find      := flag.String("find", "", "string to find")
   replace   := flag.String("replace", "", "string to replace with")
   flag.Parse()

   //if *enable {
   //   find, replace = "// assert(", "assert("
   //} else {
   //   find, replace = "  assert(", "  // assert("
   //}
   if err := directoryFindReplace(*directory, *find, *replace); err != nil {
      panic(err)
   }
}

func directoryFindReplace(directory, find, replace string) error {
   return filepath.WalkDir(directory, func(path string, file fs.DirEntry, err error) error {
      if err != nil {
         return err
      }
      matched, err := filepath.Match("*.go", file.Name())
      if err != nil {
         return err
      }
      if !matched {
         return nil
      }
      bytes, err := os.ReadFile(path)
      if err != nil {
         return err
      }
      replaced := strings.Replace(string(bytes), find, replace, -1)
      if len(replaced) != len(bytes) {
         return os.WriteFile(path, []byte(replaced), 0)
      }
      return nil
   })
}
