#
# ROTATIONS
#
WIDTH  = 36
HEIGHT = 11

load "docs/plots/figures/graph.gnuplot"
load "docs/plots/components.gnuplot"
load "docs/plots/colors.gnuplot"

COLOR_1 = BLUE
COLOR_2 = CYAN
COLOR_3 = ORANGE

dx = 2
dy = 3

X_OFFSET = 4
Y_OFFSET = 2

eval text(14, 1, "ROTATE RIGHT", BLACK)
eval line(10, 2, 18, 2, 1, 1, BLACK)

eval text(14, 7, "ROTATE LEFT", BLACK)
eval line(18, 6, 10, 6, 1, 1, BLACK)







## LEFT

X_OFFSET = 4
Y_OFFSET = 4


# (A)
#
eval node(dx, dy, COLOR_1)
eval link(dx, dy, 0, 2*dy, 0, 1, COLOR_1)
eval text(dx, dy, "A", BLACK)
eval tree(0, 2*dy, COLOR_1)

# (B)
#
eval node(2*dx, 2*dy, COLOR_3)
eval text(2*dx, 2*dy, "B", BLACK)
eval link(dx, dy, 2*dx, 2*dy, 1, 1, BLACK)

# (C)
#
eval text(2*dx, 0, "C", BLACK)
eval node(2*dx, 0, COLOR_2)
eval tree(3*dx, dy, COLOR_2)
eval link(2*dx, 0, dx, dy, 1, 1, BLACK)
eval link(2*dx, 0, 3*dx, dy, 0, 1, COLOR_2)

eval link(0*dx, -2*dy, 2*dx, 0, 0, 3, BLACK)


## RIGHT

X_OFFSET = 24
Y_OFFSET = 4

eval link(0*dx, -2*dy, 2*dx, 0, 0, 3, BLACK)

# (A)
#
eval text(2*dx, 0, "A", BLACK)
eval node(2*dx, 0, COLOR_1)
eval tree(1*dx, dy, COLOR_1)
eval link(2*dx, 0, 1*dx, dy, 0, 1, COLOR_1)
eval link(2*dx, 0, 3*dx, 1*dy, 1, 1, BLACK)

# (B)
#
eval node(2*dx, 2*dy, COLOR_3)
eval text(2*dx, 2*dy, "B", BLACK)

# (C)
#
eval text(3*dx, dy, "C", BLACK)
eval node(3*dx, dy, COLOR_2)
eval tree(4*dx, 2*dy, COLOR_2)
eval link(3*dx, dy, 2*dx, 2*dy, 1, 1, BLACK)
eval link(3*dx, 1*dy, 4*dx, 2*dy, 0, 1, COLOR_2)

plot NaN notitle