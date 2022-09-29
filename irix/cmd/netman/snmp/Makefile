#
# Makefile for cmd/netman/snmp, the simple network management protocol
#
DEPTH=..
include ${DEPTH}/Makedefs

SUBDIRS= include lib libagent agent # browser 
HEADERS_SUBDIRS= include
EXPORTS_SUBDIRS= lib libagent

COMMONPREF=snmp

default install ${COMMONTARGS}: ${COMMONPREF}$$@
	${SUBDIRS_MAKERULE}

include ${DEPTH}/Makerules

headers: ${_FORCE}
	${HEADERS_SUBDIRS_MAKERULE}

exports: ${_FORCE}
	${EXPORTS_SUBDIRS_MAKERULE}

${COMMONPREF}default ${COMMONPREF}install:

${SUBDIRS}: ${_FORCE}
	cd $@; ${MAKE}

.PRECIOUS: ${SUBDIRS} ${HEADERS_SUBDIRS}
