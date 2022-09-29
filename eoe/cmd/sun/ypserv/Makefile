#!smake
#
# Makefile for ypserv, the NIS server.
#
include ${ROOT}/usr/include/make/commondefs

CVERSION=-cckr
LCINCS= -I.. -I../yp
LCDEFS= -D_BSD_TIME

HFILES= ypsym.h
CFILES= ypdbname.c ypserv_ancil.c ypserv_map.c ypserv_proc.c ypserv.c
TARGETS= ypserv

default: ${TARGETS}

include ${COMMONRULES}

ypserv: ${OBJECTS} 
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@

install: default
	${INSTALL} -F /usr/etc -m 755 -u root -g sys ${TARGETS}
