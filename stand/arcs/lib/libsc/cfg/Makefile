# Makefile for libsc/cfg
#
# This directory contains standalone C configuration (no -D_KERNEL).
#
# $Revision: 1.4 $

SCDEPTH= ..
include $(SCDEPTH)/libscdefs

#
# Set up to build cpu architecture and endian dependent objects
OBJECTDIR=$(TARGETDIR)
LIBPATH=$(LIBNAME)

COMMONPREF=cfg

CFFILES= sctune.cf
CFILES= $(CFFILES:.cf=.c)

include $(LIBSCRULES)

incdepend depend: ${CFFILES:.cf=.c}
lint $(COMMONTARGS): $(COMMONPREF)$$@

LLDIRT= $(ALLCFDIRT)
