#!smake
# Copyright 1993, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/ml/TFP
.PATH:$(KPATH)
KCINCS=-I$(BOOTAREA)
KASINCS=-I$(BOOTAREA)

ML_NAME=ml.a
ML_CSRCS=tfpmemsupport.c tfpimgr.c
ML_ASRCS=tfpcacheops.s tfptlb.s tfpbcopy.s
ML_OBJS=${ML_CSRCS:.c=.o} ${ML_ASRCS:.s=.o}

$(ML_NAME):$(ML_NAME)($(ML_OBJS)) MAKELIB

TARGETS=$(ML_NAME)
.PRECIOUS:$(TARGETS)

# LINTNAMES should be unique in a directory for btool to work
LINTNAME=tfp
CFILES=$(ML_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

#
# 
# Since the TFP stuff goes into the ml lib - there is no reason to install
# it here
#
$(KCOMMONPREF)install: $(KCOMMONPREF)default
