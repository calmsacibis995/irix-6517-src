#!smake
#**************************************************************************
#*									  *
#* 		 Copyright (C) 1986-1992 Silicon Graphics, Inc.		  *
#*									  *
#*  These coded instructions, statements, and computer programs  contain  *
#*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
#*  are protected by Federal copyright law.  They  may  not be disclosed  *
#*  to  third  parties  or copied or duplicated in any form, in whole or  *
#*  in part, without the prior written consent of Silicon Graphics, Inc.  *
#*									  *
#**************************************************************************/

# $Revision: 1.53 $

BASEVERSION=32bit
ALTVERSIONS=64bit n32bit
WANTPARALLEL=yes-please
#
# we don't want our little .so to update the so_locations file
#
NO_STARTOPT=

include $(ROOT)/usr/include/make/cmdcommondefs

TESTS=matmul msieve shcreate shfile shmap shpin shpipe shprda thring execit \
         sexec pcreate shdma alloc spgrp pi diners hardtest sfork userr \
         m_test usema saddr lockspeed csettest semaspeed lockstress \
         usemapoll usemaalloc usemaintr usemaclose attach semstdio \
         semlocale abilock sprocsp cas fortpi runtests \
         doshuid mfgtests usmalloc sfds shclose sfork2 userrf settest spfirst \
	 slock_mode usemarecurse_pt locktest_pt lockstress_pt usemapoll_pt

SCFILES=shuid.c
STESTS=$(SCFILES:.c=)
COMMANDS=$(TESTS) $(STESTS)
COMMANDS+=lonfs.so

LLDLIBS+=-lm
LCDEFS+=-D_SGI_MP_SOURCE
# -g is really useful for debugging ... (and -g3 doesn't help much...)
LCOPTS+=-g -fullwarn
OPTIMIZER=
#
# 84 - stop whining about not using anything from libstress.a
LLDOPTS=-Wl,-woff,84
WOFF=

LDIRT=so_locations lonfs.so

# WARNING - this isn't really the default rule..
default: $(TARGETS)

include $(CMDCOMMONRULES)

shcreate:shcreate.c
	$(CCF) -ignore_unresolved shcreate.c -o shcreate $(LDFLAGS)

usemarecurse_pt:usemarecurse_pt.c
	$(CCF) -o usemarecurse_pt usemarecurse_pt.c $(LDFLAGS) -lpthread

locktest_pt:locktest_pt.c
	$(CCF) -o locktest_pt locktest_pt.c $(LDFLAGS) -lpthread

lockstress_pt:lockstress_pt.c
	$(CCF) -o lockstress_pt lockstress_pt.c $(LDFLAGS) -lpthread

usemapoll_pt:usemapoll_pt.c
	$(CCF) -o usemapoll_pt usemapoll_pt.c $(LDFLAGS) -lpthread

lonfs.so:lonfs.o
	$(LD) $(GLDOPTS) $(LDDSOOPTS) lonfs.o -o lonfs.so

32bitinstall: default
	$(INSTALL) -idb "nostrip" -F /usr/stress/Sproc $(TESTS) lonfs.so
	-$(INSTALL) -idb "nostrip" -F /usr/stress/Sproc -u lp -g lp -m 6755 \
		    $(STESTS)
	$(INSTALL) -idb "nostrip" -F /usr/diags/crash/Sproc $(TESTS) lonfs.so
	-$(INSTALL) -idb "nostrip" -F /usr/diags/crash/Sproc -u lp -g lp -m \
		    6755 $(STESTS)

64bitinstall: default
	$(INSTALL) -idb "nostrip" -F /usr/stress/Sproc/64bit $(IDB_TAG64) $(TESTS) lonfs.so
	-$(INSTALL) -idb "nostrip" -F /usr/stress/Sproc/64bit $(IDB_TAG64) \
		-u lp -g lp -m 6755 $(STESTS)
	$(INSTALL) -idb "nostrip" -F /usr/diags/crash/Sproc/64bit $(IDB_TAG64) \
			$(TESTS) lonfs.so
	-$(INSTALL) -idb "nostrip" -F /usr/diags/crash/Sproc/64bit $(IDB_TAG64)\
		-u lp -g lp -m 6755 $(STESTS)

n32bitinstall: default
	$(INSTALL) -idb "nostrip" -F /usr/stress/Sproc/n32 $(IDB_TAG64) \
		$(TESTS) lonfs.so
	-$(INSTALL) -idb "nostrip" -F /usr/stress/Sproc/n32 $(IDB_TAG64)\
		-u lp -g lp -m 6755 $(STESTS)
	$(INSTALL) -idb "nostrip" -F /usr/diags/crash/Sproc/n32 $(IDB_TAG64) \
		$(TESTS) lonfs.so
	-$(INSTALL) -idb "nostrip" -F /usr/diags/crash/Sproc/n32 $(IDB_TAG64) \
		-u lp -g lp -m 6755 $(STESTS)
