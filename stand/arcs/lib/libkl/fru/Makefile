#!${TOOLROOT}/bin/make
#
# This makefile won't work with smake because smake doesn't understand VPATH
#
# Makefile libkl/cmd
#
# This directory contains the CPU board-specific components of the standalone
# kernel.  The files in this directory are machine dependent.
#
# $Revision: 1.2 $

KLDEPTH= ..
include $(KLDEPTH)/libkldefs
# Set up to build machine dependent objects
LLCINCS=-I../../../IP27prom

# when we begin using pmake, this will work:
# LLCDEFS += -D$(CPUBOARD)

OBJECTDIR=$(TARGETDIR)
LIBPATH=$(LIBNAME)

CFILES=	sn0_fru_analysis.c sn0_fru_io.c sn0_fru_node.c sn0_fru_tables.c
ASFILES=

include $(LIBKLRULES)
MKDEPRULE= $(EVERYPRODUCT_MKDEPRULE)

sn0_fru_tables.c: sn0_fru_parser.pl sn0_fru_rules.txt
	./sn0_fru_parser.pl sn0_fru_rules.txt .



${KLDEPTH}/${PRODUCT}.O/pvers.c: ${_FORCE}
	${NEWVERS} libkl ${RELEASE} "Rev ${REVISION} ${CPUBOARD}, " "" "" "-DFRU_DEBUG -DDEBUG ${CFLAGS}" libkl > $@
