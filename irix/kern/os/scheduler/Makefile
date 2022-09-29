#!smake
# Copyright 1996, Silicon Graphics Inc., Mountain View, CA. 
#
# Makefile for the scheduler os subsystem.
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/os/scheduler
.PATH:$(KPATH)

#if defined(CPUBOARD) && ($(CPUBOARD) == "IP19" || $(CPUBOARD) == "IP21" || \
	$(CPUBOARD) == "IP25" || $(CPUBOARD) == "IP27" || \
	$(CPUBOARD) == "IP30")
SUBDIRS=frame
#endif

DISP_IDBG_NAME=disp_idbg.a
DISP_IDBG_CSRCS=disp_idbg.c
DISP_IDBG_OBJS=${DISP_IDBG_CSRCS:.c=.o}
$(DISP_IDBG_NAME):$(DISP_IDBG_NAME)($(DISP_IDBG_OBJS)) MAKELIB

DISP_NAME=disp.a
DISP_CSRCS=gang.c runq.c utility.c psched.c \
	wtree.c job.c q.c batch.c cpu.c miser.c rt.c cpuset.c
DISP_OBJS=${DISP_CSRCS:.c=.o}
$(DISP_NAME):$(DISP_NAME)($(DISP_OBJS)) MAKELIB

TARGETS=$(DISP_NAME) $(DISP_IDBG_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=scheduler
CFILES=$(DISP_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${TARGETS}
	${KDEVINSTALL} ${TARGETS}

headers!
	${INSTALL} -m 444 -F /usr/include/sys miser_public.h space.h \
		batch.h q.h rt.h wtree.h sched.h

disp_idbg.o: disp_idbg.c
	$(CCF) -c $(JALR) -G 0 $*.c
