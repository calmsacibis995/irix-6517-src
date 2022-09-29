#!${TOOLROOT}/bin/make
#
# This makefile won't work with smake because smake doesn't understand VPATH
#
# Makefile libkl/cmd
#
# This directory contains the CPU board-specific components of the standalone
# kernel.  The files in this directory are machine dependent.
#
# $Revision: 1.34 $

KLDEPTH= ..
include $(KLDEPTH)/libkldefs
# Set up to build machine dependent objects
LLCINCS=-I../../../IP27prom

# when we begin using pmake, this will work:
# LLCDEFS += -D$(CPUBOARD)

OBJECTDIR=$(TARGETDIR)
LIBPATH=$(LIBNAME)

CFILES=	hub.c bridge.c error_dump.c fprom.c promlog.c ip27log.c xtalk.c \
	i2c.c iodiscover.c widget.c nvram.c bridge_nic.c \
	iodump.c klconfig.c klconflib.c klhubii.c klmalloc.c SN0.c slots.c \
	corp.c router.c vector.c xbow.c xbow_nasid.c ioc3.c SN0nvram.c \
	xbow_init.c klconfdev.c klconfalloc.c IP31.c \
	${KLDEPTH}/${PRODUCT}.O/pvers.c
ASFILES=hub_asm.s rtc.s war.s corp_asm.s setjmp.s diagasm.s

include $(LIBKLRULES)
MKDEPRULE= $(EVERYPRODUCT_MKDEPRULE)

${KLDEPTH}/${PRODUCT}.O/pvers.c: ${_FORCE}
	${NEWVERS} libkl ${RELEASE} "Rev ${REVISION} ${CPUBOARD}, " "" "" "${CFLAGS}" libkl > $@
