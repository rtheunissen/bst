package animations

import (
   "github.com/rtheunissen/bst/trees"
   "github.com/rtheunissen/bst/trees/animations/text"
   "io"
)

type InteriorHeights struct {
   *BinaryTree
   heights [2][]int
}

func (animation *InteriorHeights) Update() {
   animation.heights = animation.Instance.(trees.BinaryTree).InteriorHeightsAlongTheSpines()
}

func (animation *InteriorHeights) Render(writer io.Writer) {
   animation.getGraphics().Print(writer)
}

func (animation *InteriorHeights) getGraphics() text.Graphics {
   return text.Graphics{
      text.Linebreak,
      text.Histogram{
         Title: "Interior heights along the spines",
         Series: animation.heights,
         Height: animation.Height,
         Offset: animation.Offset,
      },
      text.Linebreak,
      animation.FocusBar(),
      text.Linebreak,
      animation.Details(),
      text.Linebreak,
      //text.Text(" Use ↑ ↓ arrows keys to adjust the vertical offset."),
   }
}
