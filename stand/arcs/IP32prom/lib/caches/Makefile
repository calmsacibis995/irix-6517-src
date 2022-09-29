#
# Makefile for libsk
#
MHDEPTH= ../..
include $(MHDEPTH)/mhcommondefs

DEF_CSTYLE = -mips3

# Define R10K debug flags
LOCALDEFINES=
NOX=\#
YESX=\#
$(DEBUGR10K)X=
$(YESX)LOCALDEFINES=-DDEBUGR10K

# A bug in the assembler (5.3 5/30/95) forces us to explictly set the
# cpp to ansi mode.  Also, define _KERNEL since it will be defined in libsk.
LLLCINCS=-I$(MHDEPTH)/debugcard/include
LLCOPTS=-mips3
LLLCDEFS=-D__STDC__=1 -D_KERNEL $(LOCALDEFINES)
OBJECTDIR=../$(PRODUCT).O
COMMONPREF=msk

ASFILES=\
	r4400_cache.s\
	r4600_cache.s\
	r5000_cache.s\
	r10000_cache.s

CFILES=\
	cache_mi.c

TARGETS= targetdir objectdir $(TARGETDIR)/libmsk.a 

default install $(COMMONTARGS) : $(COMMONPREF)$$@

mskdefault: $(TARGETS)


$(TARGETDIR)/libmsk.a: $(OBJECTS)
	cd $(OBJECTDIR); $(AR) cru $@ $(OBJECTS)

targetdir:
	if [ ! -d $(TARGETDIR) ]; then mkdir $(TARGETDIR) ; fi

objectdir:
	if [ ! -d $(OBJECTDIR) ]; then mkdir $(OBJECTDIR) ; fi

include $(MHCOMMONRULES)
