#
#	Makefile for netvis/man
#
#	$Revision: 1.4 $
#
DEPTH= ..
include ${DEPTH}/Makedefs

SUBDIRS= man1m relnotes
COMMONPREF= netvisman

default install ${COMMONTARGS}: ${_FORCE}
	${SUBDIRS_MAKERULE}

include ${COMMONRULES}

.PRECIOUS: ${SUBDIRS}
