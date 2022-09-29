#!smake
#
# Makefile for network interface, routing, and raw socket code.
#
# $Revision: 3.24 $

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/bsd/net
.PATH:$(KPATH)

KCINCS=-I$(TOP)/bsd
KCOPTS=-signed -common

BSD_NAME=bsd.a
BSD_CSRCS=if.c if_loop.c multi.c netisr.c raw.c raw_cb.c raw_proto.c \
	 raw_usrreq.c radix.c route.c rtsock.c slcompress.c \
	 compat.c rsvp.c rsvp_qo.c
BSD_OBJS=$(BSD_CSRCS:.c=.o)
$(BSD_NAME):$(BSD_NAME)($(BSD_OBJS)) MAKELIB

TARGETS=$(BSD_NAME)

LINTNAME=bsdnet
CFILES=$(BSD_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}
#
# BSD_NAME gets installed at top level
#
$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	${INSTALL} -m 444 -F /usr/include/net *.h
