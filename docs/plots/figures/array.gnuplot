#
# ARRAY
#
WIDTH  = 40
HEIGHT = 4

load "docs/plots/figures/graph.gnuplot"
load "docs/plots/components.gnuplot"
load "docs/plots/ruler.gnuplot"
load "docs/plots/colors.gnuplot"

COLOR_1 = BLUE
COLOR_2 = ORANGE
COLOR_3 = CYAN
COLOR_4 = PINK
COLOR_4 = "#DDDDDD"

dx = real(3.0)
dy = sqrt(3.0)

X_OFFSET = 9.5
Y_OFFSET = 0.25

do for [i=0:7] {
    eval text(dx*i, dy/4, sprintf("%d", i), BLACK)
    eval line(dx*i - dx/2, dy*1, dx*i - dx/2, dy*2, 0, 1, BLACK)
}
eval line(dx*0 - dx/2, dy*1, dx*8 - dx/2, dy*1, 0, 1, BLACK)
eval line(dx*0 - dx/2, dy*2, dx*8 - dx/2, dy*2, 0, 1, BLACK)
eval line(dx*8 - dx/2, dy*1, dx*8 - dx/2, dy*2, 0, 1, BLACK)

eval text(dx*0, dy*1 + dy/2, "A", BLACK)
eval text(dx*1, dy*1 + dy/2, "B", BLACK)
eval text(dx*2, dy*1 + dy/2, "C", BLACK)
eval text(dx*3, dy*1 + dy/2, "D", BLACK)
eval text(dx*4, dy*1 + dy/2, "E", BLACK)
eval text(dx*5, dy*1 + dy/2, "F", BLACK)
eval text(dx*6, dy*1 + dy/2, "G", BLACK)








plot NaN notitle