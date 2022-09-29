#
# Makefile for Sun-derived executable components and configuration files
#
include	${ROOT}/usr/include/make/commondefs

TARGETS= 

#
# common must be made first
#
DIRS=	common functionality stress sysid nprocess tools

default:
	@for d in ${DIRS}; do \
		${ECHO} "====\tcd $$d; ${MAKE}"; \
		cd $$d; ${MAKE}; cd ..; \
	done

COMMONPREF=sun
include ${COMMONRULES}

${DIRS}: ${_FORCE}
	@${ECHO} "====\t$@"; cd $@; ${MAKE}

install ${COMMONTARGS}:
	@for d in ${DIRS}; do \
		${ECHO} "====\tcd $$d; ${MAKE} $@"; \
		cd $$d; ${MAKE} $@; cd ..; \
	done
