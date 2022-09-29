#!smake
# Copyright 1995, Silicon Graphics Inc., Mountain View, CA. 
#
# Makefile for Distributed Process Management
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/os/cell
.PATH:$(KPATH)

CELL_NAME=os.a
CELL_IDLS=thread.idl
CELL_CSRCS=kthread_cell.c \
	$(CELL_IDLS:S/^/I_/g:.idl=_stubs.c)

CELL_OBJS=${CELL_CSRCS:.c=.o}
$(CELL_NAME):$(CELL_NAME)($(CELL_OBJS)) MAKELIB

KERNIDLFILES=$(CELL_IDLS) 

TARGETS=$(CELL_NAME) 

.PRECIOUS:$(TARGETS)

LINTNAME=os
CFILES=$(CELL_CSRCS) 

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
