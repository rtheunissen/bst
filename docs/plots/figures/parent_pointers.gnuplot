#
# PARENT POINTERS
#
WIDTH  = 36
HEIGHT = 11

dx = real(3)
dy = real(3)

load "docs/plots/figures/graph.gnuplot"
load "docs/plots/components.gnuplot"
load "docs/plots/ruler.gnuplot"

load "docs/plots/colors.gnuplot"

COLOR_1 = BLUE
COLOR_2 = ORANGE
COLOR_3 = CYAN
COLOR_4 = PINK

X_OFFSET = 6
Y_OFFSET = 1

eval spot(dx*4, dy*0, BLACK)
eval link(dx*4, dy*0, dx*2, dy*1, 2, 1, BLACK)
eval link(dx*4, dy*0, dx*6, dy*1, 2, 1, BLACK)


eval spot(dx*2, dy*1, BLACK)
eval link(dx*2, dy*1, dx*1, dy*2, 2, 1, BLACK)
eval link(dx*2, dy*1, dx*3, dy*2, 2, 1, BLACK)

eval spot(dx*6, dy*1, BLACK)
eval link(dx*6, dy*1, dx*5, dy*2, 2, 1, BLACK)
eval link(dx*6, dy*1, dx*7, dy*2, 2, 1, BLACK)


eval spot(dx*1, dy*2, BLACK)
eval link(dx*1, dy*2, dx*1 - 0.5*dx, dy*3, 2, 1, BLACK)
eval link(dx*1, dy*2, dx*1 + 0.5*dx, dy*3, 2, 1, BLACK)

eval spot(dx*3, dy*2, BLACK)
eval link(dx*3, dy*2, dx*3 - 0.5*dx, dy*3, 2, 1, BLACK)
eval link(dx*3, dy*2, dx*3 + 0.5*dx, dy*3, 2, 1, BLACK)

eval spot(dx*5, dy*2, BLACK)
eval link(dx*5, dy*2, dx*5 - 0.5*dx, dy*3, 2, 1, BLACK)
eval link(dx*5, dy*2, dx*5 + 0.5*dx, dy*3, 2, 1, BLACK)

eval spot(dx*7, dy*2, BLACK)
eval link(dx*7, dy*2, dx*7 - 0.5*dx, dy*3, 2, 1, BLACK)
eval link(dx*7, dy*2, dx*7 + 0.5*dx, dy*3, 2, 1, BLACK)


eval spot(dx*0  + 0.5*dx, dy*3, BLACK)
eval spot(dx*1  + 0.5*dx, dy*3, BLACK)
eval spot(dx*2  + 0.5*dx, dy*3, BLACK)
eval spot(dx*3  + 0.5*dx, dy*3, BLACK)
eval spot(dx*4  + 0.5*dx, dy*3, BLACK)
eval spot(dx*5  + 0.5*dx, dy*3, BLACK)
eval spot(dx*6  + 0.5*dx, dy*3, BLACK)
eval spot(dx*7  + 0.5*dx, dy*3, BLACK)


plot NaN notitle














