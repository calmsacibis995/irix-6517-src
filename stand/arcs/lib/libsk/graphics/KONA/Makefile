# Makefile libsk/graphics/KONA
#
# This directory contains the graphics driver for KONA board
#

SKDEPTH= ../..
include ${SKDEPTH}/libskdefs

#
# Set up to build machine dependent objects
#
LLCDEFS= -D$(CPUBOARD) -DDG_CURSOR_WAR -DIP19_32BIT_MODE


LIBPATH=$(LIBNAME)
OBJECTDIR=$(TARGETDIR)
LLCINCS=-I$(ROOT)/usr/include/KONA/gl -I$(ROOT)/usr/include/gl \
	-I$(ROOT)/usr/include/KONA -I$(ROOT)/usr/include \
        -I$(ROOT)/usr/include/sys -I.

CFILES= kona_init.c kona_tport.c kona_logo.c
include $(LIBSKRULES)
