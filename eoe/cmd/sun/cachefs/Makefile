#
# Makefile for Sun-derived executable components of cachefs
#
include	${ROOT}/usr/include/make/commondefs

CVERSION=-xansi

TARGETS= 

DIRS=	common cfsadmin mount umount replacement init tools

default:
	@for d in ${DIRS}; do \
		${ECHO} "====\tcd $$d; ${MAKE}"; \
		cd $$d; ${MAKE}; cd ..; \
	done

COMMONPREF=cachefs
include ${COMMONRULES}

${DIRS}: ${_FORCE}
	@${ECHO} "====\t$@"; cd $@; ${MAKE}

install ${COMMONTARGS}: ${COMMONPREF}$$@
	@for d in ${DIRS}; do \
		${ECHO} "====\tcd $$d; ${MAKE} $@"; \
		cd $$d; ${MAKE} $@; cd ..; \
	done

${COMMONPREF}install:
