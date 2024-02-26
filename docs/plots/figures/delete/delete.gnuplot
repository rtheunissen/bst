load "docs/plots/colors.gnuplot"
load "docs/plots/functions.gnuplot"

set terminal svg size 400,250 dynamic round font "KaTeX_Main,10" lw 1 name "delete"

set samples 10000

set border 6 front lc '#000000' lt 0 lw 0

set style line 1000 dashtype 1 lw 0.1 ps 0 lc rgb GRAY

set style line 1  dashtype 1 ps 1 lw 1 pt 1  pn 2 lc rgb BLACK
set style line 2  dashtype 3 ps 1 lw 1 pt 2  pn 2 lc rgb BROWN
set style line 3  dashtype 1 ps 1 lw 1 pt 3  pn 2 lc rgb BLUE
set style line 4  dashtype 5 ps 1 lw 1 pt 12 pn 2 lc rgb CYAN
set style line 5  dashtype 1 ps 1 lw 1 pt 8  pn 2 lc rgb YELLOW
set style line 6  dashtype 4 ps 1 lw 1 pt 10 pn 2 lc rgb ORANGE

set grid xtics back linestyle 1000
#set grid y2tics back linestyle 1000

set tmargin 2.5
set lmargin 5
set rmargin 5
set bmargin 3

set key width 0
set key height 0
set key spacing 1.25
set key samplen 4
set key reverse Left
set key horizontal outside center top
set key at graph 0.5, screen 1
set key font "KaTeX_Main,8"

unset xtics
unset ytics
unset mxtics
unset mytics
unset my2tics
unset mx2tics
unset x2tics
unset y2tics

set xtics 1,1,10 offset 0,0.5 font "KaTeX_Main,8"

set y2tics offset 0,0 font "KaTeX_Main,8"
set y2tics 0.01

set xlabel "Random insertions and deletions / 10^8"
set ylabel "{/:Bold Average Path Length} / log_2(Size = 1000)"

set xlabel offset 0,0.5
set ylabel offset 0,0

set format y2 "%.2f"

set y2range [1.00:1.20]
set  xrange [0.5:10]

# "Step", "Steps", "Size", "Samples", "AveragePathLength"

x = "column('Step')/column('Steps')*10"
y = "column('AveragePathLength')/log2(column('Size'))"

set fit quiet
set fit logfile '/dev/null'

# Fit functions
fit_1(x) = m_1 * x + c_1
fit_2(x) = m_2 * x + c_2
fit_3(x) = m_3 * x + c_3
fit_4(x) = m_4 * x + c_4
fit_5(x) = m_5 * x + c_5
fit_6(x) = m_6 * x + c_6

fit fit_1(x) 'docs/plots/figures/delete/data/hibbard.csv'   using (@x):(@y) via m_1,c_1
fit fit_2(x) 'docs/plots/figures/delete/data/knuth.csv'     using (@x):(@y) via m_2,c_2
fit fit_3(x) 'docs/plots/figures/delete/data/random.csv'    using (@x):(@y) via m_3,c_3
fit fit_4(x) 'docs/plots/figures/delete/data/symmetric.csv' using (@x):(@y) via m_4,c_4
fit fit_5(x) 'docs/plots/figures/delete/data/smaller.csv'   using (@x):(@y) via m_5,c_5
fit fit_6(x) 'docs/plots/figures/delete/data/larger.csv'    using (@x):(@y) via m_6,c_6

plot fit_1(x) with linespoints axes x1y2 ls 1 title 'Hibbard', \
     fit_2(x) with linespoints axes x1y2 ls 2 title 'Knuth', \
     fit_3(x) with linespoints axes x1y2 ls 3 title 'Randomized', \
     fit_4(x) with linespoints axes x1y2 ls 4 title 'Symmetric', \
     fit_5(x) with linespoints axes x1y2 ls 5 title 'Smaller', \
     fit_6(x) with linespoints axes x1y2 ls 6 title 'Larger'