#
#	Makefile for netvis/event, the event server and library
#
#	$Revision: 1.4 $
#
DEPTH= ..
include ${DEPTH}/Makedefs

SUBDIRS= lib server
HEADERS_SUBDIRS= include
EXPORTS_SUBDIRS= lib

COMMONPREF= event

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
