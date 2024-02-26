#
# LARGE BINARY SEARCH TREE
#
NUMBER_OF_NODES  = (1 << 7) - 1
NUMBER_OF_LEVELS = log(NUMBER_OF_NODES+1)/log(2)

dx = real(1)
dy = real(1)

WIDTH  = dx * (NUMBER_OF_NODES  + 1) / 2 + 1
HEIGHT = dy * (NUMBER_OF_LEVELS - 1) + 2

load "docs/plots/figures/graph.gnuplot"
load "docs/plots/components.gnuplot"
load "docs/plots/ruler.gnuplot"
load "docs/plots/colors.gnuplot"


SPOT_SIZE = 0.2


Y_OFFSET = 1
X_OFFSET = 0

COLOR_1 = BLACK
COLOR_2 = BLUE
COLOR_3 = CYAN
COLOR_4 = PINK
COLOR_5 = ORANGE

do for [level=1:(NUMBER_OF_LEVELS-1)] {
    do for [x=1:WIDTH:dx * (1 << level)] {
        X = x + (1 << (level-1)) * dx + dx/2 - dx
        Y = dy * (NUMBER_OF_LEVELS - level - 1)
        eval spot(X, Y, COLOR_1)
        eval line(X, Y, X - dx/2 * (1 << (level-1)), Y + dy, 0, 1, BLACK)
        eval line(X, Y, X + dx/2 * (1 << (level-1)), Y + dy, 0, 1, BLACK)
    }
}

do for [x=1:WIDTH-1:dx] {
    X = x
    Y = dy * NUMBER_OF_LEVELS - 1
    eval spot(X, Y, COLOR_1)
}

plot NaN notitle