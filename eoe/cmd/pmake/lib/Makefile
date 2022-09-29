#ident	"$Revision: 1.2 $"

include	${ROOT}/usr/include/make/commondefs

SUBDIRS =sprite lst

COMMONPREF=lib_

default:
	$(SUBDIRS_MAKERULE)

include ${COMMONRULES}

${COMMONTARGS}: ${COMMONPREF}$$@
	$(SUBDIRS_MAKERULE)

install: default
	cd mk; ${MAKE} $@

${SUBDIRS}:${_FORCE}
	cd $@; ${MAKE}

