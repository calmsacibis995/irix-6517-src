#!smake
# Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
#
# Makefile for mips-specific network interfaces.
#
# $Revision: 1.6 $

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/io/1394
.PATH:$(KPATH)
SUBDIRS=

# lets get some dates
#
VERS    !=date '+-DVERS_Y=%y -DVERS_M=%m -DVERS_D=%d -DVERS_H=%H -DVERS_MM=%M'\
		| sed -e 's/=0/=/g'

KCINCS	=-I$(TOP)/bsd -I$(TOP)/bsd/misc
KCOPTS	=-woff 1174,1552 -common ${VERS}

#
# adfw (Adaptec PELE 1394 PCI evaluation board)
#
ADFW_NAME=adfw.o
ADFW_CSRCS=adfw.c

#
# tifw (TI LYNX 1394 PCI evaluation board)
#
TIFW_NAMES=ti_lynx.o ieee1394.o dvc1394.o rawiso1394.o
TIFW_CSRCS=ti_lynx.c ieee1394.c dvc1394.c rawiso1394.c

# PRODTARGETS are applicable to a specific system
#if $(CPUBOARD) == "IP32" || $(CPUBOARD) == "IP32SIM"
#PRODTARGETS=$(ADFW_NAME) $(TIFW_NAMES)
PRODTARGETS=$(TIFW_NAMES)
TARGETS=$(PRODTARGETS)
#endif

LINTNAME=bsdmips
CFILES=$(ADFW_CSRCS) $(TIFW_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${LBOOTPOSTOP} ${TARGETS}
#	$(KINSTALL) $(IDBNOSHIP) $(LBOOTPOSTOP) $(XXX_NAME)
	${KDEVINSTALL} ${TARGETS}
