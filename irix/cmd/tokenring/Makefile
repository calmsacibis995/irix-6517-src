#
# Copyright 1989,1990 Silicon Graphics, Inc.  All rights reserved.
#
#	Makefile for TokenRing
#
#	$Revision: 1.8 $
#
DEPTH=.
include $(DEPTH)/Makedefs

SUBDIRS	=mtr

default: ${_FORCE}
	@for d in ${SUBDIRS}; do \
		${ECHO} "\tcd $$d; ${MAKE}"; \
		cd $$d; ${MAKE}; cd ..; \
	done

COMMONPREF=tr
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
