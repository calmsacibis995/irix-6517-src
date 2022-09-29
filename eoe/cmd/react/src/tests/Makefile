#
# Copyright 1994 Silicon Graphics, Inc.  All rights reserved.
#
#	Makefile for REACT/pro src/tests directory
#
#
DEPTH=../..
include $(DEPTH)/Makedefs

#
#
SUBDIRS = cctimer_intr_00 ext_intr_00 r4ktimer_intr_00 \
		user_intr_00 vsync_intr_00 dyn_queues_00

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
	done; \
        $(INSTALL) -idb "react.sw.frs" -F /usr/react/src/tests Makefile 00runme

${SUBDIRS}: ${_FORCE}
	cd $@; ${MAKE}

.PRECIOUS: ${SUBDIRS}

_force:
