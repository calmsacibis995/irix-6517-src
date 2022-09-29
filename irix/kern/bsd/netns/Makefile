#!smake
#ident	"$Revision: 3.15 $"
#
# Makefile for Xerox Network Services (ns) code.
#
DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/bsd/netns
.PATH:$(KPATH)

KCINCS=-I$(TOP)/bsd
KCOPTS=-signed

BSD_NAME=
BSD_CSRCS=
BSD_OBJS=$(BSD_CSRCS:.c=.o)
$(BSD_NAME):$(BSD_NAME)($(BSD_OBJS)) MAKELIB

TARGETS=$(BSD_NAME)

LINTNAME=bsdnetns
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

headers!
	${INSTALL} -m 444 -F /usr/include/netns *.h
