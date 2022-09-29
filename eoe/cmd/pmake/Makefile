#
# Makefile for pmake (parallel make)
#
#ident	"$Revision: 1.3 $"

include	${ROOT}/usr/include/make/commondefs

SUBDIRS =lib src

COMMONPREF=pmake_

# Do the subdirs sequentially because src depends on lst.
# (Allows smake to make pmake/smake.)
#
default: ${_FORCE}
	$(SUBDIRS_MAKERULE)

include ${COMMONRULES}

install ${COMMONTARGS}: ${COMMONPREF}$$@
	$(SUBDIRS_MAKERULE)

${COMMONPREF}install:
