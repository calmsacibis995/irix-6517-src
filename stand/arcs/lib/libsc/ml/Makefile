# Makefile libsc/ml
#
# This directory contains architecture specific routines for the Mips
# processor and compiler.
#
# $Revision: 1.19 $

SCDEPTH=..
include $(SCDEPTH)/libscdefs

#
# Set up to build cpu architecture and endian dependent objects
OBJECTDIR=$(TARGETDIR)
LIBPATH=$(LIBNAME)

# !AVOIDING_DUMB_HACKS <-- Marker :-)
# The ll* files support long long's in the cfe compiler.
# Once we all compile with -cfe -dwopcode, all of these long long
# emulation files can be removed.

ASFILES= btrace.s csu.s llmul.s lldivrem.s llcvt.s ip20cache.s
CFILES= bt.c lldiv.c llshift.c llbit.c stack.c

include $(LIBSCRULES)
