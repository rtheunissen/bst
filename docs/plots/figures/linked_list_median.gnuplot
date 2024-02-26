#
# LINKED LIST (MEDIAN)
#
WIDTH  = 36
HEIGHT = 6

load "docs/plots/figures/graph.gnuplot"
load "docs/plots/components.gnuplot"
load "docs/plots/ruler.gnuplot"
load "docs/plots/colors.gnuplot"

dx = real(3)
dy = real(1)

X_OFFSET = 6
Y_OFFSET = 1

eval node(dx*1, dy*3, BLACK)
eval node(dx*2, dy*2, BLACK)
eval node(dx*3, dy*1, BLACK)
eval node(dx*4, dy*0, BLACK)
eval node(dx*5, dy*1, BLACK)
eval node(dx*6, dy*2, BLACK)
eval node(dx*7, dy*3, BLACK)

eval text(dx*1, dy*3, "A", BLACK)
eval text(dx*2, dy*2, "B", BLACK)
eval text(dx*3, dy*1, "C", BLACK)
eval text(dx*4, dy*0, "D", BLACK)
eval text(dx*5, dy*1, "E", BLACK)
eval text(dx*6, dy*2, "F", BLACK)
eval text(dx*7, dy*3, "G", BLACK)

eval link(dx * 1, dy*3, dx * 0, dy*4, 1, 1, BLACK)
eval link(dx * 2, dy*2, dx * 1, dy*3, 1, 1, BLACK)
eval link(dx * 3, dy*1, dx * 2, dy*2, 1, 1, BLACK)
eval link(dx * 4, dy*0, dx * 3, dy*1, 1, 1, BLACK)
eval link(dx * 4, dy*0, dx * 5, dy*1, 1, 1, BLACK)
eval link(dx * 5, dy*1, dx * 6, dy*2, 1, 1, BLACK)
eval link(dx * 6, dy*2, dx * 7, dy*3, 1, 1, BLACK)
eval link(dx * 7, dy*3, dx * 8, dy*4, 1, 1, BLACK)

eval leaf(dx * 0, dy*4, BLACK)
eval leaf(dx * 8, dy*4, BLACK)

plot NaN notitle