load "docs/plots/colors.gnuplot"

set terminal svg size 400,240 dynamic round font "KaTeX_Main,10"

set xlabel "Δ" font "Katex_Main,16" offset 25,2.5
set ylabel "Γ" font "Katex_Main,16" offset 7.5,8 rotate by 0

set tmargin 3
set bmargin 1.5
set lmargin 5
set rmargin 5

set key width 0
set key height 0
set key spacing 1.25
set key samplen 4
set key reverse Left
set key horizontal outside left top
set key at graph 0.6, screen 1
set key font "KaTeX_Main,10"

set style line 1000 dashtype 3 lw 0.1 ps 0 lc rgb GRAY

set ytics ("1" 1, "2" 2, "5/3" 5./3, "3/2" 1.5, "√2" sqrt(2), "4/3" 4./3.)
set xtics ("2" 2, "1+√2" 1.+sqrt(2), "3" 3, "7/2" 3.5, "4/3" 4./3., "4" 4, "9/2" 4.5, "5" 5)

set grid ytics back ls 1000
set grid xtics back ls 1000

set border 3 front lt 1 lw 0 lc black

set xrange [2:4.8]
set yrange [1:2.1]

plot (x-1)/1 with lines axes x1y1 dt 4 lc rgb PURPLE lw 1 title "Γ ≤ (Δ – 1)", \
     (x+1)/x with lines axes x1y1 dt 1 lc rgb CYAN   lw 1 title "Γ ≥ (Δ + 1) / Δ", \
     'docs/plots/figures/polytope/polytope.csv' using 1:2 with points pt 7 lc rgb GRAY ps 0.1 notitle

