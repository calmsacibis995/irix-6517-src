#
# Makefile for lib
#
MHDEPTH= ..
include $(MHDEPTH)/mhcommondefs

DEF_CSTYLE = -mips3

# A bug in the assembler (5.3 5/30/95) forces us to explictly set the
# cpp to ansi mode.  Also, define _KERNEL since it will be defined in libsk.
LLLCINCS=-I$(MHDEPTH)/debugcard/include
LLLCDEFS=-D__STDC__=1 -D_KERNEL
LLCOPTS=-mips3
OBJECTDIR=$(PRODUCT).O

NOX=\#
YESX=\#
$(IP32SIM)X=
$(YESX)LLLCDEFS=-D__STDC__=1 -D_KERNEL -DIP32SIM

SUBDIRS=caches

COMMONPREF=msk

ASFILES=\
	usecdelay.s\
	mte_asm.s\
	IP32asm.s

# Following files are now merged into libsk
#	mace_ec.c			<-------------->   lib/libsk/net
#	mace_16c550.c       <-------------->   lib/libsk/io
#	env.c               <-------------->   lib/libsk/ml
#	flash.c             <-------------->   lib/libsk/ml
#	flashwrite.c        <-------------->   lib/libsk/ml
#	ds2502.c            <-------------->   lib/libsk/ml
#                          the one in libsk/ml is not quite compatable with
#                          the one PROM required, we'll keep it local.
#	ds1685.c            <-------------->   lib/libsk/ml
#	IP32tci.s           <-------------->   lib/libsk/ml
#
# Files that would be better keep local to PROM.
#
#   IP32k.c             <-------------->   lib/libsk/ml/IP32.c
#                                             a mirror of IP32k.c
#	IP32asm.s           <-------------->   lib/libsk/ml/IP32asm.s
#                                             a mirror of IP32asm.s
#	tile.c              <-------------->   lib/libsk/lib/tile.c
#                                             a mirror of tile.c

CFILES=\
	IP32k.c\
	tile.c\
	ds2502.c\
	mte_copy.c\
	mte_stubs.c\
	st16c1451.c\
	DBCuartsim.c\
	DBCuartio.c


TARGETS= targetdir objectdir $(TARGETDIR)/libmsk.a

default install $(COMMONTARGS) : $(COMMONPREF)$$@
	$(SUBDIRS_MAKERULE)

mskdefault: $(TARGETS)

debug:
	echo $(LLLCDEFS) $(IP32SIM)


$(TARGETDIR)/libmsk.a: targetdir $(OBJECTS)
	cd $(OBJECTDIR) ;  ${AR} cru ../$@ $(OBJECTS)

targetdir:
	if [ ! -d $(TARGETDIR) ]; then mkdir $(TARGETDIR) ; fi

objectdir:
	if [ ! -d $(OBJECTDIR) ]; then mkdir $(OBJECTDIR) ; fi

include $(MHCOMMONRULES)
