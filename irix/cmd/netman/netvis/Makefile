#!smake
#
#	Makefile for netvis launch tool
#
#	$Revision: 1.3 $
#
DEPTH=..
include ${DEPTH}/Makedefs

default:

include ${COMMONRULES}

install:
	${INSTALL} -F ${NV_INSTALL} -idb ${DISPLAYIDB} netvis
	${INSTALL} -lns ${NV_INSTALL}/netvis -idb \
		${DISPLAYLINKSIDB} -F ${NV_TARGET_BIN} netvis
