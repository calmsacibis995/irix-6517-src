#ident "$Revision: 1.6 $"
#
# Makefile for IP26 (teton) diagnostic PROMs.
#
DEPTH= ../..
TARGDEPTH= ../../..
include ${DEPTH}/commondefs
include $(PRODUCTDEFS)
include $(ENDIANDEFS)
include $(CPUARCHDEFS)

DEF_CSTYLE=

# get the cool mips 4 instructions
MIPSVERS=-mips4

# override global opts to use -rom flag
LLDOPTS=-rom -non_shared

TEXTADDR=900000001fc00000
BSSADDR=9000000008f00000

LLCDEFS=${CHIPDEFS} $(LLLCDEFS)
LDV= ${LD} ${LLDOPTS} ${CPUARCH_MI_LDOPTS} ${VLDFLAGS} ${ENDIAN} -e start

TARGETDIR=.
VPATH=${TARGETDIR}

#
# In the ASFILES list, csu.s must come first for proper load-order.
#
ASFILES= csu.s lmem_conf.s cache.s

TARGETS=prom1 prom2 prom3 prom4

# allow developers to have a different default target
# usually will be used to make dprom or lowprom the defaut
sinclude localdefault

default: ${TARGETS} 

COMMONPREF=IP26prom
include ${DEPTH}/commonrules

$(TARGETS): $$@.o ../elspec.template
	sed -e 's/TEXTADDR/0x${TEXTADDR}/' -e 's/BSSADDR/0x${BSSADDR}/' \
		../elspec.template > elspec.$@
	cd ${TARGETDIR}; ${LDV} -elspec elspec.$@ $@.o -o $@; \
	${SIZE} -Ax $@

clobber:
	rm -f ${TARGETS} *.o elspec.*

prom2.o: prom2.o prom1.s blink.s
prom1.o: prom1.s blink.s
prom4.o: prom4.s blink.s


