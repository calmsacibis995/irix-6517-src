#!smake
#ident $Revision: 1.14 $
include ../commondefs

CFILES= fsstat.c
TARGETS= fsstat

LLDLIBS=-ldisk
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

include ../commonrules

install: all
	${INSTALL} -F /sbin -idb "std.sw.unix mr" ${TARGETS}
	${INSTALL} -F /etc -idb "std.sw.unix mr" -lns ../sbin/${TARGETS} ${TARGETS}

