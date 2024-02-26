#
# LINKED LIST
#
WIDTH  = 36
HEIGHT = 2

dx = real(3)
dy = real(4)

load "docs/plots/figures/graph.gnuplot"
load "docs/plots/components.gnuplot"
load "docs/plots/ruler.gnuplot"
load "docs/plots/colors.gnuplot"

X_OFFSET = 6 + dx/2
Y_OFFSET = 0

eval node(dx * 0, 1, BLACK)
eval node(dx * 1, 1, BLACK)
eval node(dx * 2, 1, BLACK)
eval node(dx * 3, 1, BLACK)
eval node(dx * 4, 1, BLACK)
eval node(dx * 5, 1, BLACK)
eval node(dx * 6, 1, BLACK)

eval text(dx * 0, 1, "A", BLACK)
eval text(dx * 1, 1, "B", BLACK)
eval text(dx * 2, 1, "C", BLACK)
eval text(dx * 3, 1, "D", BLACK)
eval text(dx * 4, 1, "E", BLACK)
eval text(dx * 5, 1, "F", BLACK)
eval text(dx * 6, 1, "G", BLACK)

eval link(dx * 0, 1, dx * 1, 1, 1, 1, BLACK)
eval link(dx * 1, 1, dx * 2, 1, 1, 1, BLACK)
eval link(dx * 2, 1, dx * 3, 1, 1, 1, BLACK)
eval link(dx * 3, 1, dx * 4, 1, 1, 1, BLACK)
eval link(dx * 4, 1, dx * 5, 1, 1, 1, BLACK)
eval link(dx * 5, 1, dx * 6, 1, 1, 1, BLACK)

eval link(dx * 6, 1, dx * 7, 1, 1, 1, BLACK)
eval leaf(dx * 7, 1, BLACK)

plot NaN notitle