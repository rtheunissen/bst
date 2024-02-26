#
# BALANCE
#
dx = real(2)
dy = real(3)

WIDTH  = 36
HEIGHT = 17

load "docs/plots/functions.gnuplot"
load "docs/plots/figures/graph.gnuplot"
load "docs/plots/components.gnuplot"
load "docs/plots/ruler.gnuplot"
load "docs/plots/colors.gnuplot"

X_OFFSET = 2
Y_OFFSET = 1

eval node(6*dx, dy*1, BLACK)
eval node(5*dx, 0,    BLACK)
eval node(4*dx, dy*1, BLACK)
eval node(3*dx, dy*2, BLACK)
eval node(2*dx, dy*3, BLACK)
eval node(1*dx, dy*4, BLACK)
eval node(0*dx, dy*5, BLACK)

eval text(6*dx, dy*1, "G", BLACK)
eval text(5*dx, 0,    "F", BLACK)
eval text(4*dx, dy*1, "E", BLACK)
eval text(3*dx, dy*2, "D", BLACK)
eval text(2*dx, dy*3, "C", BLACK)
eval text(1*dx, dy*4, "B", BLACK)
eval text(0*dx, dy*5, "A", BLACK)

eval link(5*dx, 0,    6*dx, dy*1, 0, 1, BLACK)
eval link(5*dx, 0,    4*dx, dy*1, 0, 1, BLACK)
eval link(4*dx, dy*1, 3*dx, dy*2, 0, 1, BLACK)
eval link(3*dx, dy*2, 2*dx, dy*3, 0, 1, BLACK)
eval link(2*dx, dy*3, 1*dx, dy*4, 0, 1, BLACK)
eval link(1*dx, dy*4, 0*dx, dy*5, 0, 1, BLACK)



X_OFFSET = 22
Y_OFFSET = 1

eval text(dx*0, dy*2, "A", BLACK)
eval text(dx*1, dy*1, "B", BLACK)
eval text(dx*2, dy*2, "C", BLACK)
eval text(dx*3, dy*0, "D", BLACK)
eval text(dx*4, dy*2, "E", BLACK)
eval text(dx*5, dy*1, "F", BLACK)
eval text(dx*6, dy*2, "G", BLACK)

eval node(dx*1, dy*1, BLACK)

eval node(dx*3, dy*0, BLACK)
eval node(dx*5, dy*1, BLACK)

eval node(dx*0, dy*2, BLACK)
eval node(dx*2, dy*2, BLACK)
eval node(dx*4, dy*2, BLACK)
eval node(dx*6, dy*2, BLACK)

eval link(dx*1, dy*1, dx*0, dy*2, 0, 1, BLACK)
eval link(dx*1, dy*1, dx*2, dy*2, 0, 1, BLACK)
eval link(dx*3, dy*0, dx*1, dy*1, 0, 1, BLACK)
eval link(dx*3, dy*0, dx*5, dy*1, 0, 1, BLACK)
eval link(dx*5, dy*1, dx*4, dy*2, 0, 1, BLACK)
eval link(dx*5, dy*1, dx*6, dy*2, 0, 1, BLACK)

plot NaN notitle