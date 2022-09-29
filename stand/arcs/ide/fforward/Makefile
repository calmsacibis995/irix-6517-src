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
$(CPUBOARD)_X=

# SUBDIRS is for IDE build
# PSUBDIRS is for PROM build
$(IP20_X)SUBDIRS= duart eeprom int2 uif i8254 scope graphics ethernet mem tlb \
		  scsi cache fpu
$(IP20_X)PSUBDIRS=duart eeprom int2 uif graphics scsi
$(IP22_X)SUBDIRS= duart eeprom int2 uif hpc3 eisa i8254 ds1286 pi1 hal2 scope \
		  graphics ethernet mem tlb scsi cache fpu
$(IP22_X)PSUBDIRS=duart eeprom int2 uif pckm graphics scsi
# for IDE Modules
$(IP22_X)MSUBDIRS=duart eeprom int2 uif hpc3 eisa i8254 ds1286 pi1 hal2 scope \
		  ethernet tlb scsi cache mem
# for Core Ide
$(IP22_X)CSUBDIRS=eeprom mem graphics scsi uif
$(IP26_X)SUBDIRS= duart eeprom int2 uif hpc3 eisa i8254 ds1286 pi1 hal2 scope \
		  graphics ethernet mem scsi fpu
$(IP26_X)PSUBDIRS=duart eeprom int2 uif pckm graphics scsi
$(IP28_X)SUBDIRS= duart eeprom int2 uif hpc3 eisa i8254 ds1286 pi1 hal2 scope \
		  graphics ethernet mem scsi cache fpu tlb
$(IP28_X)PSUBDIRS=duart eeprom int2 uif pckm graphics scsi

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

# For Modular IDE builds
module:
	@for d in ${MSUBDIRS}; do \
		echo "======\tcd $$d; ${MAKE} _module"; \
		cd $$d; ${MAKE} SHARE_COPT= SHARE_ASOPT=-KPIC _module; cd ..; \
	done	

# For Core IDE build
core:
	@for d in ${CSUBDIRS}; do \
		echo "======\tcd $$d; ${MAKE} _core"; \
		cd $$d; ${MAKE} _core; cd ..; \
	done	


