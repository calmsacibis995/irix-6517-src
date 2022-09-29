#
# Makefile for the IDE libraries.
#
# This makefile normally builds two libraries, libide.a for IDE standalone
# programs and libpide.a for PROM use.  
#
# Users can say either "make ./libide.a" or "make ./libpide.a" to make the
# desired version.
#
DIAGDEPTH=..
include ../Makedefs

IP20_X=\#
IP22_X=\#
IP26_X=\#
IP28_X=\#
IP30_X=\#
$(CPUBOARD)_X=

# SUBDIRS is for IDE build
# PSUBDIRS is for PROM build
$(IP20_X)SUBDIRS= video compression
$(IP20_X)PSUBDIRS=none
$(IP22_X)SUBDIRS= graphics video compression
$(IP22_X)PSUBDIRS=graphics
$(IP26_X)SUBDIRS= video
$(IP26_X)PSUBDIRS=none
$(IP28_X)SUBDIRS= graphics video
$(IP28_X)PSUBDIRS=graphics
$(IP30_X)SUBDIRS= graphics video compression
$(IP30_X)PSUBDIRS=graphics

LTARGETS= ${LIBNAME}
TARGETS= ${LTARGETS}

COMMONPREF=$(CPUBOARD)

default: subdirs

include ../Makerules

# LIBNAME and libpon.a are targets that never wind up existing
# in this dir; they are here for developers used to saying make
# libide.a or make libpon.a
subdirs: $(TDIRS) ${_FORCE}
	@if [ "$(SUBDIRS)" != "none" ]; then \
	for d in ${SUBDIRS}; do \
		echo "======\tcd $$d; ${MAKE} _lib"; \
		cd $$d; ${MAKE} _lib; cd ..; \
	done; fi

prom: ${TDIRS} ${_FORCE}
	@if [ "$(PSUBDIRS)" != "none" ]; then \
	for d in ${PSUBDIRS}; do \
		echo "======\tcd $$d; ${MAKE} LIBTYPE=p PROMFLAGS=-DPROM _plib"; \
		cd $$d; ${MAKE} LIBTYPE=p PROMFLAGS=-DPROM _plib ; cd ..; \
	done; fi

# Common rules which we implement at this level
# and also pass on to sub-makes
$(COMMONTARGS):
	@for i in ${PSUBDIRS} ${SUBDIRS}; do \
		if [ "$$i" != "none" ]; then \
			echo "======\tcd $$i; ${MAKE} $@"; \
			cd $$i; ${MAKE} $@; cd ..; \
		fi \
	done
	@if [ "$@" = "clobber" ]; then \
		rm -rf $(TARGETDIR) ;\
	fi
