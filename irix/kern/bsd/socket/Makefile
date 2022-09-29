#!smake
#
# Makefile for SGI's socket implementation.
#
# $Revision: 4.12 $

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/bsd/socket
.PATH:$(KPATH)

KCINCS=-I$(TOP)/bsd
KCOPTS=-signed -common
LCOPTS += -woff 1196

BSD_NAME=bsd.a
BSD_CSRCS=bsdinit.c \
	uipc_domain.c uipc_socket.c uipc_socket2.c uipc_syscalls.c \
	uipc_socket1.c
BSD_OBJS=$(BSD_CSRCS:.c=.o)
$(BSD_NAME):$(BSD_NAME)($(BSD_OBJS)) MAKELIB

TARGETS=$(BSD_NAME)

LINTNAME=bsdsoc
CFILES=$(BSD_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

#
# BSD_NAME gets installed at top level
#
$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
