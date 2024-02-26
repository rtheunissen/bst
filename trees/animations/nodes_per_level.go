package animations

import (
   "github.com/rtheunissen/bst/trees"
   "github.com/rtheunissen/bst/trees/animations/text"
   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/utility"
   "io"
)

type NodesPerLevel struct {
   *BinaryTree
   counts [2][]list.Size
}

func (animation *NodesPerLevel) Render(writer io.Writer) {
   animation.getGraphics().Print(writer)
}

func (animation *NodesPerLevel) Update() {
   animation.counts = animation.Instance.(trees.BinaryTree).NodesPerLevel()
}

func (animation *NodesPerLevel) getGraphics() text.Graphics {
   return text.Graphics{
      text.Linebreak,
      text.StackedHistogram{
         Title:  "Number of nodes per level, log2",
         Series: animation.counts,
         Height: animation.Height,
         Width: int(utility.Log2(animation.Instance.Size()) + 1),
      },
      text.Linebreak,
      animation.FocusBar(),
      text.Linebreak,
      animation.Details(),
      text.Linebreak,
   }
}
