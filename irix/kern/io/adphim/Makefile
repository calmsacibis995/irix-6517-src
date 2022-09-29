#!smake
# Copyright 1989, Silicon Graphics Inc., Mountain View, CA. 
#
# Makefile for the adaptec HIM layer library
# This makefile makes both the "old" HIM for the 7880 and the
# new CHIM for 7895.  Actually, we are only going to use the
# "hardware management layer" of the new CHIM.  I'm going to
# call this hwm.
#
# The files for the old HIM are:
# him.c himd.c himdopt.c osm.c him_scb.h him_equ.h osm.h
# Everything else belongs to hwm.
#
# The hwm files have been taken out.
#
#ident "$Revision: 1.4 $"

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/io/adphim
.PATH:$(KPATH)

KCOPTS=	-fullwarn
KCDEFS= 
HIM_NAME=adphim.a
HIM_CSRCS=him.c himd.c himdopt.c osm.c
HIM_OBJS=${HIM_CSRCS:.c=.o}
$(HIM_NAME):$(HIM_NAME)($(HIM_OBJS)) MAKELIB

TARGETS=$(HIM_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=adphim
CFILES=$(HIM_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${TARGETS}
	${KDEVINSTALL} ${TARGETS}
