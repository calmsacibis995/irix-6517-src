#!smake
# Copyright 1989, Silicon Graphics Inc., Mountain View, CA.
#
#
# Makefile for machine-level kernel sources.
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/ml/BEAST
.PATH:$(KPATH)
KCINCS=-I${BOOTAREA}
KASINCS=-I${BOOTAREA}

ML_NAME=ml.a
ML_CSRCS=exception.c
ML_ASRCS=cacheops.s exc_vectors.s beastasm.s tlb.s
ML_OBJS=${ML_CSRCS:.c=.o} ${ML_ASRCS:.s=.o}
$(ML_NAME):$(ML_NAME)($(ML_OBJS)) MAKELIB

TARGETS=$(ML_NAME)
.PRECIOUS:$(TARGETS)

#
# lint
# For lint just use dhardlocks since it has the most info
#
CFILES=$(ML_CSRCS)
LINTNAME=beast

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
