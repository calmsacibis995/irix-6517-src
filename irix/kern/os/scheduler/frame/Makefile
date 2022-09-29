#!smake
# Copyright 1996, Silicon Graphics Inc., Mountain View, CA. 
#
# Makefile for the frame scheduler subsystem.
#

DEPTH?=../../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/os/scheduler/frame
.PATH:$(KPATH)

KCOPTS+=-fullwarn
LCFLAGS+=-I$(TOP)/os/scheduler

FRS_NAME=frs.a
FRS_CSRCS=frame_barrier.c frame_semabarrier.c frame_cpu.c \
       	  frame_process.c frame_procd.c frame_minor.c \
	  frame_major.c frame_sched.c frame_xface.c frame_debug.c 
FRS_OBJS=${FRS_CSRCS:.c=.o}

$(FRS_NAME):$(FRS_NAME)($(FRS_OBJS)) MAKELIB

TARGETS=$(FRS_NAME)
.PRECIOUS:$(TARGETS)
LINTNAME=frs
CFILES=$(FRS_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules


$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	$(KINSTALL) $(LBOOTPOSTOP) -idb react.sw.frs $(FRS_NAME)
