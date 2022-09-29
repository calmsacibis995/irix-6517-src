#!smake
# Copyright 1995, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/os/as
.PATH:$(KPATH)

AS_NAME=os.a
AS_CSRCS=as_icrash.c as_mgr.c ddmap.c fault.c pas.c pas_subr.c pas_watch.c \
	pas_faultsubr.c pas_lock.c pas_addspace.c pas_grow.c \
	pas_delspace.c pas_mmapsubr.c pas_prsubr.c pas_new.c vm_pool.c \
	pmap.c region.c
AS_OBJS=${AS_CSRCS:.c=.o}
$(AS_NAME):$(AS_NAME)($(AS_OBJS)) MAKELIB

AS_IDBG_NAME=asidbg.a
AS_IDBG_CSRCS=asidbg.c
AS_IDBG_OBJS=${AS_IDBG_CSRCS:.c=.o}
$(AS_IDBG_NAME):$(AS_IDBG_NAME)($(AS_IDBG_OBJS)) MAKELIB

ITARGETS=$(AS_IDBG_NAME)
TARGETS=$(AS_NAME) $(AS_IDBG_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=os
CFILES=$(AS_CSRCS) $(AS_IDBG_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${ITARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${ITARGETS}
	${KDEVINSTALL} ${ITARGETS}

asidbg.o: asidbg.c
	$(CCF) -c $(JALR) -G 0 $(KPATH)/asidbg.c

as_icrash.o: as_icrash.c
	$(CCF) -g3 -c $(JALR) $(KPATH)/as_icrash.c && \
	$(LDF) $(PRODOBJECT) -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)


