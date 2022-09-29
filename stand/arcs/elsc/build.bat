attrib -r elscfw.c
type MAIN.C > elscfw.c 
type MONITOR.C >> elscfw.c
type IRQ.C >> elscfw.c
type SEQUENCE.C >> elscfw.c
type I2C.C >> elscfw.c
type UTIL.C >> elscfw.c
type async.c >> elscfw.c
type cmd.c >> elscfw.c
type \mpc\mpc16.lib >> elscfw.c
attrib +r elscfw.c

c:\mpc\mpc elscfw.c +e +l +di
type elscfw.err

