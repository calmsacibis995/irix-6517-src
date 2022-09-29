#!smake
#
# Makefile for netcollect, netpack, and netaccount.
#
#	$Revision: 1.10 $
#
DEPTH=..
MAKEDEFS= ${DEPTH}/Makedefs
#if ${IRIX} == "SunOS"
MAKEDEFS= ${DEPTH}/Makedefs.sun
#endif
include ${MAKEDEFS}

COLLHFILES= collect.h
COLLCFILES= collect.c collect_ops.c collect_io.c collect_xdr.c addr.c filename.c
COLLOBJECTS= ${COLLCFILES:.c=.o}

ACCTHFILES= account.h
ACCTCFILES= account.c account_io.c account_xdr.c collect_xdr.c addr.c
ACCTOBJECTS= ${ACCTCFILES:.c=.o}

PACKHFILES= collect.h
PACKCFILES= pack.c account_io.c account_xdr.c collect_xdr.c addr.c filename.c
PACKOBJECTS= ${PACKCFILES:.c=.o}

HFILES= ${COLLHFILES} ${ACCTHFILES} ${PACKHFILES}
CFILES= ${COLLCFILES} ${ACCTCFILES} ${PACKCFILES}
OBJECTS= ${COLLOBJECTS} ${ACCTOBJECTS} ${PACKOBJECTS}
TARGETS= netcollect netaccount netpack
NLDLIBS= -l${LIBNETVIS} -lnetls -lnck

default: ${TARGETS}

include ${MAKERULES}

netcollect: ${COLLOBJECTS} ${LIBRARY}
	${CCF} ${COLLOBJECTS} ${LDFLAGS} -o $@
	@if [ "${IRIX}" = "IRIS-4D operating system" ]; then \
		tag 0x010 $@; \
		${SIZE} -B $@; \
	else \
		${SIZE} $@; \
	fi

netaccount: ${ACCTOBJECTS} ${LIBRARY}
	${CCF} ${ACCTOBJECTS} ${LDFLAGS} -o $@
	@if [ "${IRIX}" = "IRIS-4D operating system" ]; then \
		tag 0x720 $@; \
		${SIZE} -B $@; \
	else \
		${SIZE} $@; \
	fi

netpack: ${PACKOBJECTS} ${LIBRARY}
	${CCF} ${PACKOBJECTS} ${LDFLAGS} -o $@
	@if [ "${IRIX}" = "IRIS-4D operating system" ]; then \
		tag 0x120 $@; \
		${SIZE} -B $@; \
	else \
		${SIZE} $@; \
	fi

install: default
	@if [ "${IRIX}" = "IRIS-4D operating system" ]; then \
		${INSTALL} -F ${NV_INSTALL_BIN} -idb ${DATAIDB} \
			netcollect netpack netaccount; \
		${INSTALL} -lns ${NV_INSTALL_BIN}/netcollect -idb \
			${DATALINKSIDB} -F ${NV_TARGET_BIN} netcollect; \
		${INSTALL} -lns ${NV_INSTALL_BIN}/netpack -idb \
			${DATALINKSIDB} -F ${NV_TARGET_BIN} netpack; \
		${INSTALL} -lns ${NV_INSTALL_BIN}/netaccount -idb \
			${DATALINKSIDB} -F ${NV_TARGET_BIN} netaccount; \
	else \
		${INSTALL} -s ${SUNOWN} ${TARGETS} ${SUNPATH}; \
	fi
