# Makefile libsk/graphics/LIGHT
#
# This directory contains the graphics driver for the LG1 board.
#
# $Revision: 1.9 $

SKDEPTH= ../..
include ${SKDEPTH}/libskdefs

#
# Set up to build machine dependent objects
LLCDEFS= -D$(CPUBOARD)
LLCINCS= -I$(ROOT)/usr/include/LIGHT/gl
LIBPATH=$(LIBNAME)
OBJECTDIR=$(TARGETDIR)

CFILES = lg1_init.c lg1_tp.c

include $(LIBSKRULES)
