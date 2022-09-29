# Makefile libsk/graphics/NEWPORT
#
# This directory contains the graphics driver for the NEWPORT board.
#
# $Revision: 1.6 $

SKDEPTH= ../..
include ${SKDEPTH}/libskdefs

#
# Set up to build machine dependent objects
LLCDEFS= -D$(CPUBOARD) -DHERE -DFLATPANEL
LLCINCS= -I$(ROOT)/usr/include/NEWPORT/gl
LLCOPTS=
LIBPATH=$(LIBNAME)
OBJECTDIR=$(TARGETDIR)

CFILES = ng1_init.c ng1_tp.c 

include $(LIBSKRULES)
