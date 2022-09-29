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
LLCINCS=

# when we begin using pmake, this will work:
# LLCDEFS += -D$(CPUBOARD)

OBJECTDIR=$(TARGETDIR)
LIBPATH=$(LIBNAME)

CFILES= hwreg.c hwreg_hub.c hwreg_router.c hwreg_c0.c hwreg_dir.c hwreg_xbow.c

include $(LIBKLRULES)

.SUFFIXES: .hwreg

LLLDIRT = \
	hwreg_compile.o hwreg_util.o hwreg_compile \
	hwreg_hub.c hwreg_router.c hwreg_c0.c hwreg_dir.c hwreg_xbow.c

hwreg_hub.c: hwreg_hub.hwreg hwreg_compile
	./hwreg_compile hub
hwreg_router.c: hwreg_router.hwreg hwreg_compile
	./hwreg_compile router
hwreg_c0.c: hwreg_c0.hwreg hwreg_compile
	./hwreg_compile c0
hwreg_dir.c: hwreg_dir.hwreg hwreg_compile
	./hwreg_compile dir
hwreg_xbow.c: hwreg_xbow.hwreg hwreg_compile
	./hwreg_compile xbow

hwreg_hub.o: hwreg_hub.c
hwreg_router.o: hwreg_router.c
hwreg_c0.o: hwreg_c0.c
hwreg_dir.o: hwreg_dir.c
hwreg_xbow.o: hwreg_xbow.c

XLCFLAGS=-g -32

hwreg_compile: hwreg_compile.o hwreg_util.o
	$(CC) ${XLCFLAGS} -o hwreg_compile hwreg_compile.o hwreg_util.o
hwreg_compile.o: hwreg_compile.c
	$(CC) -DHWREG_COMPILE ${XLCFLAGS} -c -I../../../include hwreg_compile.c
hwreg_util.o: hwreg.c
	$(CC) ${XLCFLAGS} -c -I../../../include -o hwreg_util.o hwreg.c

MKDEPRULE= $(EVERYPRODUCT_MKDEPRULE)
