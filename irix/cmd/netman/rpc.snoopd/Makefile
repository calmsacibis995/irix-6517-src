#!smake
#
#	Makefile for rpc.snoopd, the remote network snooping daemon.
#
#	$Revision: 1.20 $
#
DEPTH=..
MAKEDEFS= ${DEPTH}/Makedefs
#if ${IRIX} == "SunOS"
MAKEDEFS= ${DEPTH}/Makedefs.sun
#endif
include ${MAKEDEFS}

SRVRHFILES= snoopd.h timer.h
SRVRCFILES= snoopd.c iftable.c timer.c authorize.c \
	    netsnoop.c netlook.c pv_netlook.c histogram.c \
	    addrlist.c pv_addrlist.c
SRVROBJECTS= ${SRVRCFILES:.c=.o}
SRVRTARGETS= rpc.snoopd
SRVRINSTFLAGS= -F ${NV_INSTALL_ETC} -idb ${DATAIDB}

CLNTHFILES=
CLNTCFILES= client.c
CLNTOBJECTS= ${CLNTCFILES:.c=.o}
CLNTTARGETS=

CONFFILES= rpc.snoopd.auth
CONFTARGETS= ${CONFFILES}
CONFINSTFLAGS= -F ${NV_TARGET_ETC} -idb "${DATALINKSIDB} config(noupdate)" -u root -g sys -m 0600

NCINCS= -I${DEPTH}/lib
NLDLIBS= -l${LIBNETVIS}

HFILES= ${SRVRHFILES} ${CLNTHFILES}
CFILES= ${SRVRCFILES} ${CLNTCFILES}
OBJECTS= ${SRVROBJECTS} ${CLNTOBJECTS}
TARGETS= ${SRVRTARGETS} ${CLNTTARGETS}

default: ${TARGETS}

include ${MAKERULES}

rpc.snoopd: ${SRVROBJECTS} ${LIBRARY}
	${CCF} ${SRVROBJECTS} ${LDFLAGS} -o $@
	@if [ "${IRIX}" = "IRIS-4D operating system" ]; then \
		${SIZE} -B $@; \
	else \
		${SIZE} $@; \
	fi

client: ${CLNTOBJECTS} ${LIBRARY}
	${CCF} ${CLNTOBJECTS} ${LIBRARY} ${LDFLAGS} -o $@
	@if [ "${IRIX}" = "IRIS-4D operating system" ]; then \
		${SIZE} -B $@; \
	else \
		${SIZE} $@; \
	fi

install: default
	@if [ "${IRIX}" = "IRIS-4D operating system" ]; then \
		${INSTALL} ${SRVRINSTFLAGS} ${SRVRTARGETS}; \
		${INSTALL} -lns ${NV_INSTALL_ETC}/${SRVRTARGETS}  -idb \
			${DATALINKSIDB} -F ${NV_TARGET_USRETC} ${SRVRTARGETS}; \
		${INSTALL} ${CONFINSTFLAGS} ${CONFTARGETS}; \
	else \
		${INSTALL} -s ${SUNOWN} ${SRVRTARGETS} ${SUNPATH}; \
		${INSTALL} -m 0600 -o root -g staff ${CONFTARGETS} ${SUNPATH};\
	fi
