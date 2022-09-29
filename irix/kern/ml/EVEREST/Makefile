#!smake
# Copyright 1992, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/ml/EVEREST
.PATH:$(KPATH)
KCINCS=-I$(BOOTAREA)
KASINCS=-I$(BOOTAREA)

# Compile ksync sources multiple times to generate packages implementing
# ksync objects with statistics.
#
TLINKDIR_MAKERULE= \
        mkdir -p statistics; \
        echo "\tcd statistics; $(MAKE) $${RULE:=$@}"; \
        cd statistics; \
        ln -sf ../chardlocks.c ./chardlocks.c; \
        ln -sf ../llsclocks.s ./llsclocks.s; \
        ln -sf ../Makefile.statistics ./Makefile; \
        ${MAKE} $${RULE:=$@}; \
        cd ..;

KDIRT+=$(KPATH)/statistics


ML_NAME=ml.a
ML_CSRCS=pio.c evdma.c epc.c evarcs.c evclock.c everror.c evintr.c \
	evmp.c fchip.c init.c io4.c led.c mc3.c memsupport.c nvram.c \
	evprobe.c stubs.c vmecc.c fcg.c s1chip.c hipinit.c dang.c groupintr.c  \
	evgraph.c
ML_ASRCS=EVERESTasm.s evspl.s evslave.s
#if CELL && !UNIKERNEL
ML_CSRCS+=partition.c evxpc.c
#endif
ML_OBJS=${ML_CSRCS:.c=.o} ${ML_ASRCS:.s=.o}

HL_NAME=hardlocks.a
HL_CSRCS=
HL_ASRCS=llsclocks.s
HL_OBJS=${HL_CSRCS:.c=.o} ${HL_ASRCS:.s=.o}

$(ML_NAME):$(ML_NAME)($(ML_OBJS)) MAKELIB
$(HL_NAME):$(HL_NAME)($(HL_OBJS)) MAKELIB

TARGETS=$(ML_NAME) $(HL_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=everest
CFILES=$(ML_CSRCS) $(HL_CSRCS)

SUBDIRS=FRU

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

#
# 
# Since the EVEREST stuff goes into the ml lib - there is no reason to install
# it here
#
$(KCOMMONPREF)install: $(KCOMMONPREF)default
