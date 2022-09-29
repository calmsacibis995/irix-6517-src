# Makefile libsk/graphics/EXPRESS
#
# This directory contains the graphics driver for the GR1 board.
#
# $Revision: 1.10 $

SKDEPTH= ../..
include ${SKDEPTH}/libskdefs

# ignore ucode illegal shifts (code not executed).
LLWOFF=,848

#
# Set up to build machine dependent objects
LLCDEFS= -D$(CPUBOARD) -DFLAT_PANEL
LIBPATH=$(LIBNAME)
OBJECTDIR=$(TARGETDIR)

# The IP27 prom is extremely space limited.  These two changes
# minimize the size of the standalone textport code for IP27.
#   + do not build -DFLAT_PANEL for IP27
#   + replace mgras_re4 macros with procedures
IP27_X=\#
IP27XXL_X=\#
$(CPUBOARD)_X=
$(IP27_X)LLCDEFS= -D$(CPUBOARD) -DRE4_NON_INLINE
$(IP27XXL_X)LLCDEFS= -D$(CPUBOARD) -DRE4_NON_INLINE

CFILES = mgras_init.c mgras_timing.c mgras_tport.c mgras_ccv.c mgras_corona_i2c.c

include $(LIBSKRULES)

