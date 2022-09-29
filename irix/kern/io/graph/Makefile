#!smake
# Copyright 1994 Silicon Graphics Inc.
#
# Makefile for the Graph management module
#
#ident "$Revision: 1.3 $"

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/io/graph
.PATH:$(KPATH)

KCOPTS=	-fullwarn

GRAPH_NAME=graph.a
GRAPH_CSRCS=graph.c hwgraph.c strtbl.c

GRAPH_OBJS=${GRAPH_CSRCS:.c=.o}
$(GRAPH_NAME):$(GRAPH_NAME)($(GRAPH_OBJS)) MAKELIB

GRAPHIDBG_NAME=graphidbg.o
GRAPHIDBG_CSRCS=graphidbg.c

TARGETS=$(GRAPH_NAME) $(GRAPHIDBG_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=graph
CFILES=$(GRAPH_CSRCS) $(GRAPHIDBG_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${GRAPH_NAME}
	${KINSTALL} ${GRAPHIDBG_NAME}
	${KDEVINSTALL} ${TARGETS}

graphidbg.o: graphidbg.c
	$(CCF) -c $(JALR) -G 0 -common $(KPATH)/graphidbg.c && \
	$(LDF) $(PRODOBJECT) -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)
