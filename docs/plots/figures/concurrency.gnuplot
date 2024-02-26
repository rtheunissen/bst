#
# CONCURRENCY
#
WIDTH  = 36
HEIGHT = 14

dx = real(3)
dy = real(3)

load "docs/plots/figures/graph.gnuplot"
load "docs/plots/components.gnuplot"
load "docs/plots/ruler.gnuplot"

load "docs/plots/colors.gnuplot"

COLOR_1 = ORANGE
COLOR_2 = BLUE    
COLOR_3 = CYAN
COLOR_4 = PINK

X_OFFSET = 6
Y_OFFSET = 1

eval spot(dx*4, dy*0, BLACK)
eval link(dx*4, dy*0, dx*2, dy*1, 1, 1, COLOR_2)
eval link(dx*4, dy*0, dx*6, dy*1, 1, 1, COLOR_1)

eval text(dx*2, dy * 0, "OPERATION 1", COLOR_2)
eval text(dx*6, dy * 0, "OPERATION 2", COLOR_1)

eval spot(dx*2, dy*1, BLACK)
eval link(dx*2, dy*1, dx*1, dy*2, 1, 1, COLOR_2)
eval line(dx*2, dy*1, dx*3, dy*2, 0, 1, BLACK)

eval spot(dx*6, dy*1, BLACK)
eval link(dx*6, dy*1, dx*5, dy*2, 1, 1, COLOR_1)
eval line(dx*6, dy*1, dx*7, dy*2, 0, 1, BLACK)


eval spot(dx*1, dy*2, BLACK)
eval link(dx*1, dy*2, dx*1 - 0.5*dx, dy*3, 1, 1, COLOR_2)
eval line(dx*1, dy*2, dx*1 + 0.5*dx, dy*3, 0, 1, BLACK)

eval spot(dx*3, dy*2, BLACK)
eval line(dx*3, dy*2, dx*3 - 0.5*dx, dy*3, 0, 1, BLACK)
eval line(dx*3, dy*2, dx*3 + 0.5*dx, dy*3, 0, 1, BLACK)

eval spot(dx*5, dy*2, BLACK)
eval line(dx*5, dy*2, dx*5 - 0.5*dx, dy*3, 0, 1, BLACK)
eval link(dx*5, dy*2, dx*5 + 0.5*dx, dy*3, 1, 1, COLOR_1)

eval spot(dx*7, dy*2, BLACK)
eval line(dx*7, dy*2, dx*7 - 0.5*dx, dy*3, 0, 1, BLACK)
eval line(dx*7, dy*2, dx*7 + 0.5*dx, dy*3, 0, 1, BLACK)

eval spot(dx*0 + 0.5*dx, dy*3, COLOR_2)
eval spot(dx*1 + 0.5*dx, dy*3, BLACK)
eval spot(dx*2 + 0.5*dx, dy*3, BLACK)
eval spot(dx*3 + 0.5*dx, dy*3, BLACK)
eval spot(dx*4 + 0.5*dx, dy*3, BLACK)
eval spot(dx*5 + 0.5*dx, dy*3, BLACK)
eval spot(dx*6 + 0.5*dx, dy*3, BLACK)
eval spot(dx*7 + 0.5*dx, dy*3, BLACK)

eval link(dx*5 + 0.5*dx, dy*3, dx*6, dy*4, 1, 1, COLOR_1)
eval leaf(dx*6, dy*4, COLOR_1)

plot NaN notitle