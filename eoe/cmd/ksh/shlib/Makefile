#!smake
#	Makefile for sh edit library.
#

OBJECT_STYLE=N32
include $(ROOT)/usr/include/make/commondefs

TARGETS	= libsh.a

LCINCS	= -I../include
LCDEFS	= -DKSHELL
LCOPTS  = -signed -non_shared
OPTIMIZER=-O3 -TARG:platform=ip22_4k

CFILES = adjust.c assign.c assnum.c cannon.c chkid.c convert.c findnod.c \
	gettree.c growaray.c gsort.c linknod.c namscan.c optget.c rjust.c \
	strdata.c streval.c strmatch.c tilde.c unassign.c utos.c valup.c

default install: $(TARGETS)

include $(COMMONRULES)

$(TARGETS): $(OBJECTS)
	$(AR) c$(ARFLAGS) $(.TARGET) $(.OODATE)

