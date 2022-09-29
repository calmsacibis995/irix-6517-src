#ident "ide/godzilla/Makefile: $Revision: 1.52 $"
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

IP30_X=\#
$(CPUBOARD)_X=

# SUBDIRS is for IDE build
$(IP30_X)SUBDIRS= uif heart bridge heart_bridge ecc mem util pci xbow h_x_b ioc3 cache_thrasher scsi rad enet graphics
# PSUBDIRS is for PROM build
$(IP30_X)PSUBDIRS= uif ioc3 
# for IDE CPU Module
$(IP30_X)MODULE_CPU_SUBDIRS= heart bridge heart_bridge ecc mem util pci xbow h_x_b cache_thrasher scsi rad enet graphics
# for Core Ide
$(IP30_X)CORE_SUBDIRS= uif 

LTARGETS= ${LIBNAME}
TARGETS= ${LTARGETS}

COMMONPREF=$(CPUBOARD)

default: subdirs

include ../Makerules

# LIBNAME and libpon.a are targets that never wind up existing
# in this dir; they are here for developers used to saying make
# libide.a or make libpon.a
subdirs: $(TDIRS) ${_FORCE}
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
	@for i in ${PSUBDIRS} ${SUBDIRS}; do \
		echo "======\tcd $$i; ${MAKE} $@"; \
		cd $$i; ${MAKE} $@; cd ..; \
	done
	@if [ "$@" = "clobber" ]; then \
		rm -rf $(TARGETDIR) ;\
	fi

# For IDE CPU module build
_module_cpu:
	@for d in ${MODULE_CPU_SUBDIRS}; do \
		echo "======\tcd $$d; ${MAKE}  SHARE_COPT= SHARE_ASOPT=-KPIC LIBTYPE=module_cpu _module_cpu"; \
		cd $$d; ${MAKE} SHARE_COPT= SHARE_ASOPT=-KPIC LIBTYPE=module_cpu _module_cpu; cd ..; \
	done	

# For Core IDE build
core:
	@for d in ${CORE_SUBDIRS}; do \
		echo "======\tcd $$d; ${MAKE} _core"; \
		cd $$d; ${MAKE} LIBTYPE=core _core; cd ..; \
	done	
