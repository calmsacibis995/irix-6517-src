#!smake
# Copyright 1996, Silicon Graphics Inc., Mountain View, CA. 
#
# Makefile for Distributed Host Management
#

DEPTH?=../../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/os/host/cell
.PATH:$(KPATH)

LCFLAGS+=-I$(TOP)/os/host

HOST_NAME=os.a
HOST_IDLS=dshost.idl dchost.idl
HOST_CSRCS=dshost.c dchost.c vhost_dp.c $(HOST_IDLS:S/^/I_/g:.idl=_stubs.c)

HOST_OBJS=${HOST_CSRCS:.c=.o}
$(HOST_NAME):$(HOST_NAME)($(HOST_OBJS)) MAKELIB

KERNIDLFILES=$(HOST_IDLS)

TARGETS=$(HOST_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=os
CFILES=$(HOST_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
