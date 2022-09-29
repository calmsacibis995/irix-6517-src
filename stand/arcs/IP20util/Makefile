#$Revision: 1.3 $
DEPTH= ..
LLIBDEPTH=
include ${DEPTH}/commondefs
include $(PRODUCTDEFS)
include $(CPUARCHDEFS)
include $(ENDIANDEFS)
include $(MIDEFS)

#LLDFLAGS=-e startsc -N -T a8080000 -G 0
LLDFLAGS=${CPUARCH_MI_LDOPTS} -e startsc -x -r -d

LDV= ${LD} ${LLDFLAGS} ${VLDFLAGS} ${ENDIAN}

CFILES= fuse.c unfuse.c fcommon.c
TARGETS= fuse unfuse

default: ${TARGETS}

include ${COMMONRULES}

${TARGETS}: $$@.o fcommon.o ${DLIBSC} ${DLIBSK}
	${LDV} $@.o fcommon.o ${LIBSK} ${LIBSC} -o $@
