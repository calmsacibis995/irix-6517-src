#!smake
#
#	Makefile for netvis/event/include
#
#	$Revision: 1.1 $
#
DEPTH=../..
include ${DEPTH}/Makedefs

IFILES=daemonName.h event.h eventStrings.h msg.h ttEventdOpStrs.h ttEventdOps.h

default exports:

headers install: ${_FORCE}
	${INSTALL} -m 0444 -F ${HEADERSPATH}/event -idb ${HEADERSIDB} ${IFILES}

include ${MAKERULES}
