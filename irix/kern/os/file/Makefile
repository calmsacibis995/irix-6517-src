#!smake
# Copyright 1995, Silicon Graphics Inc., Mountain View, CA. 
#
#ident  "$Id: Makefile,v 1.3 1997/04/02 19:42:37 sp Exp $"

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/os/file
.PATH:$(KPATH)

FILE_NAME=os.a
FILE_CSRCS= vfile.c pfile.c

#if defined(CELL_IRIX)
SUBDIRS=cell
#else
SUBDIRS=
FILE_CSRCS+=vfile_lp.c
#endif

FILE_OBJS=${FILE_CSRCS:.c=.o}
$(FILE_NAME):$(FILE_NAME)($(FILE_OBJS)) MAKELIB

TARGETS=$(FILE_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=os
CFILES=$(FILE_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
