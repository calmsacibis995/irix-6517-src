#!smake
#
# $Revision: 1.26 $
# $Date: 1996/02/26 01:27:48 $
#
DEPTH=..
include ${DEPTH}/Makedefs

C++FILES= main.c++ netlook.c++ nlwindow.c++ \
	  viewgadget.c++ netlookgadget.c++ menugadget.c++ \
	  fileoptions.c++ gadgetoptions.c++ actionoptions.c++ helpoptions.c++ \
	  snoopgadget.c++ netnodegadget.c++ trafficgadget.c++ hidegadget.c++ \
	  mapgadget.c++ glmapgadget.c++ selectiongadget.c++ \
	  gltilegadget.c++ infogadget.c++ extralabel.c++ \
	  cf.c++ cf_parse.c++ cf_print.c++ db.c++ \
	  network.c++ node.c++ position.c++ nis.c++ \
	  conntable.c++ connection.c++ \
	  comm.c++ \
	  font.c++ fontdump.c++ \
	  trig.c++
CFILES= cf_lex.c
HELPCARDS= NetLook.general.help NetLook.main.help NetLook.new.help \
	   NetLook.datafile.help NetLook.uifile.help NetLook.snoop.help \
	   NetLook.map.help NetLook.netnode.help NetLook.traffic.help \
	   NetLook.hide.help NetLook.info.help NetLook.ping.help \
	   NetLook.trace.help NetLook.home.help NetLook.delete.help \
	   NetLook.tools.help

NC++OPTS= +a1 +w +p -float -Wf,-XNl8192
NLDLIBS= ${NETVISLDLIBS} -lhelpmsg -ltu -lX11 -lgl -lm -ltt

TARGETS= netlook

default: ${TARGETS}

include ${COMMONRULES}

netlook: ${OBJECTS}
	${C++F} ${OBJECTS} ${LDFLAGS} -o $@
	tag 0x010 $@
	@${SIZE} -B $@

menugadget.o: menugadgetlayout.h

menugadgetlayout.h: menugadgetlayouttedit.h
	${UNSTRINGIFY} < menugadgetlayouttedit.h > $@

install: default
	${INSTALL} -F ${NV_INSTALL_BIN} -idb ${DISPLAYIDB} netlook
	${INSTALL} -lns ${NV_INSTALL_BIN}/netlook -idb \
		${DISPLAYLINKSIDB} -F ${NV_TARGET_BIN} netlook
	${INSTALL} -F ${NV_INSTALL_APPDEF} -m 0644 -idb ${APPDEFIDB} NetLook
	${INSTALL} -lns ${NV_INSTALL_APPDEF}/NetLook -idb \
		${DISPLAYLINKSIDB} -F ${NV_TARGET_APPDEF}  NetLook
	${INSTALL} -F ${NV_INSTALL_HELP} -idb ${HELPIDB} ${HELPCARDS}
	for f in ${HELPCARDS}; do \
		${INSTALL} -lns ${NV_INSTALL_HELP}/$$f -idb \
			${DISPLAYLINKSIDB} -F ${NV_TARGET_HELPCARDS} $$f; \
	done
