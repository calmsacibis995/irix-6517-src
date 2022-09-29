# Makefile libsk/graphics/CRIME
#
# This directory contains the graphics driver for CRIME
#
# $Revision: 1.3 $

SKDEPTH= ../..
include ${SKDEPTH}/libskdefs

#
# Set up to build machine dependent objects
LLCDEFS=-D$(CPUBOARD) -DCRM_DUAL_CHANNEL
LIBPATH=$(LIBNAME)
OBJECTDIR=$(TARGETDIR)
LLCINCS=

CFILES= crm_config.c crm_cursor.c crm_i2c.c crm_init.c crm_tp.c

include $(LIBSKRULES)
