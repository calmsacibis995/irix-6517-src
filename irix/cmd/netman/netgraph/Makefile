#!smake
#
# NetGraph Makefile
#
# $Revision: 1.8 $
# $Date: 1996/02/26 01:27:46 $
#
DEPTH=..
include ${DEPTH}/Makedefs
# OPTIMIZER=-g

CFILES= snoop.c netGraph_xdr.c
C++FILES= arg.c++ colorGadget.c++ colorPatch.c++ editControl.c++ \
	graphGadget.c++ netGraph.c++ ngMain.c++ paramControl.c++ \
	scrollGadget.c++ stripGadget.c++ timeGadget.c++ timeScroll.c++ \
	viewGadget.c++ myLabel.c++

HELPCARDS = NetGraph.general.help NetGraph.edit.help NetGraph.param.help

NC++DEFS=-DDEBUG
NC++OPTS= +a1 +w +p -float -Wf,-XNl8192
NLDLIBS= ${NETVISLDLIBS} -lhelpmsg -ltu -lX11 -lgl -lm -ltt 

TARGETS= netgraph

default all: ${TARGETS}

include ${COMMONRULES}

editControl.o: editLayout.h
editLayout.h : editLayout.h.TEDIT
	${UNSTRINGIFY} < editLayout.h.TEDIT > $@

paramControl.o: paramLayout.h

viewGadget.o: viewLayout.h
viewLayout.h : viewLayout.h.TEDIT
	${UNSTRINGIFY} < viewLayout.h.TEDIT > $@

netgraph: ${OBJECTS}
	${C++F} ${OBJECTS} ${LDFLAGS} -o $@
	tag 0x010 $@
	@${SIZE} -B $@

install: default
	${INSTALL} -F ${NV_INSTALL_BIN} -idb ${DISPLAYIDB} netgraph
	${INSTALL} -lns ${NV_INSTALL_BIN}/netgraph -idb \
		${DISPLAYLINKSIDB} -F ${NV_TARGET_BIN} netgraph
	${INSTALL} -F ${NV_INSTALL_APPDEF} -m 0644 -idb ${APPDEFIDB} NetGraph
	${INSTALL} -lns ${NV_INSTALL_APPDEF}/NetGraph -idb \
		${DISPLAYLINKSIDB} -F ${NV_TARGET_APPDEF} NetGraph
	${INSTALL} -F ${NV_INSTALL_HELP} -idb ${HELPIDB} ${HELPCARDS}
	for f in ${HELPCARDS}; do \
		${INSTALL} -lns ${NV_INSTALL_HELP}/$$f -idb \
			${DISPLAYLINKSIDB} -F ${NV_TARGET_HELPCARDS} $$f; \
	done
