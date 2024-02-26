X_OFFSET = 0
Y_OFFSET = 0

LINE_WIDTH = 0.1

SPOT_SIZE = sqrt(2)/4

ARROW_HEAD_SIZE = sqrt(2)/4

# set grid lw LINE_WIDTH lt 1 lc rgb "#EEEEEE"

tree(x, y, color) = sprintf("\
    set object polygon from %f,%f to %f,%f to %f,%f to %f,%f \
    linewidth LINE_WIDTH \
    fillcolor rgb '%s'", \
        X_OFFSET + x - 3*sqrt(2)/7., HEIGHT - Y_OFFSET - (sqrt(2)/4) - y, \
        X_OFFSET + x,                HEIGHT - Y_OFFSET + (sqrt(2)/2) - y, \
        X_OFFSET + x + 3*sqrt(2)/7., HEIGHT - Y_OFFSET - (sqrt(2)/4) - y, \
        X_OFFSET + x - 3*sqrt(2)/7., HEIGHT - Y_OFFSET - (sqrt(2)/4) - y, \
        color)

leaf(x1, y1, color)           = sprintf("set object rectangle      at %f,%f size 1,1                        fs empty border lw LINE_WIDTH fc rgb '%s'",       X_OFFSET + x1, HEIGHT - Y_OFFSET - y1, color)
cube(x1, y1, color)           = sprintf("set object rectangle      at %f,%f size 1,1                        fs solid border lw LINE_WIDTH fc rgb '%s'",       X_OFFSET + x1, HEIGHT - Y_OFFSET - y1, color)
spot(x1, y1, color)           = sprintf("set object circle front   at %f,%f size SPOT_SIZE,SPOT_SIZE        fs solid        lw LINE_WIDTH fc rgb '%s'",       X_OFFSET + x1, HEIGHT - Y_OFFSET - y1, color)
node(x1, y1, color)           = sprintf("set object circle front   at %f,%f size (sqrt(2)/2.),(sqrt(2)/2.)                  lw LINE_WIDTH fc rgb '%s'",       X_OFFSET + x1, HEIGHT - Y_OFFSET - y1, color)
text(x1, y1, text, color)     = sprintf("set label '%s'            at %f,%f center norotate                                               tc rgb '%s'", text, X_OFFSET + x1, HEIGHT - Y_OFFSET - y1, color)
rect(x1, y1, x2, y2, color)   = sprintf("set object rectangle      from %f,%f to %f,%f                      fs empty border lw LINE_WIDTH fc rgb '%s'",       X_OFFSET + x1, HEIGHT - Y_OFFSET - y1, X_OFFSET + x2, HEIGHT - Y_OFFSET - y2, color)

div(a, b) = (real(b) == 0 ? real(0) : real(a) / real(b))

line(x1, y1, x2, y2, arrows, dashtype, color) = sprintf("\
    set arrow back from %f,%f to %f,%f \
    size ARROW_HEAD_SIZE,30,90 \
    filled \
    %s \
    dashtype %d \
    linewidth  0.1 \
    linecolor rgb '%s'", \
        X_OFFSET + x1, HEIGHT - Y_OFFSET - y1, \
        X_OFFSET + x2, HEIGHT - Y_OFFSET - y2, \
        (arrows == 0 ? "nohead" : (arrows == 1 ? "head" : (arrows == 2 ? "heads" : ""))), \
        dashtype, \
        color)

link(x1, y1, x2, y2, arrows, dashtype, color) = \
    line( \
        x1 - sgn(x1-x2) * (sqrt(2)/2 + sqrt(2)/4) * cos(atan(div(abs(real(y1-y2)), abs(real(x1-x2))))), \
        y1 - sgn(y1-y2) * (sqrt(2)/2 + sqrt(2)/4) * sin(atan(div(abs(real(y1-y2)), abs(real(x1-x2))))), \
        x2 + sgn(x1-x2) * (sqrt(2)/2 + sqrt(2)/4) * cos(atan(div(abs(real(y1-y2)), abs(real(x1-x2))))), \
        y2 + sgn(y1-y2) * (sqrt(2)/2 + sqrt(2)/4) * sin(atan(div(abs(real(y1-y2)), abs(real(x1-x2))))), \
        arrows, \
        dashtype, \
        color)