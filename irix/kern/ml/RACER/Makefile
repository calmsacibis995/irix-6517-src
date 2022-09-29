#!smake
# Copyright 1992, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/ml/RACER
.PATH:$(KPATH)
KCINCS=-I$(BOOTAREA)
KASINCS=-I$(BOOTAREA)

ML_NAME=ml.a
ML_CSRCS=IP30.c \
	tod.c hwtimer.c heart_err.c heart_mem.c heart_ecc.c \
	iomap.c nvram.c power.c
OTHERS=heart_bell.c xbow.c
ML_ASRCS=IP30asm.s slave.s
ML_OBJS=${ML_CSRCS:.c=.o} ${ML_ASRCS:.s=.o}


$(ML_NAME):$(ML_NAME)($(ML_OBJS)) MAKELIB

TARGETS=$(ML_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=racer
CFILES=$(ML_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

#
# 
# Since the RACER stuff goes into the ml lib - there is no reason to install
# it here
#
$(KCOMMONPREF)install: $(KCOMMONPREF)default
