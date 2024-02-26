#
# PERSISTENCE
#
WIDTH  = 53
HEIGHT = 48

X_SPACING = 30
Y_SPACING = 17

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

X_OFFSET = 0
Y_OFFSET = 1 + Y_SPACING * 0

eval spot(dx*4, dy*0, COLOR_1)
eval line(dx*4, dy*0, dx*2, dy*1, 0, 1, BLACK)
eval link(dx*4, dy*0, dx*6, dy*1, 1, 1, COLOR_1)


eval spot(dx*2, dy*1, BLACK)
eval line(dx*2, dy*1, dx*1, dy*2, 0, 1, BLACK)
eval line(dx*2, dy*1, dx*3, dy*2, 0, 1, BLACK)

eval spot(dx*6, dy*1, COLOR_1)
eval link(dx*6, dy*1, dx*5, dy*2, 1, 1, COLOR_1)
eval line(dx*6, dy*1, dx*7, dy*2, 0, 1, BLACK)


eval spot(dx*1, dy*2, BLACK)
eval line(dx*1, dy*2, dx*1 - 0.5*dx, dy*3, 0, 1, BLACK)
eval line(dx*1, dy*2, dx*1 + 0.5*dx, dy*3, 0, 1, BLACK)

eval spot(dx*3, dy*2, BLACK)
eval line(dx*3, dy*2, dx*3 - 0.5*dx, dy*3, 0, 1, BLACK)
eval line(dx*3, dy*2, dx*3 + 0.5*dx, dy*3, 0, 1, BLACK)

eval spot(dx*5, dy*2, COLOR_1)
eval link(dx*5, dy*2, dx*5 - 0.5*dx, dy*3, 1, 1, COLOR_1)
eval line(dx*5, dy*2, dx*5 + 0.5*dx, dy*3, 0, 1, BLACK)

eval spot(dx*7, dy*2, BLACK)
eval line(dx*7, dy*2, dx*7 - 0.5*dx, dy*3, 0, 1, BLACK)
eval line(dx*7, dy*2, dx*7 + 0.5*dx, dy*3, 0, 1, BLACK)


eval spot(dx*0  + 0.5*dx, dy*3, BLACK)
eval spot(dx*1  + 0.5*dx, dy*3, BLACK)
eval spot(dx*2  + 0.5*dx, dy*3, BLACK)
eval spot(dx*3  + 0.5*dx, dy*3, BLACK)
eval spot(dx*4  + 0.5*dx, dy*3, COLOR_1)
eval spot(dx*5  + 0.5*dx, dy*3, BLACK)
eval spot(dx*6  + 0.5*dx, dy*3, BLACK)
eval spot(dx*7  + 0.5*dx, dy*3, BLACK)

eval link(dx*4  + 0.5*dx, dy*3, dx*5, dy*4, 1, 1, COLOR_1)
eval leaf(dx*5, dy*4, COLOR_1)






















X_OFFSET = 0 + X_SPACING
Y_OFFSET = 1 + Y_SPACING * 0

eval spot(dx*4, dy*0, COLOR_1)
eval line(dx*4, dy*0, dx*2, dy*1, 0, 1, BLACK)
eval line(dx*4, dy*0, dx*6, dy*1, 0, 4, COLOR_1)


eval spot(dx*2, dy*1, BLACK)
eval line(dx*2, dy*1, dx*1, dy*2, 0, 1, BLACK)
eval line(dx*2, dy*1, dx*3, dy*2, 0, 1, BLACK)

eval spot(dx*6, dy*1, COLOR_1)
eval line(dx*6, dy*1, dx*5, dy*2, 0, 4, COLOR_1)
eval line(dx*6, dy*1, dx*7, dy*2, 0, 1, BLACK)


eval spot(dx*1, dy*2, BLACK)
eval line(dx*1, dy*2, dx*1 - 0.5*dx, dy*3, 0, 1, BLACK)
eval line(dx*1, dy*2, dx*1 + 0.5*dx, dy*3, 0, 1, BLACK)

