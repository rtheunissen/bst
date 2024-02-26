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

type OperationMeasurement struct {
   Samples       list.Size
   Operation     list.Operation
   Distributions []number.Distribution
   Strategies    []list.List
   Measurements  []trees.Measurement
}

func main() {
   operation := flag.String("operation", "", "")
   flag.Parse()

   OperationMeasurement{
      Samples: 1_000,
      Operation: utility.Resolve(*operation, []list.Operation{
         &operations.InsertPersistent{
            Scale: 10_000_000,
         },
         &operations.InsertDeletePersistent{
            Steps:  10_000_000,
            Scale:   1_000_000,
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
      Measurements: []trees.Measurement{
         &trees.PartitionCount{},
         &trees.PartitionDepth{},
         &trees.AveragePathLength{},
         &trees.MaximumPathLength{},
         &trees.Rotations{},
         &trees.Allocations{},
      },
   }.Run()
}


func (measurement OperationMeasurement) Run() {
   if measurement.Operation == nil {
      return
   }

   debug.SetMaxStack(10_000_000_000) // 10Gb

   for _, strategy := range measurement.Strategies {

      path := fmt.Sprintf(
         "docs/benchmarks/data/operations/measurements/%s/%s",
         utility.NameOf(measurement.Operation),
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

      header := []any{
         "Distribution",
         "Scale",
         "Size",
         "Step",
         "Position",
      }
      for _, measurement := range measurement.Measurements {
         header = append(header, utility.NameOf(measurement))
      }
      fmt.Fprintln(file, header...)

      for _, distribution := range measurement.Distributions {

         fmt.Printf("%s %-32s %-32s %-32s\n",
            time.Now().Format(time.RFC822),
            utility.NameOf(measurement.Operation),
            utility.NameOf(strategy),
            utility.NameOf(distribution))

         instance := strategy.New()

         access := distribution.New(1)

         steps := measurement.Operation.Range()

         step := steps / measurement.Samples

         for position := step; position <= steps; position = position + step {

            for _, measurement := range measurement.Measurements {
               measurement.Reset()
            }
            for i := list.Size(0); i < step; i++ {
               instance, _ = measurement.Operation.Update(instance, access)
            }
            row := []any{
               utility.NameOf(access),
               fmt.Sprint(steps),
               fmt.Sprint(instance.Size()),
               fmt.Sprint(step),
               fmt.Sprint(position),
            }
            for _, measurement := range measurement.Measurements {
               row = append(row, fmt.Sprint(measurement.Measure(instance.(trees.BinaryTree))))
            }
            fmt.Fprintln(file, row...)
         }
         instance.Free()
         runtime.GC()
      }
   }
}
