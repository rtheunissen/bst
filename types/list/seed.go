package list

import (
   "hash/fnv"
   "os"
   "regexp"
   "runtime/debug"
   "strconv"
   "strings"
   "testing"
   "time"

   "github.com/rtheunissen/bst/utility/random"
)

// The base seed of a test run is random, unless the BST_SEED environment
// variable is set to reproduce a previous run.
var baseSeed = func() uint64 {
   if seed, ok := os.LookupEnv("BST_SEED"); ok {
      parsed, err := strconv.ParseUint(seed, 10, 64)
      if err != nil {
         panic("BST_SEED must be an unsigned integer: " + seed)
      }
      return parsed
   }
   return uint64(time.Now().UnixNano())
}()

// Seeded runs a test with a generator that is seeded only for that test.
//
// The seed of every test is derived from the base seed and the name of the
// test, so a test draws the same values regardless of which other tests run,
// in which order, or in parallel. A failure reports how to reproduce it, and
// a panic is reported as a failure of the test in which it occurred.
func Seeded(t *testing.T, test func(rng random.Source)) {
   t.Helper()
   t.Cleanup(func() {
      if t.Failed() {
         t.Logf("reproduce with: BST_SEED=%d go test -count 1 -run '%s'", baseSeed, pattern(t.Name()))
      }
   })
   defer func() {
      if r := recover(); r != nil {
         t.Fatalf("panic: %v\n\n%s", r, debug.Stack())
      }
   }()
   hash := fnv.New64a()
   hash.Write([]byte(t.Name()))
   test(random.New(baseSeed ^ hash.Sum64()))
}

// Returns a pattern for `go test -run` that matches exactly the named test.
func pattern(name string) string {
   parts := strings.Split(name, "/")
   for i, part := range parts {
      parts[i] = "^" + regexp.QuoteMeta(part) + "$"
   }
   return strings.Join(parts, "/")
}

// NewSeeded creates a new, empty instance of the given list, seeded from the
// given generator if the list draws random values of its own, so that it
// behaves the same way every time it is given the same seed.
func NewSeeded(impl List, rng random.Source) List {
   instance := impl.New()
   if seeded, ok := instance.(interface{ Seed(uint64) }); ok {
      seeded.Seed(rng.Uint64())
   }
   return instance
}
