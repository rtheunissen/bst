load "docs/plots/colors.gnuplot"

##################################################################
#
#           STRATEGIES
#
##################################################################

AVLBottomUp                = 1
AVLTopDown                 = 2
AVLWeakTopDown             = 3
AVLWeakBottomUp            = 4
AVLRelaxedTopDown          = 5
AVLRelaxedBottomUp         = 6
RedBlackBottomUp           = 7
RedBlackTopDown            = 8
RedBlackRelaxedBottomUp    = 9
RedBlackRelaxedTopDown     = 10
LBSTBottomUp               = 11
LBSTTopDown                = 12
LBSTRelaxed                = 13
WBSTBottomUp               = 14
WBSTTopDown                = 15
WBSTRelaxed                = 16
TreapTopDown               = 17
TreapFingerTree            = 18
Randomized                 = 19
Zip                        = 20
Splay                      = 21
Conc                       = 22


RED       = "#F44336"
YELLOW    = "#FFAB00"
GREEN     = "#00C853"
CYAN      = "#29C3F6"
BLUE      = "#3B75EA"
PURPLE    = "#D336C8"
BLACK     = "#000000"
PINK      = "#EE82EE"
BROWN     = "#9F650D"
ORANGE    = "#EC6F1A"

POINT_CIRCLE_HOLLOW         = 6
POINT_CIRCLE_FILLED         = 7
POINT_TRIANGLE_HOLLOW_NORTH = 8
POINT_TRIANGLE_FILLED_NORTH = 9
POINT_TRIANGLE_HOLLOW_SOUTH = 10
POINT_TRIANGLE_FILLED_SOUTH = 11
POINT_DIAMOND_FILLED        = 13
POINT_PENTAGON_HOLLOW       = 14
POINT_LINE_VERTICAL         = 16
POINT_CROSS                 = 17
POINT_CROSS_VERTICAL        = 18
POINT_SQUARE_HOLLOW         = 19

DASH_TYPE_NORMAL = 1
DASH_TYPE_DASHED = 2
DASH_TYPE_DOTTED = 3
DASH_TYPE_MIXED1 = 4
DASH_TYPE_MIXED2 = 5

set style line AVLBottomUp                  dashtype DASH_TYPE_NORMAL ps 1 lw 0.5 pt POINT_TRIANGLE_FILLED_NORTH pn 2 lc rgb BLACK
set style line AVLTopDown                   dashtype DASH_TYPE_DASHED ps 1 lw 1   pt POINT_TRIANGLE_FILLED_SOUTH pn 2 lc rgb CYAN
set style line AVLWeakBottomUp              dashtype DASH_TYPE_NORMAL ps 1 lw 1   pt POINT_TRIANGLE_HOLLOW_NORTH pn 2 lc rgb GREEN
set style line AVLWeakTopDown               dashtype DASH_TYPE_MIXED2 ps 1 lw 1   pt POINT_TRIANGLE_HOLLOW_SOUTH pn 2 lc rgb YELLOW
set style line AVLRelaxedBottomUp           dashtype DASH_TYPE_NORMAL ps 1 lw 1   pt POINT_TRIANGLE_HOLLOW_NORTH pn 2 lc rgb PURPLE
set style line AVLRelaxedTopDown            dashtype DASH_TYPE_MIXED1 ps 1 lw 1   pt POINT_TRIANGLE_HOLLOW_SOUTH pn 2 lc rgb BROWN
set style line RedBlackBottomUp             dashtype DASH_TYPE_NORMAL ps 1 lw 1   pt POINT_TRIANGLE_HOLLOW_NORTH pn 2 lc rgb RED
set style line RedBlackTopDown              dashtype DASH_TYPE_NORMAL ps 1 lw 1   pt POINT_TRIANGLE_HOLLOW_SOUTH pn 2 lc rgb BLUE
set style line RedBlackRelaxedBottomUp      dashtype DASH_TYPE_DOTTED ps 1 lw 1   pt POINT_CIRCLE_HOLLOW pn 2 lc rgb ORANGE
set style line RedBlackRelaxedTopDown       dashtype DASH_TYPE_MIXED1 ps 1 lw 1   pt POINT_CIRCLE_FILLED pn 2 lc rgb PINK

