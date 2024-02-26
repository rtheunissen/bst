#
# BINARY SEARCH TREE
#
WIDTH  = 36
HEIGHT = 24

load "docs/plots/figures/graph.gnuplot"
load "docs/plots/components.gnuplot"
load "docs/plots/ruler.gnuplot"
load "docs/plots/colors.gnuplot"

COLOR_1 = BLUE
COLOR_2 = ORANGE

dx = real(3)
dy = real(3)


eval link( 9, 2,  9, 9, 0, 3, GRAY)
eval link(12, 2, 12, 8, 0, 3, GRAY)
eval link(15, 2, 15, 7, 0, 3, GRAY)
eval link(18, 2, 18, 6, 0, 3, GRAY)
eval link(21, 2, 21, 7, 0, 3, GRAY)
eval link(24, 2, 24, 8, 0, 3, GRAY)
eval link(27, 2, 27, 9, 0, 3, GRAY)

eval link( 9, 11,  9, 19, 0, 3, GRAY)
eval link(12, 10, 12, 16, 0, 3, GRAY)
eval link(15,  9, 15, 19, 0, 3, GRAY)
eval link(18,  8, 18, 13, 0, 3, GRAY)
eval link(21,  9, 21, 19, 0, 3, GRAY)
eval link(24, 10, 24, 16, 0, 3, GRAY)
eval link(27, 11, 27, 19, 0, 3, GRAY)






X_OFFSET = 9
Y_OFFSET = 1

eval node(dx*0, 0, BLACK)
eval node(dx*1, 0, BLACK)
eval node(dx*2, 0, BLACK)
eval node(dx*3, 0, BLACK)
eval node(dx*4, 0, BLACK)
eval node(dx*5, 0, BLACK)
eval node(dx*6, 0, BLACK)
eval leaf(dx*7, 0, BLACK)

eval text(dx*0, 0, "A", BLACK)
eval text(dx*1, 0, "B", BLACK)
eval text(dx*2, 0, "C", BLACK)
eval text(dx*3, 0, "D", BLACK)
eval text(dx*4, 0, "E", BLACK)
eval text(dx*5, 0, "F", BLACK)
eval text(dx*6, 0, "G", BLACK)

eval link(dx*0, 0, dx*1, 0, 1, 1, COLOR_2)
eval link(dx*1, 0, dx*2, 0, 1, 1, COLOR_2)
eval link(dx*2, 0, dx*3, 0, 1, 1, COLOR_2)
eval link(dx*3, 0, dx*4, 0, 1, 1, COLOR_2)
eval link(dx*4, 0, dx*5, 0, 1, 1, COLOR_2)
eval link(dx*5, 0, dx*6, 0, 1, 1, COLOR_2)
eval link(dx*6, 0, dx*7, 0, 1, 1, COLOR_2)






dx = real(3)
dy = real(1)

X_OFFSET = 6
Y_OFFSET = 7

eval leaf(dx*0, dy*4, BLACK)
eval node(dx*1, dy*3, BLACK)
eval node(dx*2, dy*2, BLACK)
eval node(dx*3, dy*1, BLACK)
eval node(dx*4, dy*0, BLACK)
eval node(dx*5, dy*1, BLACK)
eval node(dx*6, dy*2, BLACK)
eval node(dx*7, dy*3, BLACK)
eval leaf(dx*8, dy*4, BLACK)

eval text(dx*1, dy*3, "A", BLACK)
eval text(dx*2, dy*2, "B", BLACK)
eval text(dx*3, dy*1, "C", BLACK)
eval text(dx*4, dy*0, "D", BLACK)
eval text(dx*5, dy*1, "E", BLACK)
eval text(dx*6, dy*2, "F", BLACK)
eval text(dx*7, dy*3, "G", BLACK)


eval link(dx*1, dy*3, dx*0, dy*4, 1, 1, COLOR_1)
eval link(dx*2, dy*2, dx*1, dy*3, 1, 1, COLOR_1)
eval link(dx*3, dy*1, dx*2, dy*2, 1, 1, COLOR_1)
eval link(dx*4, dy*0, dx*3, dy*1, 1, 1, COLOR_1)

eval link(dx*4, dy*0, dx*5, dy*1, 1, 1, COLOR_2)
eval link(dx*5, dy*1, dx*6, dy*2, 1, 1, COLOR_2)
eval link(dx*6, dy*2, dx*7, dy*3, 1, 1, COLOR_2)
eval link(dx*7, dy*3, dx*8, dy*4, 1, 1, COLOR_2)








dx = real(3)
dy = real(3)

X_OFFSET = 9
Y_OFFSET = 14

eval text(dx * 0, dy * 2, "A", BLACK)
eval text(dx * 1, dy * 1, "B", BLACK)
eval text(dx * 2, dy * 2, "C", BLACK)
eval text(dx * 3, dy * 0, "D", BLACK)
eval text(dx * 4, dy * 2, "E", BLACK)
eval text(dx * 5, dy * 1, "F", BLACK)
eval text(dx * 6, dy * 2, "G", BLACK)

do for [x=(dx*3):(dx*7):(dx*8)] {
    eval node(x, (dy*0), BLACK)
    eval link(x, (dy*0), x-(2*dx), dy*1, 1, 1, COLOR_1)
    eval link(x, (dy*0), x+(2*dx), dy*1, 1, 1, COLOR_2)
}
do for [x=(dx*1):(dx*7):(dx*4)] {
    eval node(x, (dy*1), BLACK)
    eval link(x, (dy*1), x-(1*dx), dy*2, 1, 1, COLOR_1)
    eval link(x, (dy*1), x+(1*dx), dy*2, 1, 1, COLOR_2)
}
do for [x=0:(dx*7):(dx*2)] {
    eval node(x, (dy*2), BLACK)
    eval link(x, (dy*2), x-(0.5 * dx), dy*3, 1, 1, COLOR_1)
    eval link(x, (dy*2), x+(0.5 * dx), dy*3, 1, 1, COLOR_2)
}
do for [x=0:(dx*7):dx] {
    eval leaf(x-(0.5 * dx), dy*3, BLACK)
}

plot NaN notitle