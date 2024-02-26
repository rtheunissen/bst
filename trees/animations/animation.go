package animations

import (
   "github.com/rtheunissen/bst/trees"
   "github.com/rtheunissen/bst/trees/animations/text"
   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/types/list/operations"
   "github.com/rtheunissen/bst/utility"
   "github.com/rtheunissen/bst/utility/number"
   "github.com/rtheunissen/bst/utility/number/distribution"
   "github.com/rtheunissen/bst/utility/random"
   "golang.org/x/text/language"
   "golang.org/x/text/message"
   "io"
   "math"
)

var Operations = []list.Operation{
   &operations.Insert{
      Scale: 1_000_000,
   },
   &operations.InsertPersistent{
      Scale: 1_000_000,
   },
   &operations.InsertDelete{
      Scale:    10_000,
      Steps: 1_000_000,
   },
   &operations.InsertDeletePersistent{
      Scale:    10_000,
      Steps: 1_000_000,
   },
}

var Distributions = []number.Distribution{
   &distribution.Uniform{},
   &distribution.Normal{},
   &distribution.Skewed{},
   &distribution.Zipf{},
   &distribution.Minimum{},
   &distribution.Maximum{},
   &distribution.Median{},
   &distribution.Queue{},
   &distribution.Ascending{},
   &distribution.Descending{},
}

var Strategies = []list.List {
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
}

type Animation interface {
   Render(io.Writer)
   Setup()
   Update()
   Valid() bool
}

type BinaryTree struct {
   Height   int
   Offset   int
   Step     list.Position
   Last     list.Position
   Instance list.List
   list.Operation
   number.Distribution
}

func (a *BinaryTree) Setup() {
   a.Step = 0
   a.Last = 0
   a.Distribution = a.Distribution.New(random.Uint64())
   a.Instance = a.Instance.New()
}

func (a *BinaryTree) Valid() bool {
   return a.Operation.Valid(a.Instance)
}

func (a *BinaryTree) MoveUp() {
   a.Offset = max(-a.Height / 2, a.Offset - 1)
}

func (a *BinaryTree) MoveDown() {
   a.Offset = min(+a.Height / 2, a.Offset + 1)
}

func (a *BinaryTree) IncreaseHeight() {
   a.Height = a.Height + 1
}

func (a *BinaryTree) DecreaseHeight() {
   a.Height = max(0, a.Height - 1)
}

func (a *BinaryTree) Update() {
   for a.Operation.Valid(a.Instance) {
       a.Instance, a.Last = a.Operation.Update(a.Instance, a.Distribution)
       a.Step++
       if a.shouldRenderFrame() {
          return
      }
   }
}

//     1 to   100 : Render every page
//   100 to  1000 : Render every 10th
//  1000 to 10000 : Render every 100th etc.
func (a *BinaryTree) shouldRenderFrame() bool {
   nextLog10 := math.Ceil(math.Log10(float64(a.Instance.Size() + 1)))
   nextPow10 := math.Pow10(int(nextLog10))
   skipSteps := uint64(nextPow10 / 1000)
   return skipSteps == 0 || a.Step % skipSteps == 0
}

func (a *BinaryTree) FocusBar() text.FocusBar {
   return text.FocusBar{
      Focus: a.Last,
      Total: a.Instance.Size(),
   }
}

func (a *BinaryTree) Details() text.Details {
   return text.Details{
      Labels: []string{
         "Strategy",
         "Operation",
         "Distribution",
         "Size",
      },
      Values: []string{
         utility.NameOf(a.Instance),
         utility.NameOf(a.Operation),
         utility.NameOf(a.Distribution),
         message.NewPrinter(language.English).Sprint(a.Instance.Size()),
      },
   }
}