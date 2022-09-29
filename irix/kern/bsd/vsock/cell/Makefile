#!smake
# Copyright 1995, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/bsd/vsock/cell
.PATH:$(KPATH)

PROC_NAME=bsd.a
PROC_CSRCS= 

PROC_IDLS=dcvsock.idl  dsvsock.idl
KERNIDLFILES=$(PROC_IDLS)
PROC_CSRCS+=dcvsock.c dsvsock.c vsock_mgr.c vsock_migrate.c vsock_server.c $(PROC_IDLS:S/^/I_/g.idl=_stubs.c)

PROC_OBJS=${PROC_CSRCS:.c=.o}
$(PROC_NAME):$(PROC_NAME)($(PROC_OBJS)) MAKELIB

TARGETS=$(PROC_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=os
CFILES=$(PROC_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