set style line LBSTBottomUp                 dashtype DASH_TYPE_NORMAL ps 1 lw 1   pt POINT_CIRCLE_HOLLOW pn 2 lc rgb BLUE
set style line LBSTTopDown                  dashtype DASH_TYPE_DOTTED ps 1 lw 1   pt POINT_CIRCLE_FILLED pn 2 lc rgb GREEN
set style line LBSTRelaxed                  dashtype DASH_TYPE_MIXED1 ps 1 lw 1   pt POINT_DIAMOND_FILLED pn 2 lc rgb YELLOW

set style line WBSTBottomUp                 dashtype DASH_TYPE_NORMAL ps 1 lw 1   pt POINT_TRIANGLE_HOLLOW_NORTH pn 2 lc rgb BLACK
set style line WBSTTopDown                  dashtype DASH_TYPE_MIXED1 ps 1 lw 1   pt POINT_TRIANGLE_FILLED_NORTH pn 2 lc rgb CYAN
set style line WBSTRelaxed                  dashtype DASH_TYPE_MIXED2 ps 1 lw 1   pt POINT_TRIANGLE_HOLLOW_SOUTH pn 2 lc rgb PINK

set style line TreapTopDown                 dashtype DASH_TYPE_MIXED2 ps 1 lw 1   pt POINT_PENTAGON_HOLLOW pn 2 lc rgb GREEN
set style line TreapFingerTree              dashtype DASH_TYPE_NORMAL ps 1 lw 1   pt POINT_LINE_VERTICAL   pn 2 lc rgb BLUE
set style line Randomized                   dashtype DASH_TYPE_NORMAL ps 1 lw 1   pt POINT_CROSS           pn 2 lc rgb YELLOW
set style line Zip                          dashtype DASH_TYPE_NORMAL ps 1 lw 1   pt POINT_CROSS_VERTICAL  pn 2 lc rgb RED

set style line Splay                        dashtype DASH_TYPE_NORMAL ps 1 lw 1   pt POINT_SQUARE_HOLLOW pn 2 lc rgb CYAN
set style line Conc                         dashtype DASH_TYPE_DOTTED ps 1 lw 1   pt 20 pn 2 lc rgb BROWN


##################################################################
#
#           GROUPS
#
##################################################################

AVLRedBlack            = "AVLBottomUp AVLTopDown RedBlackBottomUp RedBlackTopDown"
AVLWeak                = "AVLBottomUp AVLTopDown AVLWeakBottomUp AVLWeakTopDown"
AVLWeakRedBlack        = "AVLBottomUp RedBlackBottomUp AVLWeakBottomUp AVLWeakTopDown"
AVLRelaxed             = "AVLBottomUp AVLWeakTopDown AVLRelaxedBottomUp AVLRelaxedTopDown"
RedBlackRelaxed        = "RedBlackBottomUp RedBlackTopDown RedBlackRelaxedBottomUp RedBlackRelaxedTopDown"
RankBalanced           = "AVLBottomUp AVLWeakTopDown AVLRelaxedBottomUp RedBlackRelaxedTopDown"
WeightBalanced         = "LBSTBottomUp LBSTTopDown WBSTBottomUp WBSTTopDown"
WeightBalancedRelaxed  = "LBSTBottomUp LBSTRelaxed WBSTBottomUp WBSTRelaxed"
Probabilistic          = "TreapTopDown TreapFingerTree Randomized Zip"
SelfAdjusting          = "Splay AVLBottomUp LBSTRelaxed TreapTopDown"
Relaxed                = "AVLRelaxedBottomUp RedBlackRelaxedTopDown LBSTRelaxed WBSTRelaxed"
Persistent             = "Conc Splay AVLBottomUp AVLRelaxedBottomUp RedBlackBottomUp LBSTBottomUp LBSTRelaxed TreapTopDown"
Conclusions            = "AVLTopDown AVLRelaxedBottomUp RedBlackBottomUp RedBlackRelaxedTopDown LBSTBottomUp LBSTRelaxed TreapTopDown Splay"

