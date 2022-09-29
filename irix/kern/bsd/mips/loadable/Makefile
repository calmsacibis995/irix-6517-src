#!smake
# Copyright 1997, Silicon Graphics Inc., Mountain View, CA. 
#
# Master makefile for loadable device driver subsystems.
#
#ident	"$Revision: 1.3 $"

DEPTH?=../../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/bsd/mips/loadable
.PATH:$(KPATH)

#
# loadable kernel modules must be built with these options.
# GNUMVAL is necessary so the ld uses -G 0, rather than -G 8.
#
GNUMVAL=0
LCOPTS=$(GKOPTS) $(GKCOPTS) $(JALR) -G 0

#
# IP32/IP30/IP27 PCI Token Ring
#
PTR_NAME=if_ptr.o
PTR_CSRCS=if_ptr.c


TARGETS=$(PTR_NAME)

# lint
LINTNAME=loadable
CFILES=$(PTR_CSRCS) 


#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} $(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} $(TARGETS)
	${KDEVINSTALL} $(TARGETS)

