#!smake
#
# Makefile for fs/fifofs.
#
#ident	"$Revision: 1.7 $"
DEPTH?=../..
include $(DEPTH)/kcommondefs
KPATH=$(TOP)/fs/fifofs
.PATH:$(KPATH)

FIFO_NAME=fifofs.a
FIFO_CSRCS=fifosubr.c fifovnops.c
FIFO_OBJS=$(FIFO_CSRCS:.c=.o)
$(FIFO_NAME): $$@($(FIFO_OBJS)) MAKELIB

HFILES=fifonode.h

MAC_NAME=mac_fifo.o
MAC_CSRCS=mac_fifo.c
MAC_OBJS=$(MAC_CSRCS:.c=.o)

TARGETS=$(FIFO_NAME) $(MAC_NAME)
LINTNAME=fifofs
CFILES=$(FIFO_CSRCS) $(MAC_CSRCS)

#
# Rules
#

include $(DEPTH)/kcommonrules

$(KCOMMONPREF)default: $(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${FIFO_NAME}
	${KINSTALL} ${MAC_NAME}
	${KDEVINSTALL} ${TARGETS}

headers!
	$(INSTALL) -m 444 -F /usr/include/fifofs $(HFILES)
