#!smake
# Copyright 1996, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/ml/SN/SN0
.PATH:$(KPATH)
KCINCS=-I$(BOOTAREA)
KASINCS=-I$(BOOTAREA)

ML_NAME=ml.a
ML_CSRCS= bte.c  cachesupport.c  hubcounters.c	hubdev.c  huberror.c	\
	hubio.c ip27log.c led.c  memerror.c    slots.c 	\
	sysctlr.c   sn0net.c tlbsupport.c IP27.c


ML_ASRCS= SN0asm.s

ML_OBJS=${ML_CSRCS:.c=.o} ${ML_ASRCS:.s=.o}

SN0DRV_NAME=sn0drv.o
SN0DRV_CSRCS=sn0drv.c
SN0SRV_OBJS=${SN0DRV_CSRCS:.c=.o}

$(ML_NAME):$(ML_NAME)($(ML_OBJS)) MAKELIB

TARGETS=$(ML_NAME) $(SN0DRV_NAME)
.PRECIOUS:$(TARGETS)
SUBDIRS=FRU
LINTNAME=sn0
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
	${KDEVINSTALL} ${SN0DRV_NAME}
	${KINSTALL} ${SN0DRV_NAME}















