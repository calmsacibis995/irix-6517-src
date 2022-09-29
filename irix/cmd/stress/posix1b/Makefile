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
# $Revision: 1.12 $

BASEVERSION=32bit
ALTVERSIONS=64bit n32bit
WANTPARALLEL=yes-please

include $(ROOT)/usr/include/make/cmdcommondefs

PTHREAD_TESTS=  psched_pthread mqtest_pthread sem_ptbench

RUNSCRIPTS=	runtests runmqtests runsemtests runtimetests \
		runschedtests

COMMANDS=	mqtest mqtest_sproc mq_bench ptimer \
		ptimer_rtsched ptimer_absclock sem_bench sem_sbench sem_prio \
		sem_speed sem_pbench sem_destroy sched_bench  \
		sem_abi mlocker mlockstress sem_procon sem_fork \
		sem_create_destroy psched \
		mq_pro mq_con mq_view nano_sig \
		sem_fieldtest sem_fieldtest2 \
		$(RUNSCRIPTS) \
		$(PTHREAD_TESTS)

OPTIMIZER=
LCOPTS+=-g -fullwarn
#
# 85 - stop whining about libmalloc preempting libc malloc
# 84 - stop whining about not using anything from libstress.a
LLDOPTS=-Wl,-woff,85 -Wl,-woff,84

default: $(TARGETS)

include $(CMDCOMMONRULES)

mqtest_pthread: mqtest_pthread.c
	$(CCF) -o mqtest_pthread mqtest_pthread.c $(LDFLAGS) -lpthread

sem_ptbench: sem_ptbench.c
	$(CCF) -o sem_ptbench sem_ptbench.c $(LDFLAGS) -lpthread

psched_pthread: psched_pthread.c
	$(CCF) -o psched_pthread psched_pthread.c $(LDFLAGS) -lpthread

32bitinstall: default
	$(INSTALL) -idb "nostrip" -F /usr/stress/posix1b $(COMMANDS)

64bitinstall: default
	$(INSTALL) -idb "nostrip" $(IDB_TAG64) -F /usr/stress/posix1b/64bit $(COMMANDS)

n32bitinstall: default
	$(INSTALL) -idb "nostrip" -F /usr/stress/posix1b/n32 $(COMMANDS)
