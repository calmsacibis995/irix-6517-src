DIAGDEPTH= ../../..
SUBDIR = vdma
include ${DIAGDEPTH}/Makedefs

LLCINCS= -I${ROOT}/usr/include/LIGHT/gl

CFILES= btdac.c cid.c minigl.c rex.c rexpatterns.c sram.c vc1.c vram.c

COMMONPREF = LIGHT
_lib ${COMMONTARGS} _plib: ${COMMONPREF}$$@
	@for dirs in $(SUBDIR); do \
		echo "========\tcd $$dirs; $(MAKE) $@"; \
	(cd $$dirs; exec $(MAKE) $@) \
	done


# override common POBJECTS so only necessary files compile for PROM
PCFILES= btdac.c rex.c vc1.c
POBJECTS= ${PCFILES:.c=.o}

include ${DIAGDEPTH}/Makerules
