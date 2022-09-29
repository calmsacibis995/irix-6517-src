#
# Copyright 1994 Silicon Graphics, Inc.  All rights reserved.
#
#	Makefile for REACT/pro src/tools
#
#
include ${ROOT}/usr/include/make/commondefs

SUBDIRS= rtmond rtmon_client

default: ${_FORCE}
	@for d in ${SUBDIRS}; do \
		${ECHO} "\tcd $$d; ${MAKE}"; \
		cd $$d; ${MAKE}; cd ..; \
	done

COMMONPREF=react
include ${COMMONRULES}

install ${COMMONTARGS}:
	@for d in ${SUBDIRS}; do \
		${ECHO} "\tcd $$d; ${MAKE} $@"; \
		cd $$d; ${MAKE} $@; cd ..; \
	done

${SUBDIRS}: ${_FORCE}
	cd $@; ${MAKE}

.PRECIOUS: ${SUBDIRS}

_force:
