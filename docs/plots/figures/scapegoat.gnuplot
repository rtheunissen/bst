#
# SCAPEGOAT
#
WIDTH  = 36
HEIGHT = 7

dx = real(1)
dy = real(1)

load "docs/plots/figures/graph.gnuplot"
load "docs/plots/components.gnuplot"
load "docs/plots/ruler.gnuplot"
load "docs/plots/colors.gnuplot"

SPOT_SIZE = 0.2

X_OFFSET = 2
Y_OFFSET = 1

do for [x=(dx*16):(dx*32):(dx*32)] {
    eval spot(x, dy*0, BLACK)
    eval line(x, dy*0, x - 8*dx, dy*1, 0, 1, BLACK)
}
do for [x=(dx*8):(dx*15):(dx*16)] {
    eval spot(x, dy*1, BLACK)
    eval line(x, dy*1, x - 4*dx, dy*2, 0, 1, BLACK)
    eval line(x, dy*1, x + 4*dx, dy*2, 0, 1, BLACK)
}
do for [x=(dx*4):(dx*15):(dx*8)] {
    eval spot(x, dy*2, BLACK)
    eval line(x, dy*2, x - 2*dx, dy*3, 0, 1, BLACK)
    eval line(x, dy*2, x + 2*dx, dy*3, 0, 1, BLACK)
}
do for [x=(dx*2):(dx*15):(dx*4)] {
    eval spot(x, dy*3, BLACK)
    eval line(x, dy*3, x - 1*dx, dy*4, 0, 1, BLACK)
    eval line(x, dy*3, x + 1*dx, dy*4, 0, 1, BLACK)
}
do for [x=(dx*1):(dx*15):(dx*2)] {
    eval spot(x, dy*4, BLACK)
    eval line(x, dy*4, x - 0.5*dx, dy*5, 0, 1, BLACK)
    eval line(x, dy*4, x + 0.5*dx, dy*5, 0, 1, BLACK)
}
do for [x=(dx*0):(dx*15):(dx*1)] {
    eval spot(x+ 0.5*dx, dy*5, BLACK)
}

plot NaN notitle