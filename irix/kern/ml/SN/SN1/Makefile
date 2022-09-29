#!smake
# Copyright 1996, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/ml/SN/SN1
.PATH:$(KPATH)
KCINCS=-I$(BOOTAREA)
KASINCS=-I$(BOOTAREA)

ML_NAME=ml.a
ML_CSRCS=  cachesupport.c IP33.c
#memerror.c    slots.c tlbsupport.c 	


ML_ASRCS= SN1asm.s
#SN0asm.s slave.s

ML_OBJS=${ML_CSRCS:.c=.o} ${ML_ASRCS:.s=.o}

$(ML_NAME):$(ML_NAME)($(ML_OBJS)) MAKELIB

TARGETS=$(ML_NAME)
.PRECIOUS:$(TARGETS)
LINTNAME=sn1
CFILES=$(ML_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

#
# 
# Since the SN0 stuff goes into the ml lib - there is no reason to install
# it here
#
$(KCOMMONPREF)install: $(KCOMMONPREF)default















