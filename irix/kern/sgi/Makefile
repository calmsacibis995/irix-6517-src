#!smake
# Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
#
# Makefile for SGI kernel facilities.
#
#ident	"$Revision: 3.20 $"

DEPTH?=..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/sgi
.PATH:$(KPATH)

#  
#  Kernel memory allocation routines that previously were included
#  in sgi.a, and now built as kmem.a and kmem_debug.a.  The latter
#  has debugging enabled.
#
TLINKDIR_MAKERULE= \
        for opt in kmem kmem_debug; do \
                tlink -r -x '^kmem' -x '^kmem_debug' -x '^Make.*' . $$opt; \
                echo "\tcd $$opt; $(MAKE) $${RULE:=$@}"; \
                cd $$opt; \
                ln -sf ../Makefile.$$opt ./Makefile; \
                ${MAKE} $${RULE:=$@}; \
                cd ..; \
        done
KDIRT+=$(KPATH)/kmem $(KPATH)/kmem_debug

#
SGI_NAME=sgi.a
SGI_CSRCS=bit.c chunkio.c fs_bio.c libc_src.c qsort.c random.c select.c syscalls.c
SGI_OBJS=$(SGI_CSRCS:.c=.o)
$(SGI_NAME):$(SGI_NAME)($(SGI_OBJS)) MAKELIB

TARGETS=$(SGI_NAME)

LINTNAME=sgi
CFILES=$(SGI_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${TARGETS}
	${KDEVINSTALL} ${TARGETS}
