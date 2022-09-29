#!smake
# Copyright 1995, Silicon Graphics Inc., Mountain View, CA. 
#
# Makefile for System V shared memory
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/os/shm
.PATH:$(KPATH)

SHM_NAME=os.a
SHM_CSRCS=vshm_mgr.c pshm.c
SHM_OBJS=${SHM_CSRCS:.c=.o}

#if defined(CELL_IRIX)
SUBDIRS=cell
#else
SUBDIRS=
SHM_CSRCS+=vshm_mgr_lp.c
#endif

$(SHM_NAME):$(SHM_NAME)($(SHM_OBJS)) MAKELIB

TARGETS=$(SHM_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=shm
CFILES=$(SHM_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
