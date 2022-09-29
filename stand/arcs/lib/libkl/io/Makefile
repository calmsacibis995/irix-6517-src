#!${TOOLROOT}/bin/make
#
# This makefile won't work with smake because smake doesn't understand VPATH
#
# Makefile libkl/cmd
#
# This directory contains the CPU board-specific components of the standalone
# kernel.  The files in this directory are machine dependent.
#
# $Revision: 1.6 $

KLDEPTH= ..
include $(KLDEPTH)/libkldefs

# Set up to build machine dependent objects
LLCINCS=-I../../../IP27prom

OBJECTDIR=$(TARGETDIR)
LIBPATH=$(LIBNAME)

CFILES=elsc.c junkuart.c nic.c

include $(LIBKLRULES)
MKDEPRULE= $(EVERYPRODUCT_MKDEPRULE)

nic.c:
	ln -sf ../../libsk/ml/nic.c .