eval spot(dx*3, dy*2, BLACK)
eval line(dx*3, dy*2, dx*3 - 0.5*dx, dy*3, 0, 1, BLACK)
eval line(dx*3, dy*2, dx*3 + 0.5*dx, dy*3, 0, 1, BLACK)

eval spot(dx*5, dy*2, COLOR_1)
eval line(dx*5, dy*2, dx*5 - 0.5*dx, dy*3, 0, 4, COLOR_1)
eval line(dx*5, dy*2, dx*5 + 0.5*dx, dy*3, 0, 1, BLACK)

eval spot(dx*7, dy*2, BLACK)
eval line(dx*7, dy*2, dx*7 - 0.5*dx, dy*3, 0, 1, BLACK)
eval line(dx*7, dy*2, dx*7 + 0.5*dx, dy*3, 0, 1, BLACK)


eval spot(dx*0  + 0.5*dx, dy*3, BLACK)
eval spot(dx*1  + 0.5*dx, dy*3, BLACK)
eval spot(dx*2  + 0.5*dx, dy*3, BLACK)
eval spot(dx*3  + 0.5*dx, dy*3, BLACK)
eval spot(dx*4  + 0.5*dx, dy*3, COLOR_1)
eval spot(dx*5  + 0.5*dx, dy*3, BLACK)
eval spot(dx*6  + 0.5*dx, dy*3, BLACK)
eval spot(dx*7  + 0.5*dx, dy*3, BLACK)

eval line(dx*4  + 0.5*dx, dy*3, dx*5, dy*4, 0, 4, COLOR_1)
eval spot(dx*5, dy*4, COLOR_1)
















X_OFFSET = 0
Y_OFFSET = 1 + Y_SPACING * 1

eval spot(dx*4, dy*0, COLOR_2)
eval link(dx*4, dy*0, dx*2, dy*1, 1, 1, COLOR_2)
eval line(dx*4, dy*0, dx*6, dy*1, 0, 4, COLOR_1)


eval spot(dx*2, dy*1, COLOR_2)
eval line(dx*2, dy*1, dx*1, dy*2, 0, 1, BLACK)
eval link(dx*2, dy*1, dx*3, dy*2, 1, 1, COLOR_2)

eval spot(dx*6, dy*1, COLOR_1)
eval line(dx*6, dy*1, dx*5, dy*2, 0, 4, COLOR_1)
eval line(dx*6, dy*1, dx*7, dy*2, 0, 1, BLACK)


eval spot(dx*1, dy*2, BLACK)
eval line(dx*1, dy*2, dx*1 - 0.5*dx, dy*3, 0, 1, BLACK)
eval line(dx*1, dy*2, dx*1 + 0.5*dx, dy*3, 0, 1, BLACK)

eval leaf(dx*3, dy*2, COLOR_2)
eval link(dx*3, dy*2, dx*3 + 0.5*dx, dy*3, 1, 1, COLOR_2)

eval spot(dx*5, dy*2, COLOR_1)
eval line(dx*5, dy*2, dx*5 - 0.5*dx, dy*3, 0, 4, COLOR_1)
eval line(dx*5, dy*2, dx*5 + 0.5*dx, dy*3, 0, 1, BLACK)

eval spot(dx*7, dy*2, BLACK)
eval line(dx*7, dy*2, dx*7 - 0.5*dx, dy*3, 0, 1, BLACK)
eval line(dx*7, dy*2, dx*7 + 0.5*dx, dy*3, 0, 1, BLACK)


eval spot(dx*0  + 0.5*dx, dy*3, BLACK)
eval spot(dx*1  + 0.5*dx, dy*3, BLACK)
eval spot(dx*2  + 0.5*dx, dy*3, BLACK)
eval spot(dx*3  + 0.5*dx, dy*3, COLOR_2)
eval spot(dx*4  + 0.5*dx, dy*3, COLOR_1)
eval spot(dx*5  + 0.5*dx, dy*3, BLACK)
eval spot(dx*6  + 0.5*dx, dy*3, BLACK)
eval spot(dx*7  + 0.5*dx, dy*3, BLACK)

