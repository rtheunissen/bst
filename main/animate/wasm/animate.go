//go:build js && wasm

package main

import (
   "bytes"
   "github.com/rtheunissen/bst/trees/animations"
   "github.com/rtheunissen/bst/utility"
   "github.com/rtheunissen/bst/utility/random"
   "syscall/js"
)

var operation animations.BinaryTree

var animation animations.Animation

var buffer bytes.Buffer

func match[T any](choice string, options []T) T {
   for _, o := range options {
      if choice == utility.NameOf(o) {
         return o
      }
   }
   return options[0]
}

func main() {
   c := make(chan int)
   js.Global().Set("Valid", js.FuncOf(Valid))
   js.Global().Set("Update", js.FuncOf(Update))
   js.Global().Set("Render", js.FuncOf(Render))
   js.Global().Set("MoveUp", js.FuncOf(MoveUp))
   js.Global().Set("MoveDown", js.FuncOf(MoveDown))
   js.Global().Set("SetHeight", js.FuncOf(SetHeight))

   js.Global().Set("SetAnimation", js.FuncOf(SetAnimation))
   js.Global().Set("SetOperation", js.FuncOf(SetOperation))
   js.Global().Set("SetStrategy", js.FuncOf(SetStrategy))
   js.Global().Set("SetDistribution", js.FuncOf(SetDistribution))
   <-c
}

func SetAnimation(this js.Value, args []js.Value) any {
   animation = match(args[0].String(), []animations.Animation{
      &animations.InteriorHeights{BinaryTree: &operation},
      &animations.ExteriorHeights{BinaryTree: &operation},
      &animations.NodesPerLevel{BinaryTree: &operation},
   })
   animation.Update()
   return nil
}

func SetOperation(this js.Value, args []js.Value) any {
   operation.Operation = match(args[0].String(), animations.Operations).New()
   return nil
}

func SetStrategy(this js.Value, args []js.Value) any {
   operation.Instance = match(args[0].String(), animations.Strategies).New()
   return nil
}

func SetDistribution(this js.Value, args []js.Value) any {
   operation.Distribution = match(args[0].String(), animations.Distributions).New(random.Uint64())
   return nil
}

func Update(this js.Value, args []js.Value) any {
   operation.Update()
   animation.Update()
   return nil
}

func Valid(this js.Value, args []js.Value) any {
   return operation.Valid()
}

func Render(this js.Value, args []js.Value) any {
   buffer.Reset()
   animation.Render(&buffer)
   return buffer.String()
}

func MoveUp(this js.Value, args []js.Value) any {
   operation.MoveUp()
   return nil
}

func MoveDown(this js.Value, args []js.Value) any {
   operation.MoveDown()
   return nil
}

func SetHeight(this js.Value, args []js.Value) any {
   operation.Height = args[0].Int()
   return nil
}


