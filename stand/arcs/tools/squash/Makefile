#
# Makefile for squash
#
include ${ROOT}/usr/include/make/commondefs
MKDEPFILE= Makedepend

OPTIMIZER = -g

LCOPTS= -p0 $(OPTIMIZER)
LLDLIBS= -lelf

HFILES= squash.h
CFILES= squash.c ../convert/elf.c

TARGETS= squash

default: ${TARGETS}

include ${COMMONRULES}

squash: ${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o ${TARGETS}

install: default
	${INSTALL} -F /usr/local/bin/squash -idb "noship.prom" ${TARGETS}
