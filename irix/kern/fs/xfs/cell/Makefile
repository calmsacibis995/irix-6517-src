#!smake
# Copyright 1995, Silicon Graphics Inc., Mountain View, CA. 
#
# Makefile for Distributed XFS
#
#ident	"$Revision: 1.1 $"


DEPTH?=../../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/fs/xfs/cell
.PATH:$(KPATH)

SUBDIRS=v1

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
