#!smake
# Copyright 1992, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/ml/MOOSEHEAD
.PATH:$(KPATH)
KCINCS=-I$(BOOTAREA)
KASINCS=-I$(BOOTAREA)

ML_NAME=ml.a
#ML_CSRCS=IP32cache.c IP32clock.c IP32dma.c IP32err.c IP32hinv.c IP32intr.c \
#	 IP32init.c IP32misc.c IP32nvram.c
#if $(CPUARCH) == "R10000"
ML_CSRCS=IP32intr.c IP32init.c IP32pio.c IP32cache.c IP32clock.c \
	 IP32flash.c IP32misc.c IP32err.c IP32stubs.c \
	 hw_copy.c mte_copy.c vice_copy.c mte_zero.c
#else
ML_CSRCS=IP32intr.c IP32init.c IP32pio.c IP32cache.c IP32clock.c \
	 IP32flash.c IP32misc.c IP32err.c IP32stubs.c \
	 hw_copy.c mte_copy.c vice_copy.c mte_zero.c
#endif
ML_ASRCS=IP32asm.s
#ML_ASRCS=
ML_OBJS=${ML_CSRCS:.c=.o} ${ML_ASRCS:.s=.o}

$(ML_NAME):$(ML_NAME)($(ML_OBJS)) MAKELIB

TARGETS=$(ML_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=moosehead
CFILES=$(ML_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

#
# 
# Since the MOOSEHEAD stuff goes into the ml lib - there is no reason to 
# install it here
#
$(KCOMMONPREF)install: $(KCOMMONPREF)default

# init.o: $(.PREFIX).c
# 	$(CCF) -c $*.c && \
# 	$(LDF) -x -r $(.TARGET:T) -o $$$$.o && \
# 	mv $$$$.o $(.TARGET:T)
