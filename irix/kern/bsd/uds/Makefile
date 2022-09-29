#!smake
#
# Makefile for SGI's unix domain socket implementation.
#
# $Revision: 1.12 $
DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/bsd/uds
.PATH:$(KPATH)

KCINCS=-I$(TOP)/bsd
KCOPTS=-signed -common

UDS_NAME=uds.a
UDS_CSRCS=uds.c uipc_proto.c uipc_usrreq.c
UDS_OBJS=$(UDS_CSRCS:.c=.o)
$(UDS_NAME):$(UDS_NAME)($(UDS_OBJS)) MAKELIB

TARGETS=$(UDS_NAME)

LINTNAME=bsduds
CFILES=$(UDS_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}	

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} -idb std.sw.uds $(LBOOTPOSTOP) ${TARGETS}
	${KDEVINSTALL} ${TARGETS}
