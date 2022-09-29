#!smake
#
# Makefile for parser
#
SNMPDEPTH=..
include ${SNMPDEPTH}/../Makedefs
include ${SNMPDEPTH}/Makedefs

LFILES= mib_scan.l
YFILES= mib_gram.y
C++FILES= browserMain.c++ bTopLevel.c++ \
	  desc.c++ group.c++ indiv.c++ table.c++ \
	  browser.c++ mibquery.c++
HELPCARDS = Browser.desc.help Browser.indiv.help Browser.table.help \
	  Browser.group.help Browser.main.help Browser.desc.helpfile \
	  Browser.indiv.helpfile Browser.table.helpfile \
	  Browser.group.helpfile Browser.main.helpfile

MIBS= mib2 cisco.mib cabletron.mib rmon.mib
MIBDIR= /usr/lib/netvis/mibs

YFLAGS= -d
NC++OPTS= +a1 +w +p -float -Wf,-XNl8192
NLDLIBS= -ll ../lib/libsnmp.a -lnetman -lnetls -lnck -lnetvisui -lnetvisevent -lhelpmsg -ltu -lX11 -ltt -lm 
# Needed for yacc
CVERSION= -cckr

TARGETS= browser

default: ${TARGETS}

include ${SNMPDEPTH}/Makerules

group.o: y.tab.h

indiv.o: y.tab.h

table.o: y.tab.h

mib_scan.l: y.tab.h

y.tab.h: mib_gram.o

parse: ${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -DDO_PARSE_MAIN -o $@
	@${SIZE} -B $@

browser: ${OBJECTS} ${NVLIBS} ${SNMPLIBRARY}
	${C++} ${OBJECTS} ${LDFLAGS} ${NLDLIBS} -o $@
	tag 0x010 $@
	@${SIZE} -B $@

install: default
	${INSTALL} -F ${NV_INSTALL_BIN} -idb ${DISPLAYIDB} browser
	${INSTALL} -lns ${NV_INSTALL_BIN}/browser -idb \
		${DISPLAYLINKSIDB} -F ${NV_TARGET_BIN} browser
	${INSTALL} -F ${NV_INSTALL_MIBS} -idb ${DISPLAYIDB} ${MIBS}
	for f in ${MIBS}; do \
		${INSTALL} -lns ${NV_INSTALL_MIBS}/$$f -idb \
			${DISPLAYLINKSIDB} -F ${NV_TARGET_MIBS} $$f; \
	done
	${INSTALL} -F ${NV_INSTALL_APPDEF} -m 0644 -idb ${APPDEFIDB} Browser
	${INSTALL} -lns ${NV_INSTALL_APPDEF}/Browser -idb \
		${DISPLAYLINKSIDB} -F ${NV_TARGET_APPDEF} Browser
	${INSTALL} -F ${NV_INSTALL_HELP} -idb ${HELPIDB} ${HELPCARDS}
	for f in ${HELPCARDS}; do \
		${INSTALL} -lns ${NV_INSTALL_HELP}/$$f -idb \
			${DISPLAYLINKSIDB} -F ${NV_TARGET_HELPCARDS} $$f; \
	done
