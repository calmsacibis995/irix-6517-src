attrib -r xbox.c
type MAIN.C > xbox.c 
type MONITOR.C >> xbox.c
type IRQ.C >> xbox.c
type SEQUENCE.C >> xbox.c
type UTIL.C >> xbox.c
type \mpc\mpc16.lib >> xbox.c
attrib +r xbox.c

c:\mpc\mpc xbox.c +e +l +di 
type xbox.err

