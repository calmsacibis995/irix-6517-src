#!smake
# Copyright 1994 Silicon Graphics, Inc.  All rights reserved.
#
# Makefile for Btool - code coverage analysis
#
#ident	"$Revision: 1.2 $"

DEPTH?=..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/btool
.PATH:$(KPATH)

CNTFILE=$(BOOTAREA)/btool_map.count
KCOPTS=-fullwarn
KCDEFS=-DNUM_BRANCHES=`cat $(CNTFILE)`

BTOOL_NAME=btool_lib.o
BTOOL_CSRCS=btool_lib.c

TARGETS=$(BTOOL_NAME)

LINTNAME=btool
CFILES=$(BTOOL_CSRCS)


#
# Rules
# There is no reason to install the lib anywhere - it is
# simply combined with os.a into kernel.o
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	#${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	#${KINSTALL} ${TARGETS}
	#${KDEVINSTALL} ${TARGETS}

btool_lib.o:$(CNTFILE)

$(CNTFILE):
	if [ ! -f $(CNTFILE) ]; then  \
		echo 1 >$(CNTFILE); \
	fi
