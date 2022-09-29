#!smake
# Copyright 1995, Silicon Graphics Inc., Mountain View, CA. 
#
# Makefile for Distributed Process Management
#
#ident  "$Id: Makefile,v 1.3 1997/01/27 23:09:21 sp Exp $"

DEPTH?=../../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/os/file/cell
.PATH:$(KPATH)

LCFLAGS+=-I$(TOP)/os/file

DFILE_NAME=os.a
DFILE_IDLS=dsfile.idl dcfile.idl
KERNIDLFILES=$(DFILE_IDLS)
DFILE_CSRCS=dsfile.c dcfile.c dfile_subr.c \
		$(DFILE_IDLS:S/^/I_/g:.idl=_stubs.c)
DFILE_OBJS=${DFILE_CSRCS:.c=.o}

$(DFILE_NAME):$(DFILE_NAME)($(DFILE_OBJS)) MAKELIB

TARGETS=$(DFILE_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=file
CFILES=$(DFILE_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
