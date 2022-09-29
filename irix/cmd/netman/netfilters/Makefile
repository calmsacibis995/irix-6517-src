#!smake
#
#	NetFilters Makefile
#
#	$Revision: 1.12 $
#
DEPTH=..
include ${DEPTH}/Makedefs

C++FILES= archMain.c++ archiver.c++ blankFiller.c++ dblLabel.c++ fancyList.c++
HELPCARDS= NetFilters.main.help NetFilters.filler.help \
	   NetFilters.main.helpfile NetFilters.filler.helpfile
FILTERFILE= Filters
NC++DEFS= -DDEBUG
LLDLIBS= -l${LIBNETVIS} -lhelpmsg -ltu -lX11 -lm

TARGETS= netfilters

default: ${TARGETS}

archiver.o: archiverLayout.h

archiverLayout.h : archiverLayout.h.TEDIT
	${UNSTRINGIFY} < archiverLayout.h.TEDIT > $@

blankFiller.o: blankFillerLayout.h

blankFillerLayout.h : blankFillerLayout.h.TEDIT
	${UNSTRINGIFY} < blankFillerLayout.h.TEDIT > $@

include ${COMMONRULES}

netfilters: ${OBJECTS} ${UILIBRARY}
	${C++F} -o $@ ${OBJECTS} ${UILIBRARY} ${LDFLAGS}
	tag 0x010 $@
	@${SIZE} -B $@

install: default
	${INSTALL} -F ${NV_INSTALL_BIN} -idb ${DISPLAYIDB} netfilters
	${INSTALL} -lns ${NV_INSTALL_BIN}/netfilters -idb \
		${DISPLAYLINKSIDB} -F ${NV_TARGET_BIN} netfilters
	${INSTALL} -F ${NV_INSTALL_APPDEF} -m 0644 -idb ${APPDEFIDB} NetFilters
	${INSTALL} -lns ${NV_INSTALL_APPDEF}/NetFilters -idb \
		${DISPLAYLINKSIDB} -F ${NV_TARGET_APPDEF} NetFilters

	${INSTALL} -F ${NV_INSTALL_HELP} -idb ${HELPIDB} ${HELPCARDS}
	for f in ${HELPCARDS}; do \
		$(INSTALL)   -lns ${NV_INSTALL_HELP}/$$f  -idb \
		$(DISPLAYLINKSIDB) -F ${NV_TARGET_HELPCARDS} $$f; \
	done
	${INSTALL} -F ${NV_INSTALL_LIB} -idb ${DISPLAYIDB} ${FILTERFILE}
	${INSTALL} -lns ${NV_INSTALL_LIB}/${FILTERFILE} -idb \
		${DISPLAYLINKSIDB} -F ${NV_TARGET_LIB} ${FILTERFILE}
