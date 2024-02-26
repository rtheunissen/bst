set terminal svg size 400,200 dynamic round font "KaTeX_Main,10"

set samples 1000

set border 6 front lc '#000000' lt -1 lw 0

set style line 1000 dashtype 1 lw 0.1 ps 0 lc rgb GRAY
set grid xtics back linestyle 1000

set xlabel offset 0,0.5
set ylabel offset 0,0

set tmargin 3
set bmargin 2.5
set lmargin 4
set rmargin 4

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
set y2tics autofreq offset 0,0 font "KaTeX_Main,8"

set datafile missing "NaN"


