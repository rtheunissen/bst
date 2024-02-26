package animations

import (
   "github.com/rtheunissen/bst/trees"
   "github.com/rtheunissen/bst/trees/animations/text"
   "io"
)

type ExteriorHeights struct {
   *BinaryTree
   heights [2][]int
}

func (animation *ExteriorHeights) Update() {
   animation.heights = animation.Instance.(trees.BinaryTree).ExteriorHeightsAlongTheSpines()
}

func (animation *ExteriorHeights) Render(writer io.Writer) {
   animation.getGraphics().Print(writer)
}

func (animation *ExteriorHeights) getGraphics() text.Graphics {
   return text.Graphics{
      text.Linebreak,
      text.Histogram{
         Title: "Exterior heights along the spines",
         Series: animation.heights,
         Height: animation.Height,
         Offset: animation.Offset,
      },
      text.Linebreak,
      animation.FocusBar(),
      text.Linebreak,
      animation.Details(),
      text.Linebreak,
   }
}
