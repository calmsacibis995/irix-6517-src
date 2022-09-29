# top level Makefile for stand libs

#ident "$Revision: 1.19 $"

DEPTH=../
COMMONPREF=slib
include ../commondefs

SN01_X=\#
SN0_X=\#
SN0XXL_X=\#
SN0PROM_X=\#
SN0XXLPROM_X=\#
T5I2_X=\#
$(PRODUCT)_X=

$(T5I2_X)EXTRADIRS=libwarsc
$(SN01_X)EXTRADIRS=libkl
$(SN0_X)EXTRADIRS=libkl
$(SN0XXL_X)EXTRADIRS=libkl
$(SN0PROM_X)EXTRADIRS=libkl
$(SN0XXLPROM_X)EXTRADIRS=libkl

SUBDIRS= libsc libsk cfg libsl $(EXTRADIRS)
EXPORTS_SUBDIRS=libsc

default clean clobber depend incdepend fluff rtags lint: $(_FORCE)
	@for i in $(SUBDIRS); \
	do \
		echo "=====\tcd $$i; $(MAKE) $@"; \
		( cd $$i; $(MAKE) $@ ); \
	done

include ${COMMONRULES}

tags: $(COMMONPREF)tags
	@for i in $(SUBDIRS); \
	do \
		echo "=====\tcd $$i; $(MAKE) $@"; \
		( cd $$i; $(MAKE) $@ ); \
	done

install: default
	cd libsc; $(MAKE) $@

# exports no longer a nop, because we now export the libraries
# needed to build fx, so fx moves completely over to the irix
# ism (to avoid cross ism symlinks)
exports: default
	$(EXPORTS_SUBDIRS_MAKERULE)
