logb(x, b) = log(x)/log(b)

log2(x) = logb(x, 2)

path_log2_size(size) = size > 1 ? log2(size) : NaN

mkdir(file) = sprintf("mkdir -p \"$(dirname \"%s\")\"", file)

filter(col, iff, val) = stringcolumn(col) eq (iff) ? (val) : (NaN)
