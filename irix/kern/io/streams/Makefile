#!smake
# Copyright 1989, Silicon Graphics Inc., Mountain View, CA. 
#
# Makefile for the streams i/o subsystem.
#
#ident "$Revision: 1.21 $"

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/io/streams
.PATH:$(KPATH)

# increase Olimit so streamio.c get optimized (on non-ragnarok builds)
#if $(VCC) == "CFE"
KCOPTS= -Olimit 1600
#endif

STR_NAME=stream.a
STR_CSRCS=stream.c streamio.c strmp.c strcalls.c strsubr.c mbuf_mblk.c strmon.c
STR_OBJS=${STR_CSRCS:.c=.o}
$(STR_NAME):$(STR_NAME)($(STR_OBJS)) MAKELIB

STRIDBG_NAME=stridbg.o
STRIDBG_CSRCS=stridbg.c
STRIDBG_OBJS=$(STRIDBG_CSRCS:.c=.o)

TARGETS=$(STR_NAME) ${STRIDBG_NAME}
.PRECIOUS:$(TARGETS)

LINTNAME=stream
CFILES=$(STR_CSRCS) ${STRIDBG_CSRCS}

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${STR_NAME}
	${KINSTALL} ${STRIDBG_NAME}
	${KDEVINSTALL} ${TARGETS}
	${KDEVINSTALL} ${STRIDBG_NAME}
