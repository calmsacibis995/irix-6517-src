#
# Copyright 1994 Silicon Graphics, Inc.  All rights reserved.
#
#	Makefile for REACT/pro src directory
#
#
DEPTH=..
include $(DEPTH)/Makedefs

#
#
SUBDIRS= examples tests tools

default: ${_FORCE}
	@for d in ${SUBDIRS}; do \
		if test -d $$d ; then \${ECHO} "\tcd $$d; ${MAKE} $@"; \
		cd $$d; ${MAKE} $@; cd ..; \
		else \${ECHO} "\tDirectory $$d missing, skipping"; fi \
	done

COMMONPREF=react
include ${COMMONRULES}

install ${COMMONTARGS}:
	@for d in ${SUBDIRS}; do \
		if test -d $$d ; then \${ECHO} "\tcd $$d; ${MAKE} $@"; \
		cd $$d; ${MAKE} $@; cd ..; \
		else \${ECHO} "\tDirectory $$d missing, skipping"; fi \
	done; 

${SUBDIRS}: ${_FORCE}
	cd $@; ${MAKE}

.PRECIOUS: ${SUBDIRS}

_force:
