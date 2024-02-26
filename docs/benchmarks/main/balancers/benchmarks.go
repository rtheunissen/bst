package main

import (
   "fmt"
   "github.com/rtheunissen/bst/trees"
   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/utility"
   "github.com/rtheunissen/bst/utility/number"
   "github.com/rtheunissen/bst/utility/number/distribution"
   "os"
   "path/filepath"
   "time"
)

func main() {
  BalancerBenchmark{
     Duration:              10 * time.Second,
     Samples:              100,
     Scale:         10_000_000,
     Distributions: []number.Distribution{
        &distribution.Uniform{},
     },
     Strategies: []trees.Balancer{
        &trees.Median{},
        &trees.Height{},
        &trees.Weight{},
        &trees.Log{},
        &trees.Cost{},
        &trees.DSW{},
     },
  }.Run()
}


type BalancerBenchmark struct {
  Scale         int
  Samples       int
  Strategies    []trees.Balancer
  Distributions []number.Distribution
  Duration      time.Duration
}

func (benchmark BalancerBenchmark) Run() {

  //
  for _, strategy := range benchmark.Strategies {

     path := fmt.Sprintf(
        "docs/benchmarks/data/balancers/benchmarks/%s",
        utility.NameOf(strategy),
     )
     err := os.MkdirAll(filepath.Dir(path), os.ModePerm)
     if err != nil {
        panic(err)
     }
     file, err := os.Create(path)
     if err != nil {
        panic(err)
     }

     //
     //
     header := []any{
        "Distribution",
        "Scale",
        "Size",
        "Iterations",
        "Duration",
     }
     if  _, err := fmt.Fprintln(file, header...); err != nil {
        return
     }

     step := benchmark.Scale / benchmark.Samples

     instance := trees.Splay{}.New().(*trees.Splay)


     for position := step; position <= benchmark.Scale; position += step {


        // Grow the tree.
        for instance.Size() < list.Size(position) {
            instance.Insert(0, 0)
        }

        for _, random := range benchmark.Distributions {

           start := time.Now()

           iterations := 0

           var duration time.Duration

           for  {
              iterations++

              // Randomize the tree.
              instance.Tree = instance.Tree.Randomize(random.New(uint64(iterations)))

              checkpoint := time.Now()

              instance.Tree = strategy.Restore(instance.Tree)

              duration += time.Since(checkpoint)

              if time.Since(start) > benchmark.Duration {
                 break
              }
           }
           row := []any{
              utility.NameOf(random),
              fmt.Sprint(benchmark.Scale),
              fmt.Sprint(instance.Size()),
              fmt.Sprint(iterations),
              fmt.Sprint(duration.Nanoseconds() / int64(iterations)),
           }
           if _, err := fmt.Fprintln(file, row...); err != nil {
              panic(err)
           }
           fmt.Printf("%s %-10s %-10s %10d/%d %dx\n",
              time.Now().Format(time.TimeOnly),
              utility.NameOf(strategy),
              utility.NameOf(random),
              position,
              benchmark.Scale,
              iterations)

        }
     }
     instance.Free()
  }
}
