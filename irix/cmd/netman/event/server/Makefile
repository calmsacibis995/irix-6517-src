#!smake
#
#	Makefile for netvis event daemon
#
#	$Revision: 1.14 $
#

EVENTDEPTH=..
TTDEPTH=../tt
include ../../Makedefs
include ${EVENTDEPTH}/Makedefs

NC++INCS= -I../include
C++FILES= main.c++ server.c++ logger.c++ cfg.c++ sessctrl.c++ proctbl.c++
NLDLIBS= -l${LIBNETVISEVENT} -ltt
TARGETS= nveventd
DIDB= -idb ${DISPLAYIDB}
DINFLAGS= ${DIDB} -u root -g sys -m 4555  
CIDB= -idb "${DISPLAYLINKSIDB} config(noupdate)"
CINCFGFLAGS= ${CIDB} -u root -g sys -m 444
CFGFILE = eventcfgrc 

default: ${TARGETS}

include ${EVENTDEPTH}/Makerules

nveventd: ${OBJECTS}
	${C++F} ${OBJECTS} ${LDFLAGS} -o $@
#	tag 0xsomething $@
	@${SIZE} -B $@

install: default
#	nveventd
	${INSTALL}  ${DINFLAGS}  -F ${NV_INSTALL_ETC} ${TARGETS}

	${INSTALL}   -lns ${NV_INSTALL_ETC}/${TARGETS}  -idb \
		${DISPLAYLINKSIDB} -F ${NV_TARGET_USRETC} ${TARGETS}
		
#	evventcfgrc
	${INSTALL}  ${CINCFGFLAGS}  -F ${NV_TARGET_LIB}  ${CFGFILE}
