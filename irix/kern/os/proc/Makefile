#!smake
# Copyright 1995, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/os/proc
.PATH:$(KPATH)

PROC_NAME=os.a
PROC_CSRCS= childlist.c pid.c ppgrp_mgmt.c ppgrp.c pproc_exec.c \
	    pproc_exit.c proc_ktops.c pproc_mgmt.c pproc_misc.c \
	    pproc_resource.c pproc_thread.c \
	    pproc_sig.c pproc_scalls.c pproc_psched.c pproc_uthread.c \
	    proc_util.c sproc_util.c vpgrp.c vproc.c \
	    vsession.c vsession_misc.c psession.c psession_mgmt.c \
	    pproc_ckpt.c

#if defined(CELL_IRIX)
SUBDIRS=cell
#else
SUBDIRS=
PROC_CSRCS+=vpgrp_lp.c vproc_mgmt_lp.c pid_mgmt_lp.c vsession_lp.c
#endif

PROC_OBJS=${PROC_CSRCS:.c=.o}
$(PROC_NAME):$(PROC_NAME)($(PROC_OBJS)) MAKELIB

PROCIDBG_NAME=procidbg.a
PROCIDBG_CSRCS=procidbg.c
PROCIDBG_OBJS=${PROCIDBG_CSRCS:.c=.o}
$(PROCIDBG_NAME):$(PROCIDBG_NAME)($(PROCIDBG_OBJS)) MAKELIB

PGRPIDBG_NAME=pgrpidbg.a
PGRPIDBG_CSRCS=pgrpidbg.c
PGRPIDBG_OBJS=${PGRPIDBG_CSRCS:.c=.o}
$(PGRPIDBG_NAME):$(PGRPIDBG_NAME)($(PGRPIDBG_OBJS)) MAKELIB

SESSIONIDBG_NAME=sessionidbg.a
SESSIONIDBG_CSRCS=sessionidbg.c
SESSIONIDBG_OBJS=${SESSIONIDBG_CSRCS:.c=.o}
$(SESSIONIDBG_NAME):$(SESSIONIDBG_NAME)($(SESSIONIDBG_OBJS)) MAKELIB

ITARGETS=$(PROCIDBG_NAME) $(PGRPIDBG_NAME) $(SESSIONIDBG_NAME)
TARGETS=$(PROC_NAME) $(PROCIDBG_NAME) $(PGRPIDBG_NAME) $(SESSIONIDBG_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=os
CFILES=$(PROC_CSRCS) $(PROCIDBG_CSRCS) $(PGRPIDBG_CSRCS) $(SESSIONIDBG_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${ITARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	$(KINSTALL) $(ITARGETS)
	$(KDEVINSTALL) $(ITARGETS)

procidbg.o: procidbg.c
	$(CCF) -c $(JALR) -G 0 $(KPATH)/procidbg.c

pgrpidbg.o: pgrpidbg.c
	$(CCF) -c $(JALR) -G 0 $(KPATH)/pgrpidbg.c

sessionidbg.o: sessionidbg.c
	$(CCF) -c $(JALR) -G 0 $(KPATH)/sessionidbg.c
