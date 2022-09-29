#!smake
#
# Makefile for host which contains global state management. 
#
#ident	"$Id: Makefile,v 1.3 1997/04/02 19:43:59 sp Exp $"

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/os/host
.PATH:$(KPATH)

KCINCS=-I${BOOTAREA}

HOST_NAME=os.a
HOST_CSRCS=vhost.c phost.c

#if defined(CELL_IRIX)
SUBDIRS=cell
#else
SUBDIRS=
HOST_CSRCS+=vhost_lp.c
#endif

HOST_OBJS=${HOST_CSRCS:.c=.o}
$(HOST_NAME):$(HOST_NAME)($(HOST_OBJS)) MAKELIB

TARGETS=$(HOST_NAME)
.PRECIOUS:$(TARGETS)

# lint
LINTNAME=host
CFILES=$(HOST_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

# This default rule is always invoked in BOOTAREA
$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
