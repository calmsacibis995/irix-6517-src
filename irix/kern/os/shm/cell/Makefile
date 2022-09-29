#!smake
# Copyright 1995, Silicon Graphics Inc., Mountain View, CA. 
#
# Makefile for System V shared memory
#

DEPTH?=../../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/os/shm/cell
.PATH:$(KPATH)

LCFLAGS+=-I$(TOP)/os/shm

SHM_NAME=os.a
SHM_IDLS=dsshm.idl dcshm.idl
KERNIDLFILES=$(SHM_IDLS)
SHM_CSRCS=dsshm.c dcshm.c vshm_mgr_dp.c $(SHM_IDLS:S/^/I_/g:.idl=_stubs.c) \
	  vshm_migrate.c
SHM_OBJS=${SHM_CSRCS:.c=.o}

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
