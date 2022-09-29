# Makefile for libsk/io/adphim
#
# This directory contains Adaptec HIM library code.
#
# The actual code is in
# 	him.c him_init.c himd.c himdinit.c himdiopt.c
# 	himdopt.c and osm.c
#
# They will be compiled if SIMHIM is not defined.
#
# The simulation code is in
# 	simhim.c simdisk.c
#
# They will be compiled only if SIMHIM is defined.
#
#ident "$Revision: 1.1 $"

SKDEPTH=../..
include ${SKDEPTH}/libskdefs

LLCDEFS=-D$(CPUBOARD)
OBJECTDIR=$(TARGETDIR)
LIBPATH=$(LIBNAME)

IP22_X=\#
IP32_X=\#
$(CPUBOARD)_X=

#if $(CPUBOARD) == "IP22"
$(IP22_X)CFILES=adp_dummy.c
#endif

#if $(CPUBOARD) == "IP32"
$(IP32_X)CFILES=him.c himd.c himdopt.c osm.c \
simhim.c simdisk.c
#endif

#
# Rules
#
include $(LIBSKRULES)
MKDEPRULE= $(EVERYPRODUCT_MKDEPRULE)