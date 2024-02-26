#
# PARTITION
#
WIDTH  = 36
HEIGHT = 44

load "docs/plots/figures/graph.gnuplot"
load "docs/plots/components.gnuplot"
load "docs/plots/colors.gnuplot"
load "docs/plots/ruler.gnuplot"

X_OFFSET = -2
Y_OFFSET = 0

COLOR_1 = BLUE
COLOR_2 = ORANGE
COLOR_3 = CYAN
COLOR_4 = PINK

eval node(20,  1, COLOR_1)
eval node(23,  4, COLOR_1)
eval node(26,  7, COLOR_2)
eval node(23, 10, COLOR_2)
eval node(20, 13, COLOR_3)
eval node(23, 16, COLOR_2)
eval node(26, 19, COLOR_2)

eval text(20,  1, "A", BLACK)
eval text(23,  4, "B", BLACK)
eval text(26,  7, "G", BLACK)
eval text(23, 10, "F", BLACK)
eval text(20, 13, "C", BLACK)
eval text(23, 16, "D", BLACK)
eval text(26, 19, "E", BLACK)

eval link(20,  1, 17,  4, 0, 1, COLOR_1)
eval link(23,  4, 20,  7, 0, 1, COLOR_1)
eval link(26,  7, 29, 10, 0, 1, COLOR_2)
eval link(23, 10, 26, 13, 0, 1, COLOR_2)
eval link(20, 13, 17, 16, 0, 1, COLOR_3)
eval link(23, 16, 20, 19, 0, 1, COLOR_2)
eval link(26, 19, 23, 22, 0, 1, COLOR_2)

eval tree(17,  4, COLOR_1)
eval tree(20,  7, COLOR_1)
eval tree(29, 10, COLOR_2)
eval tree(26, 13, COLOR_2)
eval tree(17, 16, COLOR_3)
eval tree(20, 19, COLOR_2)
eval tree(23, 22, COLOR_2)

eval link(20,  1, 23,  4, 1, 1, BLACK)
eval link(23,  4, 26,  7, 1, 1, BLACK)
eval link(26,  7, 23, 10, 1, 1, BLACK)
eval link(23, 10, 20, 13, 1, 1, BLACK)

eval link(20, 13, 23, 16, 0, 1, COLOR_2)
eval link(23, 16, 26, 19, 0, 1, COLOR_2)



Y_OFFSET = 2


# ROOT
eval node(20, 26, COLOR_3)
eval text(20, 26, "C", BLACK)

eval link(20, 26, 10, 29, 1, 1, BLACK)
eval link(20, 26, 30, 29, 1, 1, BLACK)

# LEFT
eval node(10, 29, COLOR_1)
eval text(10, 29, "A", BLACK)

eval node(13, 32, COLOR_1)
eval text(13, 32, "B", BLACK)

eval link(10, 29, 13, 32, 1, 1, BLACK)
eval link(13, 32, 16, 35, 1, 1, BLACK)

eval link(10, 29,  7, 32, 0, 1,  COLOR_1)
eval link(13, 32, 10, 35, 0, 1,  COLOR_1)

eval tree( 7, 32, COLOR_1)
eval tree(10, 35, COLOR_1)
eval tree(16, 35, COLOR_3)

# RIGHT
eval node(30, 29, COLOR_2)
eval node(27, 32, COLOR_2)
eval node(24, 35, COLOR_2)
eval node(27, 38, COLOR_2)

eval text(30, 29, "G", BLACK)
eval text(27, 32, "F", BLACK)
eval text(24, 35, "D", BLACK)
eval text(27, 38, "E", BLACK)

eval tree(21, 38, COLOR_2)
eval tree(24, 41, COLOR_2)
eval tree(33, 32, COLOR_2)
eval tree(30, 35, COLOR_2)

eval link(30, 29, 27, 32, 1, 1, BLACK)
eval link(27, 32, 24, 35, 1, 1, BLACK)

eval link(30, 29, 33, 32, 0, 1,  COLOR_2)
eval link(27, 32, 30, 35, 0, 1,  COLOR_2)
eval link(24, 35, 21, 38, 0, 1,  COLOR_2)
eval link(24, 35, 27, 38, 0, 1,  COLOR_2)
eval link(27, 38, 24, 41, 0, 1,  COLOR_2)

plot NaN notitle