load "docs/plots/benchmarks/balancers/graph.gnuplot"
load "docs/plots/functions.gnuplot"

SVG = sprintf("docs/benchmarks/svg/balancers/%s/%s__%s.svg", GROUP, MEASUREMENT, SMOOTH)
system mkdir(SVG)
set output SVG
print SVG

set table $TABLE
plot for [BALANCER in @GROUP] DATA.'/'.BALANCER using (@x):(@y) smooth unique
unset table

#set title "{/:Bold ".OPERATION."}"

plot for [i=1:words(@GROUP)] $TABLE \
    index (i-1) \
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

    set table $TABLE
    plot for [BALANCER in @GROUP] DATA.'/'.BALANCER using (@x):(filter('Distribution', DISTRIBUTION, @y)) smooth unique
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
