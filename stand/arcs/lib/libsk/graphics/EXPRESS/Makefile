# Makefile libsk/graphics/EXPRESS
#
# This directory contains the graphics driver for the GR1 board.
#
# $Revision: 1.7 $

SKDEPTH= ../..
include ${SKDEPTH}/libskdefs

#
# Define all XXX_LLCDEFS to be comments.  $(CPUBOARD)_LLCDEFS 
# will override the previous XXX_LLCDEFS commented version 
IP12_LLCDEFS=\#
IP19_LLCDEFS=\#
IP20_LLCDEFS=\#
IP21_LLCDEFS=\#
IP25_LLCDEFS=\#
IP22_LLCDEFS=\#
IP24_LLCDEFS=\#
IP26_LLCDEFS=\#
IP28_LLCDEFS=\#
$(CPUBOARD)_LLCDEFS=

# Only define FLAT_PANEL for systems that support it.
$(IP19_LLCDEFS)LLCDEFS= -D$(CPUBOARD)
$(IP21_LLCDEFS)LLCDEFS= -D$(CPUBOARD)
$(IP25_LLCDEFS)LLCDEFS= -D$(CPUBOARD)

$(IP12_LLCDEFS)LLCDEFS= -D$(CPUBOARD)
$(IP20_LLCDEFS)LLCDEFS= -D$(CPUBOARD)
$(IP22_LLCDEFS)LLCDEFS= -D$(CPUBOARD) -DFLAT_PANEL
$(IP24_LLCDEFS)LLCDEFS= -D$(CPUBOARD) -DFLAT_PANEL
$(IP26_LLCDEFS)LLCDEFS= -D$(CPUBOARD) -DFLAT_PANEL
$(IP28_LLCDEFS)LLCDEFS= -D$(CPUBOARD) -DFLAT_PANEL

# Set up to build machine dependent objects
LLCINCS= -I$(ROOT)/usr/include/EXPRESS/gl
LIBPATH=$(LIBNAME)
OBJECTDIR=$(TARGETDIR)

CFILES = gr2_init.c gr2_tp.c

include $(LIBSKRULES)
