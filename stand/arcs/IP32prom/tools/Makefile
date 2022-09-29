#
# Makefile for tools
#
MHDEPTH= ..
include $(MHDEPTH)/mhcommondefs

OBJECTDIR=.

SUBDIRS=\
	flashbuild\
	flashcreate\
	segment

CFILES=\
	futil.c\
	asmtool.c\
	dumptext.c\
	force_mips2.c


TARGETS= futil force_mips2 dumptext asmtool inspect

default $(COMMONTARGS): $(COMMONPREF)$$@
	$(SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(COMMONPREF)default: $(TARGETS)

futil: futil.c
	$(CC) -g -I$(MHDEPTH)/include  futil.c -L$(ROOT)/usr/lib -lelf -o futil

asmtool: asmtool.c
	$(CC) -o $@ asmtool.c 

dumptext: dumptext.c
	$(CC) dumptext.c -L$(ROOT)/usr/lib -lelf -o $@ 

force_mips2: force_mips2.c
	$(CC) -g force_mips2.c -o force_mips2

inspect:
	chmod +x inspect.awk

install: default

include $(MHCOMMONRULES)