GROUPS = "\
    AVLRedBlack \
    AVLWeak \
    AVLWeakRedBlack \
    AVLRelaxed \
    RedBlackRelaxed \
    RankBalanced \
    WeightBalanced \
    WeightBalancedRelaxed \
    Probabilistic \
    SelfAdjusting \
    Relaxed \
    Persistent \
    Conclusions \
    "

OPERATIONS = "\
    Insert \
    InsertPersistent \
    InsertDelete \
    InsertDeletePersistent \
    "

DISTRIBUTIONS = "\
    Uniform \
    Zipf \
    Maximum \
    Skewed \
    Normal \
    "

do for [GROUP in GROUPS] {
    do for [OPERATION in OPERATIONS] {

        ##################################################################
        #
        #           Allocations
        #
        ##################################################################

        MEASUREMENT = 'Allocations'

        set xlabel "Operations / 10^6"
        set ylabel "{/:Bold Allocations } / log_2(Size)"

        DATA = sprintf("docs/benchmarks/data/operations/measurements/%s", OPERATION)

        x = "column('Position')/(column('Scale')/10)"
        y = "column('Allocations')/column('Step')/log2(column('Size'))"

        set xrange [0.5:*]
        set format y2 "%.2f"

        NORMAL = "unique"
        SMOOTH = "sbezier"
        load "docs/plots/benchmarks/operations/lines.gnuplot"

        NORMAL = "unique"
        SMOOTH = "unique"
        load "docs/plots/benchmarks/operations/lines.gnuplot"

        NORMAL = "unique"
        SMOOTH = "cumulative"
        load "docs/plots/benchmarks/operations/lines.gnuplot"

        ##################################################################
        #
        #           MAXIMUM PATH LENGTH
        #
        ##################################################################

        MEASUREMENT = 'MaximumPathLength'

        set xlabel "Operations / 10^6"
        set ylabel "{/:Bold Maximum Path Length } / log_2(Size)"

        x = "column('Position')/(column('Scale')/10)"
        y = "column('MaximumPathLength')/log2(column('Size'))"

        DATA = sprintf("docs/benchmarks/data/operations/measurements/%s", OPERATION)

        set xrange [0.5:*]
        set format y2 "%.2f"

        NORMAL = "unique"
        SMOOTH = "sbezier"
        load "docs/plots/benchmarks/operations/lines.gnuplot"

        NORMAL = "unique"
        SMOOTH = "unique"
        load "docs/plots/benchmarks/operations/lines.gnuplot"

        ##################################################################
        #
        #           MAXIMUM PATH LENGTH (LOGARITHMIC)
        #
        ##################################################################

        MEASUREMENT = 'LogMaximumPathLength'

        set xlabel "Operations / 10^6"
        set ylabel "log_2({/:Bold Maximum Path Length } / log_2(Size))"

        DATA = sprintf("docs/benchmarks/data/operations/measurements/%s", OPERATION)

        set logscale y2 2

        set xrange [0.5:*]
        set format y2 "%L"

        x = "column('Position')/(column('Scale')/10)"
        y = "column('MaximumPathLength')/log2(column('Size'))"

        NORMAL = "unique"
        SMOOTH = "sbezier"
        load "docs/plots/benchmarks/operations/lines.gnuplot"

        NORMAL = "unique"
        SMOOTH = "unique"
        load "docs/plots/benchmarks/operations/lines.gnuplot"

        unset logscale y2


        ##################################################################
        #
        #           AVERAGE PATH LENGTH
        #
        ##################################################################

        MEASUREMENT = 'AveragePathLength'

        set xlabel "Operations / 10^6"
        set ylabel "{/:Bold Average Path Length } / log_2(Size)"

        DATA = sprintf("docs/benchmarks/data/operations/measurements/%s", OPERATION)

        set xrange [0.5:*]
        set format y2 "%.3f"

        x = "column('Position')/(column('Scale')/10)"
        y = "column('AveragePathLength')/log2(column('Size'))"

        NORMAL = "unique"
        SMOOTH = "sbezier"
        load "docs/plots/benchmarks/operations/lines.gnuplot"

        NORMAL = "unique"
        SMOOTH = "unique"
        load "docs/plots/benchmarks/operations/lines.gnuplot"


        ##################################################################
        #
        #           AVERAGE PATH LENGTH (LOGARITHMIC)
        #
        ##################################################################

        MEASUREMENT = 'LogAveragePathLength'

        set xlabel "Operations / 10^6"
        set ylabel "log_2({/:Bold Average Path Length } / log_2(Size))"

        DATA = sprintf("docs/benchmarks/data/operations/measurements/%s", OPERATION)

        set logscale y2 2

        set xrange [0.5:*]
        set format y2 "%L"

        x = "column('Position')/(column('Scale')/10)"
        y = "column('AveragePathLength')/log2(column('Size'))"

        NORMAL = "unique"
        SMOOTH = "sbezier"
        load "docs/plots/benchmarks/operations/lines.gnuplot"

        NORMAL = "unique"
        SMOOTH = "unique"
        load "docs/plots/benchmarks/operations/lines.gnuplot"

        unset logscale y2

        ##################################################################
        #
        #           ROTATIONS
        #
        ##################################################################

        MEASUREMENT = "Rotations"

        set xlabel "{Operations / 10^6}"
        set ylabel "{/:Bold Rotations} / Operations"

        x = "column('Position')/(column('Scale')/10)"
        y = "column('Rotations')/column('Step')"

        DATA = sprintf("docs/benchmarks/data/operations/measurements/%s", OPERATION)

        set xrange [0.5:*]
        set format y2 "%.2f"

        NORMAL = "unique"
        SMOOTH = "sbezier"
        load "docs/plots/benchmarks/operations/lines.gnuplot"

        NORMAL = "unique"
        SMOOTH = "unique"
        load "docs/plots/benchmarks/operations/lines.gnuplot"

        ##################################################################
        #
        #           DURATION
        #
        ##################################################################

        MEASUREMENT = 'Duration'

        set xlabel "Operations / 10^6"
        set ylabel "{/:Bold Duration } in milliseconds"

        x = "column('Position')/(column('Scale')/10)"
        y = "column('Duration')/1000000" # nano / 10^6 is 1k

        set xrange [0.5:*]
        set format y2 "%.2f"

        DATA = sprintf("docs/benchmarks/data/operations/benchmarks/%s", OPERATION)

        NORMAL = "sbezier"
        SMOOTH = "mcsplines"
        load "docs/plots/benchmarks/operations/lines.gnuplot"

        NORMAL = "unique"
        SMOOTH = "unique"
        load "docs/plots/benchmarks/operations/lines.gnuplot"

        ##################################################################
        #
        #           TOTAL DURATION
        #
        ##################################################################

        MEASUREMENT = 'Duration'

        DATA = sprintf("docs/benchmarks/data/operations/benchmarks/%s", OPERATION)

        set xlabel "Operations / 10^6"
        set ylabel "{/:Bold Total Duration } in seconds"

        x = "column('Position')/(column('Scale')/10)"
        y = "column('Duration')/1000/1000/1000"

        set xrange [0:*]
        set format y2 "%.2f"

        NORMAL = "unique"
        SMOOTH = "cumulative"
        load "docs/plots/benchmarks/operations/lines.gnuplot"
    }
}