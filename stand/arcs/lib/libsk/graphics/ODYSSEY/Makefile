# Makefile libsk/graphics/ODYSSEY
#
# This directory contains the graphics driver for the Odyssey Rev. A board.
#
# $Revision: 1.1 $

SKDEPTH= ../..
include ${SKDEPTH}/libskdefs

# ignore ucode illegal shifts (code not executed).
LLWOFF=,848

#
# Set up to build machine dependent objects
# ODSY_NO_FC shuts off flow control withing this build.
LLCDEFS= -D$(CPUBOARD) -DFLAT_PANEL -DODSY_NO_FC=1 -Xcpluscomm 

# Textport bringup tools
#LLCDEFS+= -DTEXTPORT_BRINGUP
#LLCDEFS+= -DDEBUG

LIBPATH=$(LIBNAME)
OBJECTDIR=$(TARGETDIR)

CFILES = 	odsy_tport.c \
			odsy_init.c \
			odsy_timings.c \
			odsy_brdmgr.c \
			odsy_sdram.c \
			odsy_adt.c \
			odsy_i2c.c \
			odsy_ddc.c \
			odsy_flow.c 

include $(LIBSKRULES)

