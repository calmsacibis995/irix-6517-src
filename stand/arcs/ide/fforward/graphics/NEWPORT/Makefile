DIAGDEPTH= ../../..
include ${DIAGDEPTH}/Makedefs

LINCLUDES = -I../ -I../.. -I${ROOT}/usr/include/NEWPORT/

CFILES= minigl3.c rex3patterns.c cid3.c vram3.c ng1visuals.c rex3.c newport_init.c bt445.c xmap9.c vc2.c sram3.c

COMMONPREF = NEWPORT

_lib ${COMMONTARGS}: ${COMMONPREF}$$@

_plib: ${COMMONPREF}$$@

# override common POBJECTS so only necessary files compile for PROM
PCFILES=  ng1_pon.c rex3.c
POBJECTS= ${PCFILES:.c=.o}

include ${DIAGDEPTH}/Makerules

