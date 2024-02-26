#
# MEDIAN BALANCE
#
WIDTH  = 36
HEIGHT = 12

dx = real(2)
dy = real(3)

load "docs/plots/figures/graph.gnuplot"
load "docs/plots/components.gnuplot"
load "docs/plots/ruler.gnuplot"
load "docs/plots/colors.gnuplot"

COLOR_1 = ORANGE
COLOR_2 = BLUE
COLOR_3 = CYAN
COLOR_4 = PINK


X_OFFSET = 10
Y_OFFSET = -10

eval leaf(dx*1-0.25-5.5, dy*7, BLACK)
eval text(dx*1-0.5, dy*7, "MEDIAN-BALANCED", BLACK)

eval cube(dx*2.5-0.25+7.5, dy*7, BLACK)
eval text(dx*2.5-0.5+13, dy*7, "MEDIAN-BALANCED", BLACK)


X_OFFSET = 8
Y_OFFSET = -8

eval node(dx*0, dy*4, BLACK)
eval node(dx*1, dy*3, BLACK)
eval node(dx*1, dy*5, BLACK)
eval node(dx*2, dy*4, BLACK)
eval node(dx*3, dy*5, BLACK)

eval text(dx*0, dy*4, "A", BLACK)
eval text(dx*1, dy*3, "B", BLACK)
eval text(dx*1, dy*5, "C", BLACK)
eval text(dx*2, dy*4, "D", BLACK)
eval text(dx*3, dy*5, "E", BLACK)

eval link(dx*0, dy*4, dx*1, dy*3, 0, 1, BLACK)
eval link(dx*1, dy*3, dx*2, dy*4, 0, 1, BLACK)
eval link(dx*2, dy*4, dx*3, dy*5, 0, 1, BLACK)
eval link(dx*2, dy*4, dx*1, dy*5, 0, 1, BLACK)

X_OFFSET = 23

eval node(dx*0, dy*5, BLACK)
eval node(dx*1, dy*4, BLACK)
eval node(dx*2, dy*3, BLACK)
eval node(dx*3, dy*4, BLACK)
eval node(dx*4, dy*5, BLACK)

eval text(dx*0,  dy*5, "A", BLACK)
eval text(dx*1,  dy*4, "B", BLACK)
eval text(dx*2,  dy*3, "C", BLACK)
eval text(dx*3,  dy*4, "D", BLACK)
eval text(dx*4,  dy*5, "E", BLACK)

eval link(dx*0, dy*5, dx*1, dy*4, 0, 1, BLACK)
eval link(dx*1, dy*4, dx*2, dy*3, 0, 1, BLACK)
eval link(dx*2, dy*3, dx*3, dy*4, 0, 1, BLACK)
eval link(dx*3, dy*4, dx*4, dy*5, 0, 1, BLACK)


plot NaN notitle














