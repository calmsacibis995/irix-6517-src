#!smake
# Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
#
# Makefile for Token Ring streams.
#
# $Revision: 1.3 $

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/io/tr
.PATH:$(KPATH)

KCINCS	=-I$(TOP)/io/tr -I$(TOP)/bsd -I$(TOP)/bsd/misc
KCOPTS	=

CFILES  =tr_lli.c
TARGETS=${CFILES:.c=.o}
LINTNAME=trio

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${TARGETS}
	${KDEVINSTALL} ${TARGETS}
