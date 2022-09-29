#!smake
#**************************************************************************
#*									  *
#* 		 Copyright (C) 1989 Silicon Graphics, Inc.		  *
#*									  *
#*  These coded instructions, statements, and computer programs  contain  *
#*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
#*  are protected by Federal copyright law.  They  may  not be disclosed  *
#*  to  third  parties  or copied or duplicated in any form, in whole or  *
#*  in part, without the prior written consent of Silicon Graphics, Inc.  *
#*									  *
#**************************************************************************/
# $Revision: 1.27 $

BASEVERSION=32bit
ALTVERSIONS=64bit n32bit
WANTPARALLEL=yes-please

include $(ROOT)/usr/include/make/cmdcommondefs

CFILES = f.c msclose.c msync.c remap.c mmem.c m.c zero.c nfds.c \
	ag.c agfull.c agpriv.c agrand.c pfork.c sfork.c \
	addrattach.c noag.c madvise.c mstat.c mprotect.c \
	sprocsync.c agsync.c agtrunc.c aglock.c shadows.c mmap_ftrunc.c \
	mapall.c zeroex.c setex.c elfmap.c mapdev.c
SHFILES= runtests.sh mfgtests.sh
COMMANDS= $(CFILES:.c=) $(SHFILES:.sh=)

OPTIMIZER=
LCOPTS+=-g -fullwarn
LCDEFS+=-D_SGI_MP_SOURCE
#
# 84 - stop whining about not using anything from libstress.a
LLDOPTS=-Wl,-woff,84

default: $(TARGETS)

include $(CMDCOMMONRULES)

32bitinstall: default
	$(INSTALL) -idb "nostrip" -F /usr/stress/Mmap $(COMMANDS)
	$(INSTALL) -idb "nostrip" -F /usr/diags/crash/Mmap $(COMMANDS)

64bitinstall: default
	$(INSTALL) -idb "nostrip" -F /usr/stress/Mmap/64bit $(IDB_TAG64) $(COMMANDS)
	$(INSTALL) -idb "nostrip" -F /usr/diags/crash/Mmap/64bit $(IDB_TAG64) $(COMMANDS)

n32bitinstall: default
	$(INSTALL) -idb "nostrip" -F /usr/stress/Mmap/n32 $(COMMANDS)
	$(INSTALL) -idb "nostrip" -F /usr/diags/crash/Mmap/n32 $(COMMANDS)

zeroex:setex zeroex.c
	$(CCF) -o zeroex zeroex.c $(LDFLAGS)
	./setex -v -f zeroex

setex:setex.c
	ROOT=/ cc -fullwarn -n32 -mips3 setex.c -o setex -L -L$(ROOT)/usr/lib32 -lelf 
