#!smake
# Copyright 1995, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/os/pagg
.PATH:$(KPATH)

PAGG_NAME=os.a
PAGG_CSRCS=ppag.c vpagg.c
PAGG_OBJS=${PAGG_CSRCS:.c=.o}
$(PAGG_NAME):$(PAGG_NAME)($(PAGG_OBJS)) MAKELIB

TARGETS=$(PAGG_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=os
CFILES=$(PAGG_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${TARGETS}
	${KDEVINSTALL} ${TARGETS}
