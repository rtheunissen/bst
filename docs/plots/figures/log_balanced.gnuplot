#
# LOG BALANCED TREE OF SIZE 7 and 3
#
WIDTH  = 36
HEIGHT = 12

load "docs/plots/figures/graph.gnuplot"
load "docs/plots/components.gnuplot"
load "docs/plots/ruler.gnuplot"
load "docs/plots/colors.gnuplot"

COLOR_1 = BLUE
COLOR_2 = ORANGE
COLOR_3 = CYAN
COLOR_4 = PINK

dx = real(1)
dy = real(2)

eval text(9, 1, "BALANCED",     BLACK)
eval text(28, 1, "NOT BALANCED", BLACK)


X_OFFSET = 2
Y_OFFSET = 3

eval text(8, 8, "8", COLOR_1)
eval text(14, 6, "4", COLOR_2)

eval text(27, 8, "8", COLOR_1)
eval text(32, 4, "2", COLOR_2)

do for [x=3:7:8] {
    eval spot(x*dx, dy*1, BLACK)
    eval line(x*dx, dy*1, x*dx - 2*dx, dy*2, 0, 1, BLACK)
    eval line(x*dx, dy*1, x*dx + 2*dx, dy*2, 0, 1, BLACK)
}
do for [x=1:7:4] {
    eval spot(x*dx, dy*2, BLACK)
    eval line(x*dx, dy*2, x*dx - 1*dx, dy*3, 0, 1, BLACK)
    eval line(x*dx, dy*2, x*dx + 1*dx, dy*3, 0, 1, BLACK)
}
do for [x=0:7:2] {
    eval spot(x*dx, dy*3, BLACK)
    eval link(x*dx, dy*3, x*dx - dx/2, dy*4, 0, 1, BLACK)
    eval link(x*dx, dy*3, x*dx + dx/2, dy*4, 0, 1, BLACK)
}
do for [x=0:7] {
    eval leaf(x*dx-dx/2, dy*4, COLOR_1)
}



eval spot(7*dx, dy*0, BLACK)
eval line(7*dx, dy*0, 3*dx, dy*1, 0, 1, BLACK)
eval line(7*dx, dy*0, 11*dx, dy*1, 0, 1, BLACK)

do for [x=11:16:8] {
    eval spot(x*dx, dy*1, BLACK)
    eval line(x*dx, dy*1, x*dx - 1*dx, dy*2, 0, 1, BLACK)
    eval line(x*dx, dy*1, x*dx + 1*dx, dy*2, 0, 1, BLACK)
}
do for [x=10:12:2] {
    eval spot(x*dx, dy*2, BLACK)
    eval link(x*dx, dy*2, x*dx - 0.5*dx, dy*3, 0, 1, BLACK)
    eval link(x*dx, dy*2, x*dx + 0.5*dx, dy*3, 0, 1, BLACK)
}
do for [x=10:13] {
    eval leaf(x*dx-dx/2, dy*3, COLOR_2)
}











X_OFFSET = 21

do for [x=3:7:8] {
    eval spot(x*dx, dy*1, BLACK)
    eval line(x*dx, dy*1, x*dx - 2*dx, dy*2, 0, 1, BLACK)
    eval line(x*dx, dy*1, x*dx + 2*dx, dy*2, 0, 1, BLACK)
}
do for [x=1:7:4] {
    eval spot(x*dx, dy*2, BLACK)
    eval line(x*dx, dy*2, x*dx - 1*dx, dy*3, 0, 1, BLACK)
    eval line(x*dx, dy*2, x*dx + 1*dx, dy*3, 0, 1, BLACK)
}
do for [x=0:7:2] {
    eval spot(x*dx, dy*3, BLACK)
    eval link(x*dx, dy*3, x*dx - dx/2, dy*4, 0, 1, BLACK)
    eval link(x*dx, dy*3, x*dx + dx/2, dy*4, 0, 1, BLACK)
}
do for [x=0:7] {
    eval leaf(x*dx-dx/2, dy*4, COLOR_1)
}



eval spot(7*dx, dy*0, BLACK)
eval line(7*dx, dy*0, 3*dx, dy*1, 0, 1, BLACK)
eval line(7*dx, dy*0, 11*dx, dy*1, 0, 1, BLACK)



do for [x=11:11:8] {
    eval spot(x*dx, dy*1, BLACK)
    eval link(x*dx, dy*1, x*dx - 1*dx, dy*2, 0, 1, BLACK)
    eval link(x*dx, dy*1, x*dx + 1*dx, dy*2, 0, 1, BLACK)
}
do for [x=11:12] {
    eval leaf(x*dx-dx/2, dy*2, COLOR_2)
}



plot NaN notitle