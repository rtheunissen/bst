package trees

// Assertions are checks within code to verify that certain conditions are met.
//
// Go does not support zero-cost assertions, so this project uses find/replace
// to toggle them with comments `//`. Tests enable them, code coverage disables
// them, as well as benchmarks and animations. Some assertions run in O(n) or
// worse, so they should always be disabled when performance matters.
//
//    `make assertions-on`   : enables assertions
//    `make assertions-off`  : disables assertions
//
func assert(condition bool) {
   if !condition {
      panic("assertion failed")
   }
}

// Invariant checks are always evaluated even when assertions are disabled.
func invariant(condition bool) {
   if !condition {
      panic("invariant failed")
   }
}