#!smake
#ident $Revision: 1.13 $
include ${ROOT}/usr/include/make/commondefs

CFILES= fsr.c lib.c free_lib.c
TARGETS= fsr_efs

LCINCS= -I../nlib
LCOPTS= -fullwarn
LLDOPTS= -L../nlib
LLDLIBS= -lefs -ldisk

all: ${TARGETS}

include ${COMMONRULES}

fsr_efs:	${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@

install: all
	${INSTALL} -F /usr/etc -idb "std.sw.unix" ${TARGETS}

