#
# Makefile for the IDE libraries.
#
# This makefile normally builds two libraries, libide.a for IDE standalone
# programs and libpide.a for PROM use.  
#
# Users can say either "make default" or "make prom" to make the
# desired version.
#
DIAGDEPTH=..
include ../Makedefs

SUBDIRS= pon teton cache tlb ecc

PSUBDIRS= pon teton

LTARGETS= ${LIBNAME}
TARGETS= ${LTARGETS}

default: ip26core

COMMONPREF=$(CPUBOARD)
include ../Makerules

ip26core: $(TDIRS) ${_FORCE}
	@for d in ${SUBDIRS}; do \
		echo "======\tcd $$d; ${MAKE} _lib"; \
		cd $$d; ${MAKE} _lib; cd ..; \
	done

prom: ${TDIRS} ${_FORCE}
	@for d in ${PSUBDIRS}; do \
		echo "======\tcd $$d; ${MAKE} LIBTYPE=p PROMFLAGS=-DPROM _plib"; \
		cd $$d; ${MAKE} LIBTYPE=p PROMFLAGS=-DPROM _plib ; cd ..; \
	done

# Common rules which we implement at this level
# and also pass on to sub-makes
$(COMMONTARGS):
	@for i in ${SUBDIRS}; do \
		echo "======\tcd $$i; ${MAKE} $@"; \
		cd $$i; ${MAKE} $@; cd ..; \
	done
	@if [ "$@" = "clobber" ]; then \
		rm -rf $(TARGETDIR) ;\
	fi
