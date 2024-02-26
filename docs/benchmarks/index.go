package main

import (
   "os"
   "path"
   "strings"
   "text/template"
)

var funcMap = template.FuncMap{
   "inline": func(path string) (string, error) {
      content, err := os.ReadFile(path)
      return string(content), err
   },
}

func Index(directory string, tmpl *template.Template) {
   paths, err := os.ReadDir(directory)
   if err != nil {
      panic(err)
   }
   var files []string
   for _, file := range paths {
      if file.IsDir() {
         Index(path.Join(directory, file.Name()), tmpl)
      } else {
         if strings.HasSuffix(file.Name(), "svg") {
            files = append(files, path.Join(directory, file.Name()))
         }
      }
   }
   if len(files) == 0 {
      return
   }
   //
   file, err := os.Create(path.Join(directory, "index.html"))
   if err != nil {
      panic(err)
   }
   //

   err = tmpl.Execute(file, map[string]any{
      "directory": directory,
      "files": files,
   })
   if err != nil {
      panic(err)
   }
}

func Template(path string) *template.Template {
   tmpl, err := os.ReadFile(path)
   if err != nil {
      panic(err)
   }
   parsed, err := template.New("").Funcs(funcMap).Parse(string(tmpl))
   if err != nil {
      panic(err)
   }
   return parsed
}

func main() {
   Index("docs/benchmarks", Template("docs/benchmarks/index.template.html"))
}
