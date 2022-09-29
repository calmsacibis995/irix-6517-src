#!smake
#
# NetTop Makefile
#
# $Revision: 1.16 $
# $Date: 1996/02/26 01:27:57 $
#
DEPTH=..
include ${DEPTH}/Makedefs
# OPTIMIZER=-g

CFILES= snoop.c
C++FILES= dataGizmo.c++ font.c++ fontdump.c++ graphGadget.c++ \
	  ntMain.c++ netTop.c++ nodeGizmo.c++ top.c++ viewGadget.c++
HELPCARDS = NetTop.general.help NetTop.node.help NetTop.traffic.help

NC++DEFS=-DDEBUG
NC++OPTS= +a1 +w +p -float -Wf,-XNl8192
NLDLIBS= ${NETVISLDLIBS} -lhelpmsg -ltu -lX11 -lgl -lm -ltt 

TARGETS= nettop

default all: ${TARGETS}

include ${COMMONRULES}

nodeGizmo.o: nodeLayout.h

nodeLayout.h : nodeLayout.h.TEDIT
	${UNSTRINGIFY} < nodeLayout.h.TEDIT > $@

dataGizmo.o: dataLayout.h

dataLayout.h : dataLayout.h.TEDIT
	${UNSTRINGIFY} < dataLayout.h.TEDIT > $@

nettop: ${OBJECTS}
	${C++F} ${OBJECTS} ${LDFLAGS} -o $@
	tag 0x010 $@
	@${SIZE} -B $@

install: default
	${INSTALL} -F ${NV_INSTALL_BIN} -idb ${DISPLAYIDB} nettop
	${INSTALL} -lns ${NV_INSTALL_BIN}/nettop -idb \
		${DISPLAYLINKSIDB} -F ${NV_TARGET_BIN} nettop
	${INSTALL} -F ${NV_INSTALL_APPDEF} -m 0644 -idb ${APPDEFIDB} NetTop
	${INSTALL} -lns ${NV_INSTALL_APPDEF}/NetTop -idb \
		${DISPLAYLINKSIDB} -F ${NV_TARGET_APPDEF} NetTop
	${INSTALL} -F ${NV_INSTALL_HELP} -idb ${HELPIDB} ${HELPCARDS}
	for f in ${HELPCARDS}; do \
		${INSTALL} -lns ${NV_INSTALL_HELP}/$$f -idb \
			${DISPLAYLINKSIDB} -F ${NV_TARGET_HELPCARDS} $$f; \
	done
