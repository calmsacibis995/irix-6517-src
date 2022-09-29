#ident "ide/IP30/Makefile: $Revision: 1.15 $"
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

SUBDIRS=util pon cache tlb fpu debugtool

PSUBDIRS= pon

# for Core Ide
CORE_SUBDIRS=pon

# for Ide CPU Module
MODULE_CPU_SUBDIRS=cache tlb 

LTARGETS= ${LIBNAME}
TARGETS= ${LTARGETS}

default: ip30core ip30mfg

COMMONPREF=$(CPUBOARD)
include ../Makerules

ip30core: $(TDIRS) ${_FORCE}
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

ip30mfg:
	rm -f ip30imp
	cat ip30imp.pre | sed -e 's/DIAG //' -e 's/FUNC //' > ip30imp

### For Core IDE build
##core:
##	@for d in ${CORE_SUBDIRS}; do \
##		echo "======\tcd $$d; ${MAKE} _core"; \
##		cd $$d; ${MAKE} LIBTYPE=core _core; cd ..; \
##	done	
##
### For IDE cpu module build
##_module_cpu:
##	@for d in ${MODULE_CPU_SUBDIRS}; do \
##		echo "======\tcd $$d; ${MAKE} SHARE_COPT= SHARE_ASOPT=-KPIC LIBTYPE=module_cpu _module_cpu"; \
##		cd $$d; ${MAKE} SHARE_COPT= SHARE_ASOPT=-KPIC LIBTYPE=module_cpu _module_cpu; cd ..; \
##	done	
