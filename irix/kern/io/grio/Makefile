#!smake
# Copyright 1993, Silicon Graphics Inc., Mountain View, CA. 
#
# Makefile for the XLV driver core
#
#ident "$Header: /proj/irix6.5.7m/isms/irix/kern/io/grio/RCS/Makefile,v 1.5 1995/10/24 07:44:52 ack Exp $"

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/io/grio
.PATH:$(KPATH)

KCOPTS=	-fullwarn $(XFSCOPTS)

GRIO_NAME=grio.a
GRIO_CSRCS= grio.c grio_comm.c grio_sub.c grio_vod.c grio_monitor.c
GRIO_OBJS=${GRIO_CSRCS:.c=.o}
$(GRIO_NAME):$(GRIO_NAME)($(GRIO_OBJS)) MAKELIB

GRIOIDBG_NAME=grioidbg.o
GRIOIDBG_CSRCS=grioidbg.c

TARGETS=$(GRIO_NAME) $(GRIOIDBG_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=grio
CFILES=$(GRIO_CSRCS) $(GRIOIDBG_CSRCS)
#HFILES=

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${GRIO_NAME}
	${KDEVINSTALL} ${GRIO_NAME}
	${KINSTALL} ${GRIOIDBG_NAME}
	${KDEVINSTALL} ${GRIOIDBG_NAME}

grioidbg.o: grioidbg.c
	$(CCF) -c $(JALR) -G 0 $(KPATH)/grioidbg.c && \
	$(LDF) $(PRODOBJECT) -G 0 -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)
