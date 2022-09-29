#!smake
#
#	Makefile for NetVisualyzer common UI tools
#
#	$Revision: 1.2 $
#
DEPTH= .

COMMONDEFS= ${ROOT}/usr/include/make/commondefs
include ${COMMONDEFS}

SUBDIRS= lib
EXPORTS_SUBDIRS= lib
HEADERS_SUBDIRS= include

COMMONPREF=netvisui

default install ${COMMONTARGS}: ${COMMONPREF}$$@
	${SUBDIRS_MAKERULE}

headers: ${_FORCE}
	${HEADERS_SUBDIRS_MAKERULE}

exports: ${_FORCE}
	${EXPORTS_SUBDIRS_MAKERULE}

include ${COMMONRULES}

${COMMONPREF}default ${COMMONPREF}install:

${SUBDIRS}: ${_FORCE}
	cd $@; ${MAKE}

.PRECIOUS: ${SUBDIRS}