eval line(dx*4  + 0.5*dx, dy*3, dx*5, dy*4, 0, 4, COLOR_1)
eval spot(dx*5, dy*4, COLOR_1)


















X_OFFSET = 0 + X_SPACING
Y_OFFSET = 1 + Y_SPACING * 1

eval spot(dx*4, dy*0, COLOR_2)
eval line(dx*4, dy*0, dx*2, dy*1, 0, 3, COLOR_2)
eval line(dx*4, dy*0, dx*6, dy*1, 0, 4, COLOR_1)


eval spot(dx*2, dy*1, COLOR_2)
eval line(dx*2, dy*1, dx*1, dy*2, 0, 1, BLACK)
eval line(dx*2, dy*1, dx*3, dy*2, 0, 3, COLOR_2)

eval spot(dx*6, dy*1, COLOR_1)
eval line(dx*6, dy*1, dx*5, dy*2, 0, 4, COLOR_1)
eval line(dx*6, dy*1, dx*7, dy*2, 0, 1, BLACK)


eval spot(dx*1, dy*2, BLACK)
eval line(dx*1, dy*2, dx*1 - 0.5*dx, dy*3, 0, 1, BLACK)
eval line(dx*1, dy*2, dx*1 + 0.5*dx, dy*3, 0, 1, BLACK)

eval spot(dx*3, dy*2, COLOR_2)
eval line(dx*3, dy*2, dx*2 + 0.5*dx, dy*3, 0, 4, BLACK)

eval spot(dx*5, dy*2, COLOR_1)
eval line(dx*5, dy*2, dx*5 - 0.5*dx, dy*3, 0, 4, COLOR_1)
eval line(dx*5, dy*2, dx*5 + 0.5*dx, dy*3, 0, 1, BLACK)

eval spot(dx*7, dy*2, BLACK)
eval line(dx*7, dy*2, dx*7 - 0.5*dx, dy*3, 0, 1, BLACK)
eval line(dx*7, dy*2, dx*7 + 0.5*dx, dy*3, 0, 1, BLACK)


eval spot(dx*0  + 0.5*dx, dy*3, BLACK)
eval spot(dx*1  + 0.5*dx, dy*3, BLACK)
eval spot(dx*2  + 0.5*dx, dy*3, BLACK)
eval spot(dx*4  + 0.5*dx, dy*3, COLOR_1)
eval spot(dx*5  + 0.5*dx, dy*3, BLACK)
eval spot(dx*6  + 0.5*dx, dy*3, BLACK)
eval spot(dx*7  + 0.5*dx, dy*3, BLACK)

eval line(dx*4  + 0.5*dx, dy*3, dx*5, dy*4, 0, 4, COLOR_1)
eval spot(dx*5, dy*4, COLOR_1)











X_OFFSET = 0
Y_OFFSET = 1 + Y_SPACING * 2

eval spot(dx*4, dy*0, COLOR_3)
eval link(dx*4, dy*0, dx*2, dy*1, 1, 1, COLOR_3)
eval line(dx*4, dy*0, dx*6, dy*1, 0, 4, COLOR_1)


eval spot(dx*2, dy*1, COLOR_3)
eval link(dx*2, dy*1, dx*1, dy*2, 1, 1, COLOR_3)
eval line(dx*2, dy*1, dx*3, dy*2, 0, 3, COLOR_2)

eval spot(dx*6, dy*1, COLOR_1)
eval line(dx*6, dy*1, dx*5, dy*2, 0, 4, COLOR_1)
eval line(dx*6, dy*1, dx*7, dy*2, 0, 1, BLACK)

eval spot(dx*1, dy*2, COLOR_3)
eval link(dx*1, dy*2, dx*1 - 0.5*dx, dy*3, 1, 1, COLOR_3)
eval line(dx*1, dy*2, dx*1 + 0.5*dx, dy*3, 0, 1, BLACK)

eval spot(dx*3, dy*2, COLOR_2)
eval line(dx*3, dy*2, dx*3 - 0.5*dx, dy*3, 0, 1, BLACK)

