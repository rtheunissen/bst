load "docs/plots/benchmarks/operations/graph.gnuplot"
load "docs/plots/functions.gnuplot"

unset title

set tmargin 3

if (words(@GROUP)) > 4 {
    set tmargin 4
}
if (words(@GROUP)) > 6 {
    set tmargin 5
}

SVG = sprintf("docs/benchmarks/svg/operations/%s/%s/%s__%s.svg", GROUP, OPERATION, MEASUREMENT, SMOOTH)
system mkdir(SVG)
set output SVG
print SVG

set table $TABLE
plot for [STRATEGY in @GROUP] DATA.'/'.STRATEGY using (@x):(@y) smooth @NORMAL
unset table

plot for [i=1:words(@GROUP)] $TABLE \
    index (i-1) \
    using 1:2 \
    axes x1y2 \
    smooth @SMOOTH \
    with linespoints \
    linestyle value(word(@GROUP,i)) \
    title word(@GROUP,i)

do for [DISTRIBUTION in DISTRIBUTIONS] {

    SVG = sprintf("docs/benchmarks/svg/operations/%s/%s/%s/%s__%s.svg", GROUP, OPERATION, DISTRIBUTION, MEASUREMENT, SMOOTH)
    system mkdir(SVG)
    set output SVG
    print SVG

    set table $TABLE
    plot for [STRATEGY in @GROUP] DATA.'/'.STRATEGY using (@x):(filter('Distribution', DISTRIBUTION, @y)) smooth @NORMAL
    unset table

    plot for [i=1:words(@GROUP)] $TABLE \
        index (i-1) \
        using 1:2 \
        axes x1y2 \
        smooth @SMOOTH \
        with linespoints \
        linestyle value(word(@GROUP,i)) \
        title word(@GROUP,i)
}