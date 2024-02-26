#
# DELETE
#

WIDTH  = 36
HEIGHT = 21

load "docs/plots/functions.gnuplot"
load "docs/plots/figures/graph.gnuplot"
load "docs/plots/components.gnuplot"
load "docs/plots/ruler.gnuplot"
load "docs/plots/colors.gnuplot"

COLOR_1 = BLUE
COLOR_2 = ORANGE

dx = real(2)
dy = real(3)


#
# CENTER DIVIDE
#
eval line(18, 1, 18, 23, 0, 1, GRAY)


#
# TOP LEFT
#
X_OFFSET = 1
Y_OFFSET = -2

eval line(dx * 3 - 0.5, dy * 1 - 0.5, dx * 3 + 0.5, dy * 1 + 0.5, 0, 1, BLACK)
eval line(dx * 3 + 0.5, dy * 1 - 0.5, dx * 3 - 0.5, dy * 1 + 0.5, 0, 1, BLACK)

eval node(dx * 0, dy * 3, BLACK)
eval node(dx * 1, dy * 2, BLACK)
eval node(dx * 2, dy * 3, COLOR_1)
eval node(dx * 3, dy * 1, BLACK)
eval node(dx * 4, dy * 3, BLACK)
eval node(dx * 5, dy * 2, BLACK)
eval node(dx * 6, dy * 3, BLACK)
eval tree(dx * 1, dy * 4, COLOR_1)

eval text(dx * 0, dy * 3, "A", BLACK)
eval text(dx * 1, dy * 2, "B", BLACK)
eval text(dx * 2, dy * 3, "C", BLACK)
eval text(dx * 3, dy * 1, "D", BLACK)
eval text(dx * 4, dy * 3, "E", BLACK)
eval text(dx * 5, dy * 2, "F", BLACK)
eval text(dx * 6, dy * 3, "G", BLACK)

eval link(dx * 1, dy * 2, dx * 0, dy * 3, 0, 1, BLACK)
eval link(dx * 3, dy * 1, dx * 1, dy * 2, 1, 1, BLACK)
eval link(dx * 1, dy * 2, dx * 2, dy * 3, 1, 1, BLACK)
eval link(dx * 5, dy * 2, dx * 6, dy * 3, 0, 1, BLACK)
eval link(dx * 5, dy * 2, dx * 4, dy * 3, 0, 1, BLACK)
eval link(dx * 2, dy * 3, dx * 1, dy * 4, 0, 1, COLOR_1)
eval link(dx * 3, dy * 1, dx * 5, dy * 2, 0, 1, BLACK)


#
# TOP RIGHT
#
X_OFFSET = 23
Y_OFFSET = -2

eval line(dx * 3 - 0.5, dy * 1 - 0.5, dx * 3 + 0.5, dy * 1 + 0.5, 0, 1, BLACK)
eval line(dx * 3 + 0.5, dy * 1 - 0.5, dx * 3 - 0.5, dy * 1 + 0.5, 0, 1, BLACK)

eval node(dx * 0, dy * 3, BLACK)
eval node(dx * 1, dy * 2, BLACK)
eval node(dx * 2, dy * 3, BLACK)
eval node(dx * 3, dy * 1, BLACK)
eval node(dx * 4, dy * 3, COLOR_2)
eval node(dx * 5, dy * 2, BLACK)
eval node(dx * 6, dy * 3, BLACK)
eval tree(dx * 5, dy * 4, COLOR_2)

eval text(dx * 0, dy * 3, "A", BLACK)
eval text(dx * 1, dy * 2, "B", BLACK)
eval text(dx * 2, dy * 3, "C", BLACK)
eval text(dx * 3, dy * 1, "D", BLACK)
eval text(dx * 4, dy * 3, "E", BLACK)
eval text(dx * 5, dy * 2, "F", BLACK)
eval text(dx * 6, dy * 3, "G", BLACK)

eval link(dx * 1, dy * 2, dx * 0, dy * 3, 0, 1, BLACK)
eval link(dx * 3, dy * 1, dx * 1, dy * 2, 0, 1, BLACK)
eval link(dx * 1, dy * 2, dx * 2, dy * 3, 0, 1, BLACK)
eval link(dx * 5, dy * 2, dx * 6, dy * 3, 0, 1, BLACK)
eval link(dx * 5, dy * 2, dx * 4, dy * 3, 1, 1, BLACK)
eval link(dx * 4, dy * 3, dx * 5, dy * 4, 0, 1, COLOR_2)
eval link(dx * 3, dy * 1, dx * 5, dy * 2, 1, 1, BLACK)


#
# BOTTOM LEFT
#
X_OFFSET = 1
Y_OFFSET = 11

eval node(dx * 0, dy * 3, BLACK)
eval node(dx * 1, dy * 2, BLACK)
eval node(dx * 3, dy * 1, COLOR_1)
eval node(dx * 4, dy * 3, BLACK)
eval node(dx * 5, dy * 2, BLACK)
eval node(dx * 6, dy * 3, BLACK)

eval text(dx * 0, dy * 3, "A", BLACK)
eval text(dx * 1, dy * 2, "B", BLACK)
eval text(dx * 3, dy * 1, "C", BLACK)
eval text(dx * 4, dy * 3, "E", BLACK)
eval text(dx * 5, dy * 2, "F", BLACK)
eval text(dx * 6, dy * 3, "G", BLACK)

eval tree(dx * 2, dy * 3, COLOR_1)

eval link(dx * 1, dy * 2, dx * 0, dy * 3, 0, 1, BLACK)
eval link(dx * 3, dy * 1, dx * 1, dy * 2, 0, 1, BLACK)
eval link(dx * 1, dy * 2, dx * 2, dy * 3, 0, 1, BLACK)
eval link(dx * 5, dy * 2, dx * 6, dy * 3, 0, 1, BLACK)
eval link(dx * 5, dy * 2, dx * 4, dy * 3, 0, 1, BLACK)
eval link(dx * 3, dy * 1, dx * 5, dy * 2, 0, 1, BLACK)


#
# BOTTOM RIGHT
#
X_OFFSET = 23
Y_OFFSET = 11

eval node(dx * 0, dy * 3, BLACK)
eval node(dx * 1, dy * 2, BLACK)
eval node(dx * 2, dy * 3, BLACK)
eval node(dx * 3, dy * 1, COLOR_2)

eval node(dx * 5, dy * 2, BLACK)
eval node(dx * 6, dy * 3, BLACK)

eval text(dx * 0, dy * 3, "A", BLACK)
eval text(dx * 1, dy * 2, "B", BLACK)
eval text(dx * 2, dy * 3, "C", BLACK)
eval text(dx * 3, dy * 1, "E", BLACK)
eval text(dx * 5, dy * 2, "F", BLACK)
eval text(dx * 6, dy * 3, "G", BLACK)

eval tree(dx * 4, dy * 3, COLOR_2)

eval link(dx * 1, dy * 2, dx * 0, dy * 3, 0, 1, BLACK)
eval link(dx * 3, dy * 1, dx * 1, dy * 2, 0, 1, BLACK)
eval link(dx * 1, dy * 2, dx * 2, dy * 3, 0, 1, BLACK)
eval link(dx * 5, dy * 2, dx * 6, dy * 3, 0, 1, BLACK)
eval link(dx * 5, dy * 2, dx * 4, dy * 3, 0, 1, BLACK)
eval link(dx * 3, dy * 1, dx * 5, dy * 2, 0, 1, BLACK)

plot NaN notitle