eval spot(dx*5, dy*2, COLOR_1)
eval line(dx*5, dy*2, dx*5 - 0.5*dx, dy*3, 0, 4, COLOR_1)
eval line(dx*5, dy*2, dx*5 + 0.5*dx, dy*3, 0, 1, BLACK)

eval spot(dx*7, dy*2, BLACK)
eval line(dx*7, dy*2, dx*7 - 0.5*dx, dy*3, 0, 1, BLACK)
eval line(dx*7, dy*2, dx*7 + 0.5*dx, dy*3, 0, 1, BLACK)


eval spot(dx*0  + 0.5*dx, dy*3, COLOR_3)
eval spot(dx*1  + 0.5*dx, dy*3, BLACK)
eval spot(dx*2  + 0.5*dx, dy*3, BLACK)
eval spot(dx*4  + 0.5*dx, dy*3, COLOR_1)
eval spot(dx*5  + 0.5*dx, dy*3, BLACK)
eval spot(dx*6  + 0.5*dx, dy*3, BLACK)
eval spot(dx*7  + 0.5*dx, dy*3, BLACK)

eval line(dx*4  + 0.5*dx, dy*3, dx*5, dy*4, 0, 4, COLOR_1)
eval spot(dx*5, dy*4, COLOR_1)

eval link(dx*0.5, dy*3, dx*0.25, dy*4, 1, 1, COLOR_3)
eval leaf(dx*0.25, dy*4, COLOR_3)








X_OFFSET = 0 + X_SPACING
Y_OFFSET = 1 + Y_SPACING * 2

eval spot(dx*2, dy*1, COLOR_3)
eval line(dx*2, dy*1, dx*1, dy*2, 0, 5, COLOR_3)
eval line(dx*2, dy*1, dx*3, dy*2, 0, 3, COLOR_2)

eval spot(dx*6, dy*1, COLOR_1)
eval line(dx*6, dy*1, dx*5, dy*2, 0, 4, COLOR_1)
eval line(dx*6, dy*1, dx*7, dy*2, 0, 1, BLACK)

eval spot(dx*1, dy*2, COLOR_3)
eval line(dx*1, dy*2, dx*1 - 0.5*dx, dy*3, 0, 5, COLOR_3)
eval line(dx*1, dy*2, dx*1 + 0.5*dx, dy*3, 0, 1, BLACK)

eval spot(dx*3, dy*2, COLOR_2)
eval line(dx*3, dy*2, dx*3 - 0.5*dx, dy*3, 0, 1, BLACK)

eval spot(dx*5, dy*2, COLOR_1)
eval line(dx*5, dy*2, dx*5 - 0.5*dx, dy*3, 0, 4, COLOR_1)
eval line(dx*5, dy*2, dx*5 + 0.5*dx, dy*3, 0, 1, BLACK)

eval spot(dx*7, dy*2, BLACK)
eval line(dx*7, dy*2, dx*7 - 0.5*dx, dy*3, 0, 1, BLACK)
eval line(dx*7, dy*2, dx*7 + 0.5*dx, dy*3, 0, 1, BLACK)

eval spot(dx*0  + 0.5*dx, dy*3, COLOR_3)
eval spot(dx*1  + 0.5*dx, dy*3, BLACK)
eval spot(dx*2  + 0.5*dx, dy*3, BLACK)
eval spot(dx*4  + 0.5*dx, dy*3, COLOR_1)
eval spot(dx*5  + 0.5*dx, dy*3, BLACK)
eval spot(dx*6  + 0.5*dx, dy*3, BLACK)
eval spot(dx*7  + 0.5*dx, dy*3, BLACK)

eval line(dx*4  + 0.5*dx, dy*3, dx*5, dy*4, 0, 4, COLOR_1)
eval spot(dx*5, dy*4, COLOR_1)

eval line(dx*0.5, dy*3, dx*0.25, dy*4, 0, 5, COLOR_3)
eval spot(dx*0.25, dy*4, COLOR_3)

eval spot(dx*4, dy*0, COLOR_3)
eval line(dx*4, dy*0, dx*2, dy*1, 0, 5, COLOR_3)
eval line(dx*4, dy*0, dx*6, dy*1, 0, 4, COLOR_1)













plot NaN notitle