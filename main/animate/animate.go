package main

import (
   "github.com/eiannone/keyboard"
   "github.com/rtheunissen/bst/trees/animations"
   "github.com/rtheunissen/bst/trees/animations/text"
   "github.com/rtheunissen/bst/types/list"
   "github.com/rtheunissen/bst/utility/number"
   "github.com/rtheunissen/bst/utility/random"
   "os"
)

func chooseOperation() list.Operation {
   return text.Choose[list.Operation]("Operation", animations.Operations...)
}

func chooseDistribution() number.Distribution {
   return text.Choose[number.Distribution]("Distribution", animations.Distributions...)
}

func chooseStrategy() list.List {
   return text.Choose[list.List]("Strategy", animations.Strategies...)
}

func main() {
   operation := animations.BinaryTree{
      Operation:    chooseOperation(),
      Instance:     chooseStrategy().New(),
      Distribution: chooseDistribution().New(random.Uint64()),
      Height:       40,
   }
   animation := text.Choose[animations.Animation]("Animation",
      &animations.ExteriorHeights{BinaryTree: &operation},
      &animations.InteriorHeights{BinaryTree: &operation},
      &animations.NodesPerLevel{BinaryTree: &operation},
   )
   //
   // Run animation.
   //
   animation.Setup()
   for animation.Valid() {
       text.Clear.Print(os.Stdout)
       animation.Render(os.Stdout)
       Update(animation, &operation)
   }
}

func Close() {
   if err := keyboard.Close(); err != nil {
      panic(err)
   }
   os.Exit(0)
}

func nextKeyPress() keyboard.Key {
   _, key, err := keyboard.GetSingleKey()
   if err != nil {
      panic(err)
  }
   return key
}

func Update(animation animations.Animation, operation *animations.BinaryTree) {
   switch key := nextKeyPress(); key {

   // " ← " decreases the draw offset of the image within the viewport.
   case keyboard.KeyArrowLeft:
      operation.MoveUp()

   // " → " increases the draw offset of the image within the viewport.
   case keyboard.KeyArrowRight:
      operation.MoveDown()

   // " ↑ " increases the height of the viewport.
   case keyboard.KeyArrowUp:
      operation.IncreaseHeight()

   // " ↓ " Decreases the height of the viewport.
   case keyboard.KeyArrowDown:
      operation.DecreaseHeight()

   // ESC and various common combinations to exit.
   case keyboard.KeyCtrlC: fallthrough
   case keyboard.KeyCtrlD: fallthrough
   case keyboard.KeyCtrlZ: fallthrough
   case keyboard.KeyCtrlQ: fallthrough
   case keyboard.KeyEsc:
      Close()

   // Next frame.
   default:
      operation.Update()
      animation.Update()
   }
}

