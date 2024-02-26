package main

import (
   "flag"
   "fmt"
   "github.com/rtheunissen/bst/trees"
   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/types/list/operations"
   "github.com/rtheunissen/bst/utility"
   "github.com/rtheunissen/bst/utility/number"
   "github.com/rtheunissen/bst/utility/number/distribution"
   "os"
   "path/filepath"
   "runtime"
   "runtime/debug"
   "time"
)

func main() {
   operation := flag.String("operation", "", "")
   flag.Parse()

   TreeBenchmark{
      Iterations: 10,
      Samples:    1_000,
      Operation: utility.Resolve(*operation, []list.Operation{
         //&operations.Insert{
         //  Scale: 10_000_000,
         //},
         &operations.InsertPersistent{
           Scale: 10_000_000,
         },
         &operations.InsertDelete{
            Steps: 10_000_000,
            Scale:  1_000_000,
         },
         &operations.InsertDeletePersistent{
           Steps: 10_000_000,
           Scale:  1_000_000,
         },
      }),
      Distributions: []number.Distribution{
         &distribution.Uniform{},
         &distribution.Normal{},
         &distribution.Skewed{},
         &distribution.Zipf{},
         &distribution.Maximum{},
      },
      Strategies: []list.List{
         &trees.AVLBottomUp{},
         &trees.AVLTopDown{},
         &trees.AVLWeakTopDown{},
         &trees.AVLWeakBottomUp{},
         &trees.AVLRelaxedTopDown{},
         &trees.AVLRelaxedBottomUp{},
         &trees.RedBlackBottomUp{},
         &trees.RedBlackTopDown{},
         &trees.RedBlackRelaxedBottomUp{},
         &trees.RedBlackRelaxedTopDown{},
         &trees.WBSTBottomUp{},
         &trees.WBSTTopDown{},
         &trees.WBSTRelaxed{},
         &trees.LBSTBottomUp{},
         &trees.LBSTTopDown{},
         &trees.LBSTRelaxed{},
         &trees.TreapTopDown{},
         &trees.TreapTopDown{},
         &trees.TreapFingerTree{},
         &trees.Randomized{},
         &trees.Zip{},
         &trees.Splay{},
         &trees.Conc{},
      },
   }.Run()
}


type TreeBenchmark struct {
   Samples       list.Size
   Operation     list.Operation
   Distributions []number.Distribution
   Strategies    []list.List
   Iterations    int
}

func (benchmark TreeBenchmark) Run() {
   if benchmark.Operation == nil {
      return
   }

   //
   for _, strategy := range benchmark.Strategies {

      path := fmt.Sprintf(
         "docs/benchmarks/data/operations/benchmarks/%s/%s",
         utility.NameOf(benchmark.Operation),
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
      // Header
      //
      fmt.Fprintln(file, []any{
         "Distribution", "Scale","Size", "Step", "Position", "Iteration", "Duration",
      }...)

      //
      //
      //
      debug.SetGCPercent(-1)

      //
      for iteration := 1; iteration <= benchmark.Iterations; iteration++ {

         //
         for _, random := range benchmark.Distributions {
            //
            //
            //
            fmt.Printf("%s %-32s %-32s %-32s %10d/%d\n",
               time.Now().Format(time.RFC822),
               utility.NameOf(benchmark.Operation),
               utility.NameOf(strategy),
               utility.NameOf(random),
               iteration,
               benchmark.Iterations)

            //
            access := random.New(uint64(iteration + 1))

            //
            instance := strategy.New()

            steps := benchmark.Operation.Range()

            //
            step := steps / benchmark.Samples

            //
            for position := step; position <= steps; position += step {
               //
               //
               //
               start := time.Now()
               for i := list.Size(0); i < step; i++ {
                  instance, _ = benchmark.Operation.Update(instance, access)
               }
               duration := time.Since(start)
               //
               //
               //
               fmt.Fprintln(file, []any{
                  utility.NameOf(access),
                  fmt.Sprint(steps),
                  fmt.Sprint(instance.Size()),
                  fmt.Sprint(step),
                  fmt.Sprint(position),
                  fmt.Sprint(iteration),
                  fmt.Sprint(duration.Nanoseconds()),
               }...)
            }
            instance.Free()
            runtime.GC()
         }
      }
   }
}