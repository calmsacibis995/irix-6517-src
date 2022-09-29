#!smake
# Copyright 1995, Silicon Graphics Inc., Mountain View, CA. 
#
# Makefile for Distributed Process Management
#

DEPTH?=../../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/os/proc/cell
.PATH:$(KPATH)

LCFLAGS+=-I$(TOP)/os/proc

PROC_NAME=os.a
PROC_IDLS=dsproc.idl dcproc.idl dspgrp.idl dcpgrp.idl \
        dssession.idl dcsession.idl dpid.idl
PROC_CSRCS=dsproc.c dcproc.c pid_mgmt_dp.c vproc_mgmt_dp.c \
        dspgrp.c dcpgrp.c pgrp_migrate.c vpgrp_dp.c \
	dssession.c dcsession.c vsession_dp.c dpid.c \
	vproc_migrate.c pproc_migrate.c \
	$(PROC_IDLS:S/^/I_/g:.idl=_stubs.c)

PROC_OBJS=${PROC_CSRCS:.c=.o}
$(PROC_NAME):$(PROC_NAME)($(PROC_OBJS)) MAKELIB

KERNIDLFILES=$(PROC_IDLS) 

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
