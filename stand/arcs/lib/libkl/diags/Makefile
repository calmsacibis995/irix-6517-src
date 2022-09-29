#!${TOOLROOT}/bin/make
#
# This makefile won't work with smake because smake doesn't understand VPATH
#
# Makefile libkl/cmd
#
# This directory contains the CPU board-specific components of the standalone
# kernel.  The files in this directory are machine dependent.
#
# $Revision: 1.9 $

KLDEPTH= ..
include $(KLDEPTH)/libkldefs
# Set up to build machine dependent objects
LLCINCS=-I../../../IP27prom

# when we begin using pmake, this will work:
# LLCDEFS += -D$(CPUBOARD)

OBJECTDIR=$(TARGETDIR)
LIBPATH=$(LIBNAME)

CFILES=	diag_lib.c diag_bridge.c diag_xbow.c diag_io6config.c \
	diag_pcibus.c diag_serial.c report.c

include $(LIBKLRULES)
MKDEPRULE= $(EVERYPRODUCT_MKDEPRULE)

${KLDEPTH}/${PRODUCT}.O/pvers.c: ${_FORCE}
	${NEWVERS} libkl ${RELEASE} "Rev ${REVISION} ${CPUBOARD}, " "" "" "${CFLAGS}" libkl > $@

