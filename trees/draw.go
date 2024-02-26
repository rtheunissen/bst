package trees

import (
   "fmt"
   "github.com/rtheunissen/bst/utility"
   "io"
)

// Draw is a basic implementation of a binary tree drawing algorithm, derived
// from how I was drawing them by hand for many years. The algorithm is only
// suitable for small trees because the width is exponential and wraps very
// quickly in the terminal. The main purpose of this algorithm is to debug cases
// that trigger invariant checks and to understand and verify some balancing and
// building algorithms along the way.
//
// For example:
//
//   Tree{}.New(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 50, 100).Draw()
//
//
//                                   7                     <-- 1st level
//                           ╭───────┴───────╮
//                           4               20            <-- 2nd level
//                       ╭───┴───╮       ╭───┴───╮
//                       2       6       9      100        <-- 3rd level
//                     ╭─┴─╮   ╭─╯     ╭─┴─╮   ╭─╯
//                     1   3   5       8   10  50          <-- 4th level
//
// Notice the following properties:
//
//   - Nodes are drawn centered above subtrees.
//   - Nodes are positioned in-order from left to right.
//   - Nodes are assumed to have a value no greater than 3-characters,
//     which is plenty in most cases before the tree becomes too wide.
func (p *Node) Draw(writer io.Writer) {
   if p == nil {
      return
   }
   // The first stage of the drawing algorithm collects nodes, level by level.
   //
   // For example, in the example drawing abive, the first level would have
   // one node (7), the second level would have two nodes: (4) and (20), etc.
   //
   // Nodes that are nil are collected as well to know where to draw spacing.
   //
   var level []*Node    // Current level
   var levels [][]*Node // Collected levels

   level = append(level, p)
   for {
      // Collect the nodes for the next level by iterating through the nodes
      // of the current level, until all nodes in the next level are nil.
      //
      var next []*Node
      for i, node := range level {
         if node != nil {
            break
         }
         if i == len(level)-1 { // All nodes in the next level are nil.
            goto drawing
         }
      }
      for _, node := range level {
         if node == nil {
            next = append(next, nil)
            next = append(next, nil)
         } else {
            next = append(next, node.l)
            next = append(next, node.r)
         }
      }
      // Add the collected nodes to the list of level, move on to the next.
      levels = append(levels, level)
      level = next
   }
   // Now we can start drawing.
   //
   // For each level, we first draw the line-work (except for the first level),
   // followed by the node values of that level.
   //
drawing:
   for depth, level := range levels {
      //
      // The spacing is a measure of the separation between nodes, increasing
      // exponentially from the bottom to the top of the tree.
      //
      spacing := 1 << (len(levels) - depth)

      // Skip the line-work for the first level because the root has to parent.
      if depth > 0 {
         //
         // The first space to write is the prefix, or the leading gap from the
         // left of the frame before the first node of the level is to be drawn.
         //
         fmt.Fprint(writer, utility.Repeat(" ", spacing-1))

         // Here begins the line-work.
         //
         // There are 3 possibilities for each pair of nodes on this level:
         //
         //    ╭───┴───╮      Left and right are not nil.
         //
         //    ╭───╯          Left is not nil, right is nil.
         //
         //        ╰───╮      Left is nil, right is not nil.
         //
         // We first draw the left side, then the connector to the parent node,
         // then the right side, as indicated by these vertical guides:
         //
         //       | |
         //   ╭───|┴|───╮
         //       | |
         //   ╭───|╯|
         //       | |
         //       |╰|───╮
         //       | |
         //
         for i, node := range level {
            if i % 2 == 0 {
               //
               // Left node.
               //
               // Here we draw a shape like this: ╭──, under which the node will
               // be placed in the next row of the drawing. When the node is nil
               // we just draw more spacing until we are under the parent, where
               // the line for the next node (the right subtree) will be drawn.
               //
               if node == nil {
                  fmt.Fprint(writer, utility.Repeat(" ", spacing))
               } else {
                  fmt.Fprint(writer, "╭")
                  fmt.Fprint(writer, utility.Repeat("─", spacing-1))
               }
            } else {
               //
               // Right node.
               //
               // Here we first draw the connector to the parent: ╯ or ┴ or ╰
               // depending on whether the left or right nodes are nil, then
               // the line towards where the right child will be drawn, then the
               // connector to the right node (all spacing if the node is nil).
               //
               // Because the index is odd, we know that a previous index must
               // exist, so it is safe to access the left node as level[i-1].
               //
               if node == nil {
                  //
                  // The right node of the pair is nil, so either the left node
                  // is also nil (no line-work at all), or the left node is not
                  // nil, in which case it is the only non-nil node under the
                  // parent and we can use the ╯ connector to complete the pair.
                  //
                  if level[i-1] == nil {
                     fmt.Fprint(writer, " ") // Left and right are nil.
                  } else {
                     fmt.Fprint(writer, "╯") // Left is not nil, right is nil.
                  }
                  if i < len(level)-1 {
                     fmt.Fprint(writer, utility.Repeat(" ", spacing))
                  }
                  //
                  // The right node is nil so the line-work for this pair is now
                  // complete. The final step is to draw the spacing before the
                  // line-work for the next pair should start. However, only add
                  // this spacing if this is not the last node of the level.
                  //
               } else {
                  //
                  // The right node of the pair is NOT nil, so either the left
                  // node is nil (╰) or they are both not nil (┴).
                  //
                  if level[i-1] == nil {
                     fmt.Fprint(writer, "╰") // Left is nil, right is not nil.
                  } else {
                     fmt.Fprint(writer, "┴") // Left and right are not nil.
                  }
                  //
                  // The right node is NOT nil, so we need to now draw a line
                  // from the parent connector to where the right node will be
                  // drawn below. Given that there is a node here, we can draw
                  // the downward connector (╮) exactly above that node.
                  //
                  fmt.Fprint(writer, utility.Repeat("─", spacing-1))
                  fmt.Fprint(writer, "╮")
               }
               //
               // The final step is to draw the spacing that separates this pair
               // from the next pair on this level. However, to avoid wrapping
               // and unnecessary trailing whitespace, we only add this spacing
               // if this was not the last pair of the level.
               //
               if i < len(level)-1 {
                  fmt.Fprint(writer, utility.Repeat(" ", spacing))
                  fmt.Fprint(writer, utility.Repeat(" ", spacing-1))
               }
            }
         }
         fmt.Fprint(writer, "\n")
      }
      // The line-work for this level is done, and we are now on a new line.
      // Here we follow the same pattern as with the line-work, except instead
      // of lines we draw the `value` of each node.
      //
      // There is no need to consider differences between left and right because
      // all nodes are drawn the same and the spacing is consistent.
      //
      for i, node := range level {
         //
         // When the node to draw is nil, we need to draw empty space below the
         // missing line-work, up to where the node value would have been drawn
         // if it was not nil, and then more spacing for the separation leading
         // up to the next pair when it is not the last node of the level.
         //
         if node == nil {
            if i < len(level)-1 {
               fmt.Fprint(writer, utility.Repeat(" ", spacing))
               fmt.Fprint(writer, utility.Repeat(" ", spacing))
            }
         } else {
            //
            // When the node to draw is NOT nil, we center the node's value (x)
            // within a cell of 3-characters and prefix it with node separation.
            //
            value := utility.Centered(utility.String(node.y), " ", 3)
            fmt.Fprint(writer, utility.Repeat(" ", spacing-len(value)+1))
            fmt.Fprint(writer, value)

            // Draw spacing to create separation between this pair and the next
            // pair of nodes on this level. However, to avoid line-wrapping and
            // unnecessary trailing whitespace, we only add this spacing if this
            // is not the last node of the current level.
            if i < len(level)-1 {
               fmt.Fprint(writer, utility.Repeat(" ", spacing-1))
            }
         }
      }
      fmt.Fprint(writer, "\n")
   }
}

//TODO: is Draw ever _not_ stdout?
func (tree Tree) Draw(writer io.Writer) {
   tree.root.Draw(writer)
}
