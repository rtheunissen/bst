load "docs/plots/benchmarks/balancers/graph.gnuplot"
load "docs/plots/functions.gnuplot"

# Every strategy is tabulated into a datablock of its own, after a sentinel
# block, so that its data is always the second block of that datablock, or is
# missing when the strategy has no data. Tabulating every strategy into one
# datablock instead would shift the blocks of all the strategies that follow a
# strategy without data, which would plot their data under the wrong titles.
$SENTINEL << EOD
1 1
EOD

SVG = sprintf("docs/benchmarks/svg/balancers/%s/%s__%s.svg", GROUP, MEASUREMENT, SMOOTH)
system mkdir(SVG)
set output SVG
print SVG

do for [i=1:words(@GROUP)] {
    eval sprintf('set table $TABLE%d', i)
    plot $SENTINEL using 1:2 notitle, DATA.'/'.word(@GROUP,i) using (@x):(@y) smooth unique
    unset table
}

#set title "{/:Bold ".OPERATION."}"

plot for [i=1:words(@GROUP)] sprintf('$TABLE%d', i) \
    index 1 \
    using 1:2 \
    axes x1y2 \
    smooth @SMOOTH \
    with linespoints \
    linestyle value(word(@GROUP,i)) \
    title word(@GROUP,i)

do for [DISTRIBUTION in DISTRIBUTIONS] {

    SVG = sprintf("docs/benchmarks/svg/balancers/%s/%s/%s__%s.svg", GROUP, DISTRIBUTION, MEASUREMENT, SMOOTH)
    system mkdir(SVG)
    set output SVG
    print SVG

    do for [i=1:words(@GROUP)] {
        eval sprintf('set table $TABLE%d', i)
        plot $SENTINEL using 1:2 notitle, DATA.'/'.word(@GROUP,i) using (@x):(filter('Distribution', DISTRIBUTION, @y)) smooth unique
        unset table
    }

    plot for [i=1:words(@GROUP)] sprintf('$TABLE%d', i) \
        index 1 \
        using 1:2 \
        axes x1y2 \
        smooth @SMOOTH \
        with linespoints \
        linestyle value(word(@GROUP,i)) \
        title word(@GROUP,i)
}
