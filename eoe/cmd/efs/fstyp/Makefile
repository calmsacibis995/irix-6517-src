#!smake
#ident $Revision: 1.9 $
include ../commondefs

CFILES= fstyp.c
TARGETS= fstyp.efs

LLDOPTS= -Wl,-I,$(ROOTRLDNAME)

include ../commonrules

fstyp.efs: ${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o fstyp.efs

install: all
	${INSTALL} -F /sbin/fstyp.d -idb "std.sw.unix mr" ${TARGETS}
	${INSTALL} -F /etc -lns ../sbin/fstyp.d -idb "std.sw.unix mr" fstyp.d
