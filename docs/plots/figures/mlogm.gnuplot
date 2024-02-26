load "docs/plots/functions.gnuplot"
load "docs/plots/colors.gnuplot"

set terminal svg size 400,150 dynamic round font "KaTeX_Main,10"

set tmargin 1
set bmargin 2

set lmargin 1
set rmargin 4

set key width -3
set key height 0
set key spacing 1.25
set key samplen 4
set key reverse Left
set key horizontal outside center top
set key at graph 0.5, screen 1
#set key font "KaTeX_Main,10"

set style line 1000 dashtype 3 lw 0.1 ps 0 lc rgb GRAY

set grid xtics back ls 1000

set border 3 front lt 1 lw 0 lc black

set xrange [1:10000]

set logscale x 10

#set format x "10^%L"

set y2tics
set xtics

unset ytics

plot 1*logb(x, 2)  with linespoints axes x1y2 dt 1 pn 2 lc rgb BLUE   lw 1 title   "log_{2}(x)", \
    11*logb(x, 12) with linespoints axes x1y2 dt 4 pn 2 lc rgb CYAN   lw 1 title  "11 * log_{12}(x)", \
    32*logb(x, 5)  with linespoints axes x1y2 dt 5 pn 2 lc rgb ORANGE lw 1 title   "32 * log_{5}(x)"


