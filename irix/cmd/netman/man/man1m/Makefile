#
#	Makefile for netvis man pages
#
#	$Revision: 1.1 $
#
include ${ROOT}/usr/include/make/commondefs

SUBDIRS= display data
COMMONPREF= netvis

default install ${COMMONTARGS}: ${COMMONPREF}$$@
	${SUBDIRS_MAKERULE}

include ${COMMONRULES}

${COMMONPREF}default ${COMMONPREF}install:

${SUBDIRS}: ${_FORCE}
	cd $@; ${MAKE}

.PRECIOUS: ${SUBDIRS}
