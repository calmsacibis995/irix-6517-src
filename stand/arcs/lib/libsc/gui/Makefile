# Makefile libsc/gui
# 
# This directory contains standalone gui routines.
#
# $Revision: 1.7 $

SCDEPTH=..
include ${SCDEPTH}/libscdefs

#
# Set up to build cpu architecture and endian dependent objects
OBJECTDIR=$(TARGETDIR)
LIBPATH=$(LIBNAME)

CFILES= core.c background.c text.c button.c dialog.c tpbutton.c gutil.c \
	tfield.c tport.c canvas.c radio.c progress.c pcbm.c
ASFILES=tpstub.s
include $(LIBSCRULES)
