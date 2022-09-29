#!smake
# Copyright 1995-2000 Silicon Graphics Inc.
#
# Makefile for the VMEbus module
#
#ident "$Revision: 1.6 $"

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/io/vme
.PATH:$(KPATH)

KCOPTS=	-fullwarn

# Generic VMEbus service provider 
VME_NAME=vmeio.o 
VME_CSRCS=vmeio.c

# XIO-PCI-VME board
XIOVME_NAME = xiovme.o
XIOVME_CSRCS = xiovme.c

# PCI/VME bridge -- Universe chip
UNIVERSE_NAME = universe.o
UNIVERSE_CSRCS = universe.c

# User VME driver
USRVME_NAME = usrvme.o
USRVME_CSRCS = usrvme.c

# User UNIVERSE DMA engine driver
UNIVERSE_DMA_ENGINE_NAME = ude.o
UNIVERSE_DMA_ENGINE_CSRCS = ude.c

# VMEbus idbg utilities
VME_IDBG_NAME  = vme_idbg.o
VME_IDBG_CSRCS = vme_idbg.c

# VMEbus stubs
VMESTUBS_NAME = vmestubs.a
VMESTUBS_CSRCS = vmestubs.c
VMESTUBS_OBJS = ${VMESTUBS_CSRCS:.c=.o}
$(VMESTUBS_NAME):$(VMESTUBS_NAME)($(VMESTUBS_OBJS)) MAKELIB

TARGETS = $(VME_NAME) $(XIOVME_NAME) $(UNIVERSE_NAME) $(USRVME_NAME) \
	$(UNIVERSE_DMA_ENGINE_NAME) $(VME_IDBG_NAME) \
	$(VMESTUBS_NAME)

.PRECIOUS:$(TARGETS)

CFILES = $(VME_CSRCS) $(XIOVME_CSRCS) $(UNIVERSE_CSRCS) $(USRVME_CSRCS) \
	$(UNIVERSE_DMA_ENGINE_CSRCS) $(VME_IDBG_CSRCS) \
	$(VMESTUBS_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

#if $(CPUBOARD) == "IP27"
$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}
$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${TARGETS}
	${KDEVINSTALL} ${TARGETS}
#else
$(KCOMMONPREF)default:
$(KCOMMONPREF)install: 
#endif

#VMEidbg.o: VMEidbg.c
#	$(CCF) -c $(JALR) -G 0 -common $(KPATH)/VMEidbg.c && \
#	$(LDF) $(PRODOBJECT) -r $(.TARGET:T) -o $$$$.o && \
#	mv $$$$.o $(.TARGET:T)











