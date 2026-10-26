package trees

import (
	"testing"

	"github.com/rtheunissen/bst/types/list"
	"github.com/rtheunissen/bst/utility/number"
	"github.com/rtheunissen/bst/utility/number/distribution"
)

func TestBST(t *testing.T) {
   list.TestSuite{
      Scale: 100,
      Tests: []list.Test{
         list.TestInsert,
         list.TestInsertPersistent,
         list.TestSelect,
         list.TestSelectPersistent,
         list.TestSelectAfterInsert,
         list.TestSelectAfterInsertPersistent,
         list.TestUpdate,
         list.TestUpdatePersistent,
         list.TestDelete,
         list.TestDeletePersistent,
         list.TestInsertDelete,
         list.TestInsertDeletePersistent,
         list.TestSplit,
         list.TestJoin,
         list.TestJoinFromSplit,
         list.TestJoinAfterInsertDelete,
         list.TestPersistentVersions,
      },
      Distributions: []number.Distribution{
         &distribution.Uniform{},
         &distribution.Normal{},
         &distribution.Skewed{},
         &distribution.Zipf{},
         &distribution.Maximum{},
         &distribution.Minimum{},
         &distribution.Ascending{},
         &distribution.Descending{},
         &distribution.Queue{},
      },
      Lists: implementations,
   }.Run(t)
}

// Every list implementation, as a template from which to create new instances.
var implementations = []list.List{
   &AVLBottomUp{},
   &AVLTopDown{},
   &AVLWeakTopDown{},
   &AVLWeakBottomUp{},
   &AVLRelaxedTopDown{},
   &AVLRelaxedBottomUp{},
   &RedBlackBottomUp{},
   &RedBlackTopDown{},
   &RedBlackRelaxedBottomUp{},
   &RedBlackRelaxedTopDown{},
   &WBSTBottomUp{},
   &WBSTTopDown{},
   &WBSTRelaxed{},
   &LBSTBottomUp{},
   &LBSTTopDown{},
   &LBSTRelaxed{},
   &TreapBottomUp{},
   &TreapTopDown{},
   &TreapFingerTree{},
   &Randomized{},
   &Zip{},
   &Splay{},
   &Conc{},
   &LLRB{},
   &AA{},
}
