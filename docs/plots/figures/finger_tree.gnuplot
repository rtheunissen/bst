#
# FINGER TREE
#
WIDTH  = 36
HEIGHT = 18

load "docs/plots/figures/graph.gnuplot"
load "docs/plots/components.gnuplot"
load "docs/plots/colors.gnuplot"

COLOR_1 = BLUE
COLOR_2 = ORANGE
COLOR_3 = CYAN

X_OFFSET = 0
Y_OFFSET = 1

# ROOT
eval node(18,   2, COLOR_3)
eval text(18,   2, "D", BLACK)

# LEFT SPINE
eval text( 7,  13, "A", BLACK)
eval text(10,  10, "B", BLACK)
eval text(13,   7, "C", BLACK)

eval node(13,   7, COLOR_1)
eval node(10,  10, COLOR_1)
eval node( 7,  13, COLOR_1)

eval link(7,  13,  10, 10, 1, 1, BLACK)
eval link(10, 10,  13,  7, 1, 1, BLACK)

# RIGHT SPINE
eval text(23,  7,  "E", BLACK)
eval text(26, 10,  "F", BLACK)
eval text(29, 13,  "G", BLACK)

eval node(23,  7,  COLOR_2)
eval node(26, 10,  COLOR_2)
eval node(29, 13,  COLOR_2)

eval link(26, 10, 23,  7, 1, 1, BLACK)
eval link(29, 13, 26, 10, 1, 1, BLACK)

# LEFT SPINE INTERIOR
eval link(14-1, 8-1, 15+1,  9+1, 1, 1, COLOR_1)
eval link(11-1, 11-1, 12+1, 12+1, 1, 1, COLOR_1)
eval link(8-1, 14-1, 9+1,  15+1, 1, 1, COLOR_1)

eval tree(10, 16, COLOR_1)
eval tree(13, 13, COLOR_1)
eval tree(16, 10, COLOR_1)

# RIGHT SPINE INTERIOR
eval link(22+1,  8-1, 21-1,  9+1, 1, 1, COLOR_2)
eval link(25+1, 11-1, 24-1, 12+1, 1, 1, COLOR_2)
eval link(28+1, 14-1, 27-1, 15+1, 1, 1, COLOR_2)

eval tree(20, 10, COLOR_2)
eval tree(23, 13, COLOR_2)
eval tree(26, 16, COLOR_2)

# ROOT REFERENCES
eval link(13, 7, 18, 2, 0, 3, BLACK)
eval link(23, 7, 18, 2, 0, 3, BLACK)

eval link(4, 16,  7, 13, 1, 1, BLACK)
eval link(32, 16, 29, 13, 1, 1, BLACK)

# LABELS
eval text( 4, 16, "HEAD", BLACK)
eval text(32, 16, "TAIL", BLACK)
eval text(18,  0, "ROOT", BLACK)

plot NaN notitle