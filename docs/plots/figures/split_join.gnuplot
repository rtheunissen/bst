#
# SPLIT JOIN
#
WIDTH  = 36
HEIGHT = 40

load "docs/plots/figures/graph.gnuplot"
load "docs/plots/components.gnuplot"
load "docs/plots/colors.gnuplot"

COLOR_1 = BLUE
COLOR_2 = ORANGE

X_OFFSET = 0.5
Y_OFFSET = -4

# SPLIT LEFT SPINE
eval text( 6,  4.5, "9", BLACK)
eval text( 9,  7.5, "8", BLACK)
eval text(12, 10.5, "5", BLACK)
eval text(15, 13.5, "4", BLACK)

eval text( 6,  6, "A", BLACK)
eval text( 9,  9, "B", BLACK)
eval text(12, 12, "C", BLACK)
eval text(15, 15, "D", BLACK)

eval node( 6,  6, COLOR_1)
eval node( 9,  9, COLOR_1)
eval node(12, 12, COLOR_1)
eval node(15, 15, COLOR_1)

eval tree(3, 9, COLOR_1)
eval tree(6, 12, COLOR_1)
eval tree(9, 15, COLOR_1)
eval tree(12, 18, COLOR_1)

eval link( 6,  6,  9,  9, 1, 1, BLACK)
eval link( 9,  9, 12, 12, 1, 1, BLACK)
eval link(12, 12, 15, 15, 1, 1, BLACK)

eval link( 6,  6,  3,  9, 0, 1,  COLOR_1)
eval link( 9,  9,  6, 12, 0, 1,  COLOR_1)
eval link(12, 12,  9, 15, 0, 1,  COLOR_1)
eval link(15, 15, 12, 18, 0, 1,  COLOR_1)




X_OFFSET = 22.5

# SPLIT RIGHT SPINE
eval text(7,  4.5, "7", BLACK)
eval text(4,  7.5, "6", BLACK)
eval text(1, 10.5, "3", BLACK)

eval node(1, 12, COLOR_2)
eval node(4,  9, COLOR_2)
eval node(7,  6, COLOR_2)

eval text(7,  6, "Z", BLACK)
eval text(4,  9, "Y", BLACK)
eval text(1, 12, "X", BLACK)

eval link(7,  6, 4,  9, 1, 1, BLACK)
eval link(4,  9, 1, 12, 1, 1, BLACK)

eval link(7,  6, 10,  9, 0, 1,  COLOR_2)
eval link(4,  9,  7, 12, 0, 1,  COLOR_2)
eval link(1, 12,  4, 15, 0, 1,  COLOR_2)

eval tree(10, 9, COLOR_2)
eval tree( 7, 12, COLOR_2)
eval tree( 4, 15, COLOR_2)



X_OFFSET = 0
Y_OFFSET = -7

# JOIN
eval node(19, 25, COLOR_1)
eval node(22, 28, COLOR_1)
eval node(25, 31, COLOR_2)
eval node(22, 34, COLOR_2)
eval node(19, 37, COLOR_1)
eval node(22, 40, COLOR_1)
eval node(25, 43, COLOR_2)

eval text(19, 25, "A", BLACK)
eval text(22, 28, "B", BLACK)
eval text(25, 31, "Z", BLACK)
eval text(22, 34, "Y", BLACK)
eval text(19, 37, "C", BLACK)
eval text(22, 40, "D", BLACK)
eval text(25, 43, "X", BLACK)

eval link(19, 25, 22, 28, 0, 1, BLACK)
eval link(22, 28, 25, 31, 0, 1, BLACK)
eval link(25, 31, 22, 34, 0, 1, BLACK)
eval link(22, 34, 19, 37, 0, 1, BLACK)
eval link(19, 37, 22, 40, 0, 1, BLACK)
eval link(22, 40, 25, 43, 0, 1, BLACK)

eval link(19, 25, 16, 28, 0, 1,  COLOR_1)
eval link(22, 28, 19, 31, 0, 1,  COLOR_1)
eval link(25, 31, 28, 34, 0, 1,  COLOR_2)
eval link(22, 34, 25, 37, 0, 1,  COLOR_2)
eval link(19, 37, 16, 40, 0, 1,  COLOR_1)
eval link(22, 40, 19, 43, 0, 1,  COLOR_1)
eval link(25, 43, 28, 46, 0, 1,  COLOR_2)

eval tree(16, 28, COLOR_1)
eval tree(19, 31, COLOR_1)
eval tree(16, 40, COLOR_1)
eval tree(19, 43, COLOR_1)

eval tree(28, 34, COLOR_2)
eval tree(25, 37, COLOR_2)
eval tree(28, 46, COLOR_2)

eval text(19, 23.5, "9", BLACK)
eval text(22, 26.5, "8", BLACK)
eval text(25, 29.5, "7", BLACK)
eval text(22, 32.5, "6", BLACK)
eval text(19, 35.5, "5", BLACK)
eval text(22, 38.5, "4", BLACK)
eval text(25, 41.5, "3", BLACK)

plot NaN notitle