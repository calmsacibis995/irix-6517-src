# $Revision: 1.4 $
#
include ../commondefs

CFILES= dma.c
TARGETS= dma

include ../commonrules

dma: ${OBJECTS} ${LIBEFS}
	${CCF} ${OBJECTS} ${LIBEFS} ${LDFLAGS} -o $@

install: ${TARGETS}
