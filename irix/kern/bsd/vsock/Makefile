#!smake
# Copyright 1995, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/bsd/vsock
.PATH:$(KPATH)

VSOCK_NAME=bsd.a
VSOCK_CSRCS= vsocket.c lsock.c

#if defined(CELL_IRIX)
SUBDIRS=cell
#else
SUBDIRS=
VSOCK_CSRCS+=vsock_lp.c
#endif

VSOCK_OBJS=${VSOCK_CSRCS:.c=.o}
$(VSOCK_NAME):$(VSOCK_NAME)($(VSOCK_OBJS)) MAKELIB

TARGETS=$(VSOCK_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=bsd
CFILES=$(VSOCK_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

# everything gets installed from somewhere else
$(KCOMMONPREF)install: $(KCOMMONPREF)default
