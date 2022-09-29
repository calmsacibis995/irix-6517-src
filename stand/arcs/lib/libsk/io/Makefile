#!${TOOLROOT}/bin/make
#
# Makefile for libsk/io
#
# This directory contains standalone device drivers.
# The files in this directory may be  machine dependent.
#
# $Revision: 1.65 $

SKDEPTH= ..
include $(SKDEPTH)/libskdefs

IP19_X=\#
IP20_Y=\#
IP21_X=\#
IP22_Y=\#
IP25_X=\#
IP26_X=\#
IP27_X=\#
IP27XXL_X=\#
IP27H1_X=\#
IP27SABLE_X=\#
IP28_X=\#
IP30_X=\#
IP32_Y=\#
$(CPUBOARD)$(SUBPRODUCT)_X=
$(CPUBOARD)_Y=

IOC3_PCKM=ioc3_pckm.c pc_kbtable.c
PC_PCKM=pc_kbd.c pc_kbtable.c
SUBDIRS=none

BASEIP27FILES=hubuart.c klioc3.c scsidsk.c ql.c ns16550cons.c elsc.c \
	      klgraphics.c $(IOC3_PCKM)

$(IP19_X)MACHCFILES=ccuart.c z8530cons.c sgi_kbd.c wd95.c wd95a_wcs.c
$(IP1932_X)MACHCFILES=ccuart.c z8530cons.c sgi_kbd.c wd95.c wd95a_wcs.c
$(IP20_Y)MACHCFILES=scsi.c sgi_kbd.c z8530cons.c
$(IP21_X)MACHCFILES=ccuart.c z8530cons.c sgi_kbd.c wd95.c wd95a_wcs.c
$(IP22_Y)MACHCFILES=scsi.c z8530cons.c $(PC_PCKM) corona_i2c.c
$(IP25_X)MACHCFILES=ccuart.c z8530cons.c sgi_kbd.c wd95.c wd95a_wcs.c
$(IP26_X)MACHCFILES=scsi.c z8530cons.c $(PC_PCKM) corona_i2c.c
$(IP27_X)MACHCFILES=$(BASEIP27FILES)
$(IP27XXL_X)MACHCFILES=$(BASEIP27FILES)
$(IP27H1_X)MACHCFILES=$(BASEIP27FILES)
$(IP27SABLE_X)MACHCFILES= sabledsk.c $(BASEIP27FILES)
$(IP28_X)MACHCFILES=scsi.c z8530cons.c $(PC_PCKM) corona_i2c.c
$(IP30_X)MACHCFILES=ns16550cons.c ql.c $(IOC3_PCKM)
$(IP32_Y)MACHCFILES=pci_intf.c adp78.c scsi_common.c mace_16c550.c mh_kbd.c pc_kbtable.c
$(IP32_Y)SUBDIRS=adphim

# Set up to build machine dependent objects

OBJECTDIR=$(TARGETDIR)
LIBPATH=$(LIBNAME)

CFILES= cdrom.c dksc.c ioutils.c lboot.c null.c mem.c sgi_ms.c tpsc.c \
	$(MACHCFILES)
default: $(CFILES) $(SUBDIRS)

install: default

clobber: lclobber

include $(LIBSKRULES)
MKDEPRULE= $(EVERYPRODUCT_MKDEPRULE)
${SUBDIRS}: ${_FORCE}
	for d in "${SUBDIRS}"; do \
		if [ "$(SUBDIRS)" = "none" ] ; \
			then exit 0; fi; \
		echo "=======\tcd $$d"; \
		( cd $$d; exec ${MAKE} ); cd ..; \
		echo "======\tio level"; \
	done;

lclobber: ${_FORCE}
	for d in "${SUBDIRS}"; do \
		if [ "$(SUBDIRS)" = "none" ] ; \
			then exit 0; fi; \
		echo "=======\tcd $$d"; \
		( cd $$d; exec ${MAKE} clobber); cd ..; \
		echo "======\tio level"; \
	done;

