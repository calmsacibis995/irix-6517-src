#!smake

# Copyright 1989,1990 Silicon Graphics, Inc.  All rights reserved.
#
#	Makefile for SMT
#
#	$Revision: 1.14 $

include ${ROOT}/usr/include/make/commondefs

# test must be built after apps so that xpi_flash.firm exists
#ifndef SRC_PRD
SUBDIRS=lib lib32 smtd apps fddivis test
#else
SUBDIRS=lib lib32 smtd apps fddivis
#endif
COMMONPREF=smt

default: ${_FORCE}
	@for d in ${SUBDIRS}; do \
		${ECHO} "\tcd $$d; ${MAKE}"; \
		cd $$d; ${MAKE}; cd ..; \
	done

include ${COMMONRULES}

install ${COMMONTARGS}: ${COMMONPREF}$$@
	@for d in ${SUBDIRS}; do \
		${ECHO} "\tcd $$d; ${MAKE} $@"; \
		cd $$d; ${MAKE} $@; cd ..; \
	done

${COMMONPREF}install:
	${INSTALL} -F /etc/fddi -idb "fddi.sw.fddi config(update)" \
		-m 644 -u root -g sys smtd.mib smtd.conf

${SUBDIRS}: ${_FORCE}
	cd $@; ${MAKE}

.PRECIOUS: ${SUBDIRS}
