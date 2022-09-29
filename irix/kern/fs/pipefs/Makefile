#!smake
#
# Makefile for fs/pipefs.
#
#ident	"$Revision: 1.5 $"
DEPTH?=../..
include $(DEPTH)/kcommondefs
KPATH=$(TOP)/fs/pipefs
.PATH:$(KPATH)

HFILES=pipenode.h \
	pipeioctl.h

PIPE_NAME=pipefs.a
PIPE_CSRCS=pipevnops.c
PIPE_OBJS=$(PIPE_CSRCS:.c=.o)

MAC_NAME=mac_pipe.o
MAC_CSRCS=mac_pipe.c
MAC_OBJS=$(MAC_CSRCS:.c=.o)

TARGETS=$(PIPE_NAME) $(MAC_NAME)
LINTNAME=pipefs

$(PIPE_NAME): $$@($(PIPE_OBJS)) MAKELIB

include $(DEPTH)/kcommonrules

$(KCOMMONPREF)default: $(TARGETS)
	$(KLINKINSTALL) $(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	$(KINSTALL) $(TARGETS)
	${KDEVINSTALL} ${TARGETS}

headers!
	$(INSTALL) -m 444 -F /usr/include/pipefs $(HFILES)
