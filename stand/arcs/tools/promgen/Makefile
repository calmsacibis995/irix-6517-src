# Makefile for promgen
#

include ${ROOT}/usr/include/make/commondefs

MKDEPFILE= Makedepend
MKDEPOPT   = -MDupdate $(MKDEPFILE)

OPTIMIZER = -g

CVERSION=-xansi $(FULLWARN)
FULLWARN=-fullwarn

LWOFF=,1185,1209${LLWOFF}

LCDEFS= $(CPUARCH_DEFS) ${LLCDEFS} $(PRODDEFS)
LCOPTS= $(OPTIMIZER) $(CVERSION)

LDIRT = unzip.c unzip.h prom.out prom.s3 ${MKDEPFILE} ${TARGETS}

LDFLAGS= -lelf
LCOPTS=

CFILES= promgen.c dozip.c unzip.c s3.c

TARGETS= promgen

default: ${TARGETS}

include ${COMMONRULES}

./unzip.h:
	ln -sf ../../IP27prom/unzip.h .

unzip.c:
	ln -sf ../../IP27prom/unzip.c .

promgen.o: ./unzip.h

promgen: dozip.c ${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o promgen ${LDOPTS}
