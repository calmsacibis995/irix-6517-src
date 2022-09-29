#
# Copyright 1994, 1995 Silicon Graphics, Inc.  All rights reserved.
#
#	Makefile for REACT/pro src/examples directory
#
#
include ${ROOT}/usr/include/make/commondefs
REACTINSTALLDIR=/usr/share/src/react/examples

#
#
SUBDIRS= complete dintr driver eiuli memlock mprogs multi recovery \
	simple sixtyhz stop_resume upreuse rtmon_user_tstamps \
	ext_intr cpu_intr user_intr vsync_intr multimprogs insert \
	simple_pt multi_pt variable_s1 variable_s2 variable_s3 \
	variable_m1 variable_m2 variable_m3 variable_pt_m1

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
		cd $$d; ${MAKE} $@ ; cd ..; \
		else \${ECHO} "\tDirectory $$d missing, skipping"; fi \
	done
	$(INSTALL) -idb REACT_SRC -F ${REACTINSTALLDIR} Makefile README

${SUBDIRS}: ${_FORCE}
	cd $@; ${MAKE}

.PRECIOUS: ${SUBDIRS}

_force:
