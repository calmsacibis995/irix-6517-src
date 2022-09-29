#!smake
# Copyright 1996, Silicon Graphics Inc., Mountain View, CA. 
#
# Makefile for datapipe related files
#


DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/io/datapipe
.PATH:$(KPATH)

DATAPIPE_NAME=datapipe.a
DATAPIPE_CSRCS=fspe.c
DATAPIPE_OBJS=${DATAPIPE_CSRCS:.c=.o}
$(DATAPIPE_NAME):$(DATAPIPE_NAME)($(DATAPIPE_OBJS)) MAKELIB

#
# Pipe driver
#
DPIPE_NAME=dpipe.o
DPIPE_CSCRS=dpipe.c


TARGETS=$(DATAPIPE_NAME) $(FSPE_NAME) $(DPIPE_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=DATAPIPE
CFILES=$(DATAPIPE_CSRCS)


#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} $(DATAPIPE_NAME) $(FSPE_NAME) $(DPIPE_NAME)
	${KDEVINSTALL} ${TARGETS}
