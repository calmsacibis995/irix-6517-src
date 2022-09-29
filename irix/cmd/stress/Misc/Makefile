#!smake
#**************************************************************************
#*									  *
#* 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
#*									  *
#*  These coded instructions, statements, and computer programs  contain  *
#*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
#*  are protected by Federal copyright law.  They  may  not be disclosed  *
#*  to  third  parties  or copied or duplicated in any form, in whole or  *
#*  in part, without the prior written consent of Silicon Graphics, Inc.  *
#*									  *
#**************************************************************************/
# $Revision: 1.42 $

BASEVERSION=32bit
ALTVERSIONS=64bit n32bit

include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=libchk achk mal amal libmal remal aremal libremal \
	 malspeed amalspeed libmalspeed mtest libmtest runtests \
	 librecalloc arecalloc amemalign libmemalign memalign \
	 re libre trunc \
	 dirchk fchk fcntl fetchop flock freectty gettime \
	 lastclose mpin notty orphanpgrp \
	 rt sigtstp sigtstp2 slinkchk \
	 procpoll lockstep snoopw startstop watch watchread flimit \
	 fds strfds mandflock mandflock_child vmlimit flimitquark procwrite \
	 testclocks ptwatch sigwatch

.NOTPARALLEL:
LCDEFS+=-D_SGI_MP_SOURCE
OPTIMIZER=
LCOPTS+=-g -fullwarn
#
# 85 - stop whining about libmalloc preempting libc malloc
# 84 - stop whining about not using anything from libstress.a
LLDOPTS=-Wl,-woff,85 -Wl,-woff,84 -Wl,-woff,134

default: $(TARGETS)

include $(CMDCOMMONRULES)

mtest:mtest.c
	$(CCF) -o mtest mtest.c $(LDFLAGS)

libmtest:mtest.c
	$(CCF) -DLIBMALLOC -o libmtest mtest.c $(LDFLAGS) -lmalloc

amal:mal.c
	$(CCF) -DAMALLOC -o amal mal.c $(LDFLAGS)

libmal:mal.c
	$(CCF) -DLIBMALLOC -o libmal mal.c $(LDFLAGS) -lmalloc

libchk:chk.c
	$(CCF) -o libchk chk.c $(LDFLAGS) -lmalloc

libre:re.c
	$(CCF) -DLIBMALLOC -o libre re.c $(LDFLAGS) -lmalloc

libremal:remal.c
	$(CCF) -o libremal remal.c $(LDFLAGS) -lmalloc

amalspeed:malspeed.c
	$(CCF) -DAMALLOC -o amalspeed malspeed.c $(LDFLAGS)

libmalspeed:malspeed.c
	$(CCF) -DLIBMALLOC -o libmalspeed malspeed.c $(LDFLAGS) -lmalloc

arecalloc:recalloc.c
	$(CCF) -o arecalloc recalloc.c $(LDFLAGS) -lm

fetchop:      fetchop.c
#if $(OBJECT_STYLE) == "32"
	$(CCF) -o $@ fetchop.c $(LDFLAGS)
#else 
	$(CCF) -o $@ fetchop.c $(LDFLAGS) -lfetchop
#endif

amemalign:memalign.c
	$(CCF) -DAMALLOC -o amemalign memalign.c $(LDFLAGS) -lm

libmemalign:memalign.c
	$(CCF) -DLIBMALLOC -o libmemalign memalign.c $(LDFLAGS) -lm -lmalloc

librecalloc:recalloc.c
	$(CCF) -DLIBMALLOC -o librecalloc recalloc.c $(LDFLAGS) -lm -lmalloc

flimitquark:flimit.c
	$(CCF) -o flimitquark flimit.c $(LDFLAGS) -Wl,-elspec,elspec

ptwatch:ptwatch.c
	$(CCF) -DPTHREAD -o ptwatch ptwatch.c $(LDFLAGS) -lpthread

sigwatch:watch.c
	$(CCF) -o sigwatch watch.c $(LDFLAGS) -lpthread

32bitinstall: default
	$(INSTALL) -idb "nostrip" -F /usr/stress/Misc $(COMMANDS)
	$(INSTALL) -idb "nostrip" -F /usr/diags/crash/Misc $(COMMANDS)

64bitinstall: default
	$(INSTALL) -idb "nostrip" $(IDB_TAG64) -F /usr/stress/Misc/64bit $(COMMANDS)
	$(INSTALL) -idb "nostrip" $(IDB_TAG64) -F /usr/diags/crash/Misc/64bit $(COMMANDS)

n32bitinstall: default
	$(INSTALL) -idb "nostrip" -F /usr/stress/Misc/n32 $(COMMANDS)
	$(INSTALL) -idb "nostrip" -F /usr/diags/crash/Misc/n32 $(COMMANDS)
