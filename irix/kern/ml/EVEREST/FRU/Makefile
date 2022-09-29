#!smake
# Copyright 1992, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/ml/EVEREST/FRU
.PATH:$(KPATH)
KCINCS=-I$(BOOTAREA)
KASINCS=-I$(BOOTAREA)

FRU_NAME=fru.a
FRU_CSRCS= fru_decode.c fru_io4.c fru_main.c \
	fru_matcher.c fru_mc3.c fru_pattern.c kern_fru.c
FRU_ASRCS=
FRU_OBJS=${FRU_CSRCS:.c=.o} ${FRU_ASRCS:.s=.o}

#if $(CPUBOARD) == "IP19"
MACHDEP_CSRCS=fru_ip19.c
MACHDEP_ASRCS=
#endif

#if $(CPUBOARD) == "IP21"
MACHDEP_CSRCS=fru_ip21.c
MACHDEP_ASRCS=
#endif

#if $(CPUBOARD) == "IP25"
MACHDEP_CSRCS=fru_ip25.c
MACHDEP_ASRCS=
#endif

MACHDEP_OBJS=${MACHDEP_CSRCS:.c=.o} ${MACHDEP_ASRCS:.s=.o}

$(FRU_NAME):$(FRU_NAME)($(FRU_OBJS) $(MACHDEP_OBJS)) MAKELIB

TARGETS=$(FRU_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=fru
CFILES=$(FRU_CSRCS) $(MACHDEP_CSRCS)

LCDEFS+=-DVERSION='"K1.01"'

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

#
# 
#
$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} -idb eoe.sw.unix ${FRU_NAME}

