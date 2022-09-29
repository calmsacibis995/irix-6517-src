#!smake
# Copyright 1989, Silicon Graphics Inc., Mountain View, CA. 
#
# Makefile for the adaptec HIM layer library
#
#ident "$Revision: 1.4 $"

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/io/adpfc
.PATH:$(KPATH)

KCOPTS=-fullwarn
HIM_NAME=fcadp.a
HIM_CSRCS= \
	hwlip.c hwutil.c hwconfig.c hwintr.c hwtrans.c ulmutil.c
HIM_OBJS=${HIM_CSRCS:.c=.o}
$(HIM_NAME):$(HIM_NAME)($(HIM_OBJS)) MAKELIB

TARGETS=$(HIM_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=fcadp
CFILES=$(HIM_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

#
#
$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${TARGETS}
	${KDEVINSTALL} ${TARGETS}
