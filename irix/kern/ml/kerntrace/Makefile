#!smake
# Copyright 1992, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/ml/kerntrace
.PATH:$(KPATH)
KCINCS=-I$(BOOTAREA)
KASINCS=-I$(BOOTAREA)

ML_NAME=ml.a
ML_CSRCS=
ML_ASRCS=kerntrace.s
ML_OBJS=${ML_CSRCS:.c=.o} ${ML_ASRCS:.s=.o}

$(ML_NAME):$(ML_NAME)($(ML_OBJS)) MAKELIB

TARGETS=$(ML_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=kerntrace
CFILES=$(ML_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

#
# 
# Since the kerntrace stuff goes into the ml lib - there is no reason to
# install it here
#
$(KCOMMONPREF)install: $(KCOMMONPREF)default
