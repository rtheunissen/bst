#
# PERFECT TREES
#
WIDTH  = 36
HEIGHT = 8

dx = real(1)
dy = real(1)

load "docs/plots/figures/graph.gnuplot"
load "docs/plots/components.gnuplot"
load "docs/plots/ruler.gnuplot"
load "docs/plots/colors.gnuplot"

SPOT_SIZE = 0.2

COLOR_1 = BLUE
COLOR_2 = ORANGE
COLOR_3 = CYAN
COLOR_4 = PINK



















X_OFFSET = -0.5
Y_OFFSET = 3

eval text(1, -2, "1", BLACK)
eval spot(1 + 0*dx, dy*0, BLACK)





X_OFFSET = 2

eval text(1, -2, "3", BLACK)

do for [x=(dx*1):(dx*1):(dx*2)] {
    eval spot(x, dy*0, BLACK)
    eval line(x, dy*0, x - 0.5*dx, dy*1, 0, 1, BLACK)
    eval line(x, dy*0, x + 0.5*dx, dy*1, 0, 1, BLACK)
}
do for [x=(dx*0):(dx*1):(dx*1)] {
    eval spot(x + 0.5*dx, dy*1, BLACK)
}







X_OFFSET = 5

eval text(2, -2, "7", BLACK)

do for [x=(dx*2):(dx*3):(dx*4)] {
    eval spot(x, dy*0, BLACK)
    eval line(x, dy*0, x - 1*dx, dy*1, 0, 1, BLACK)
    eval line(x, dy*0, x + 1*dx, dy*1, 0, 1, BLACK)
}
do for [x=(dx*1):(dx*3):(dx*2)] {
    eval spot(x, dy*1, BLACK)
    eval line(x, dy*1, x - 0.5*dx, dy*2, 0, 1, BLACK)
    eval line(x, dy*1, x + 0.5*dx, dy*2, 0, 1, BLACK)
}
do for [x=(dx*0):(dx*3):(dx*1)] {
    eval spot(x+ 0.5*dx, dy*2, BLACK)
}




X_OFFSET = 10

eval text(4, -2, "15", BLACK)

do for [x=(dx*4):(dx*7):(dx*8)] {
    eval spot(x, dy*0, BLACK)
    eval line(x, dy*0, x - 2*dx, dy*1, 0, 1, BLACK)
    eval line(x, dy*0, x + 2*dx, dy*1, 0, 1, BLACK)
}
do for [x=(dx*2):(dx*7):(dx*4)] {
    eval spot(x, dy*1, BLACK)
    eval line(x, dy*1, x - 1*dx, dy*2, 0, 1, BLACK)
    eval line(x, dy*1, x + 1*dx, dy*2, 0, 1, BLACK)
}
do for [x=(dx*1):(dx*7):(dx*2)] {
    eval spot(x, dy*2, BLACK)
    eval line(x, dy*2, x - 0.5*dx, dy*3, 0, 1, BLACK)
    eval line(x, dy*2, x + 0.5*dx, dy*3, 0, 1, BLACK)
}
do for [x=(dx*0):(dx*7):(dx*1)] {
    eval spot(x + 0.5*dx, dy*3, BLACK)
}





X_OFFSET = 20

eval text(8, -2, "31", BLACK)

do for [x=(dx*8):(dx*15):(dx*16)] {
    eval spot(x, dy*0, BLACK)
    eval line(x, dy*0, x - 4*dx, dy*1, 0, 1, BLACK)
    eval line(x, dy*0, x + 4*dx, dy*1, 0, 1, BLACK)
}
do for [x=(dx*4):(dx*15):(dx*8)] {
    eval spot(x, dy*1, BLACK)
    eval line(x, dy*1, x - 2*dx, dy*2, 0, 1, BLACK)
    eval line(x, dy*1, x + 2*dx, dy*2, 0, 1, BLACK)
}
do for [x=(dx*2):(dx*15):(dx*4)] {
    eval spot(x, dy*2, BLACK)
    eval line(x, dy*2, x - 1*dx, dy*3, 0, 1, BLACK)
    eval line(x, dy*2, x + 1*dx, dy*3, 0, 1, BLACK)
}
do for [x=(dx*1):(dx*15):(dx*2)] {
    eval spot(x, dy*3, BLACK)
    eval line(x, dy*3, x - 0.5*dx, dy*4, 0, 1, BLACK)
    eval line(x, dy*3, x + 0.5*dx, dy*4, 0, 1, BLACK)
}
do for [x=(dx*0):(dx*15):(dx*1)] {
    eval spot(x+ 0.5*dx, dy*4, BLACK)
}




plot NaN notitle