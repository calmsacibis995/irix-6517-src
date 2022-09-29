#ident "ide/IP32/Makefile: $Revision: 1.3 $"
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

SUBDIRS=pon cache tlb mace fpu util uif mem scsi graphics

PSUBDIRS= pon

LTARGETS= ${LIBNAME}
TARGETS= ${LTARGETS}

default: ip32core

COMMONPREF=$(CPUBOARD)
include ../Makerules

ip32core: $(TDIRS) ${_FORCE}
	@for d in ${SUBDIRS}; do \
		echo "======\tcd $$d; ${MAKE} _lib"; \
		cd $$d; ${MAKE} _lib; cd ..; \
	done

prom: ${TDIRS} ${_FORCE}
	@for d in ${PSUBDIRS}; do \
		echo "======\tcd $$d; ${MAKE} LIBTYPE=p _plib"; \
		cd $$d; ${MAKE} LIBTYPE=p _plib ; cd ..; \
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
