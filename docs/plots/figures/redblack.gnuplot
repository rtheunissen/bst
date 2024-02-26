#
# RED BLACK
#
WIDTH  = 36
HEIGHT = 7


load "docs/plots/figures/graph.gnuplot"
load "docs/plots/components.gnuplot"
load "docs/plots/ruler.gnuplot"
load "docs/plots/colors.gnuplot"

dx = real(1)
dy = real(1)

SPOT_SIZE = 0.2

X_OFFSET = 1
Y_OFFSET = 1

eval spot(dx*3, dy*5, RED)
eval spot(dx*4, dy*5, RED)
eval spot(dx*8, dy*5, RED)
eval spot(dx*29, dy*5, RED)
eval spot(dx*30, dy*5, RED)
eval spot(dx*31, dy*5, RED)
eval spot(dx*32, dy*5, RED)



X_OFFSET = 1 + dx/2

eval spot(dx*1, dy*4, BLACK)
eval spot(dx*3, dy*4, BLACK)
eval spot(dx*5, dy*4, BLACK)
eval spot(dx*7, dy*4, BLACK)
eval spot(dx*9, dy*4, RED)
eval spot(dx*11, dy*4, RED)
eval spot(dx*23, dy*4, RED)
eval spot(dx*27, dy*4, RED)
eval spot(dx*29, dy*4, BLACK)
eval spot(dx*31, dy*4, BLACK)

eval line(dx*3, dy*4, dx*3 + dx*0.5, dy*5, 0, 1, BLACK)
eval line(dx*3, dy*4, dx*3 - dx*0.5, dy*5, 0, 1, BLACK)
eval line(dx*7, dy*4, dx*7 + dx*0.5, dy*5, 0, 1, BLACK)
eval line(dx*29, dy*4, dx*29 - dx*0.5, dy*5, 0, 1, BLACK)
eval line(dx*29, dy*4, dx*29 + dx*0.5, dy*5, 0, 1, BLACK)
eval line(dx*31, dy*4, dx*31 - dx*0.5, dy*5, 0, 1, BLACK)
eval line(dx*31, dy*4, dx*31 + dx*0.5, dy*5, 0, 1, BLACK)

X_OFFSET = 1 + dx + dx/2

eval spot(dx*1, dy*3, RED)
eval spot(dx*5, dy*3, RED)
eval spot(dx*9, dy*3, BLACK)
eval spot(dx*13, dy*3, BLACK)
eval spot(dx*17, dy*3, BLACK)
eval spot(dx*21, dy*3, BLACK)
eval spot(dx*25, dy*3, BLACK)
eval spot(dx*29, dy*3, RED)

eval line(dx*1, dy*3, dx*1 + dx, dy*4, 0, 1, BLACK)
eval line(dx*1, dy*3, dx*1 - dx, dy*4, 0, 1, BLACK)
eval line(dx*5, dy*3, dx*5 + dx, dy*4, 0, 1, BLACK)
eval line(dx*5, dy*3, dx*5 - dx, dy*4, 0, 1, BLACK)
eval line(dx*9, dy*3, dx*9 + dx, dy*4, 0, 1, BLACK)
eval line(dx*9, dy*3, dx*9 - dx, dy*4, 0, 1, BLACK)
eval line(dx*21, dy*3, dx*21 + dx, dy*4, 0, 1, BLACK)
eval line(dx*25, dy*3, dx*25 + dx, dy*4, 0, 1, BLACK)
eval line(dx*29, dy*3, dx*29 - dx, dy*4, 0, 1, BLACK)
eval line(dx*29, dy*3, dx*29 + dx, dy*4, 0, 1, BLACK)



X_OFFSET = 1 + 3*dx + dx/2

eval spot(dx*1, dy*2, BLACK)
eval spot(dx*9, dy*2, BLACK)
eval spot(dx*17, dy*2, BLACK)
eval spot(dx*25, dy*2, BLACK)

eval line(dx*1, dy*2, dx*1 + dx*2, dy*3, 0, 1, BLACK)
eval line(dx*1, dy*2, dx*1 - dx*2, dy*3, 0, 1, BLACK)
eval line(dx*9, dy*2, dx*9 + dx*2, dy*3, 0, 1, BLACK)
eval line(dx*9, dy*2, dx*9 - dx*2, dy*3, 0, 1, BLACK)
eval line(dx*17, dy*2, dx*17 + dx*2, dy*3, 0, 1, BLACK)
eval line(dx*17, dy*2, dx*17 - dx*2, dy*3, 0, 1, BLACK)
eval line(dx*25, dy*2, dx*25 - dx*2, dy*3, 0, 1, BLACK)
eval line(dx*25, dy*2, dx*25 + dx*2, dy*3, 0, 1, BLACK)


X_OFFSET = 1 + 7*dx + dx/2

eval spot(dx*1, dy*1, RED)
eval spot(dx*17, dy*1, RED)

eval line(dx*1, dy*1, dx*1 + dx*4, dy*2, 0, 1, BLACK)
eval line(dx*1, dy*1, dx*1 - dx*4, dy*2, 0, 1, BLACK)
eval line(dx*17, dy*1, dx*17 - dx*4, dy*2, 0, 1, BLACK)
eval line(dx*17, dy*1, dx*17 + dx*4, dy*2, 0, 1, BLACK)


X_OFFSET = 1 + 15*dx + dx/2

eval spot(dx*1, dy*0, BLACK)

eval line(dx*1, dy*0, dx*1 - dx*8, dy*1, 0, 1, BLACK)
eval line(dx*1, dy*0, dx*1 + dx*8, dy*1, 0, 1, BLACK)



plot NaN notitle