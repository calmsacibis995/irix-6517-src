#!smake
#
# Makefile for fs/namefs.
#
#ident	"$Revision: 1.5 $"
DEPTH?=../..
include $(DEPTH)/kcommondefs
KPATH=$(TOP)/fs/namefs
.PATH:$(KPATH)

CFILES=namevfs.c namevno.c
HFILES=namenode.h

TARGETS=namefs.a
LINTNAME=namefs

$(TARGETS): $$@($(OBJECTS)) MAKELIB

include $(DEPTH)/kcommonrules

$(KCOMMONPREF)default: $(TARGETS)
	$(KLINKINSTALL) $(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	$(KINSTALL) $(TARGETS)
	${KDEVINSTALL} ${TARGETS}

headers!
	$(INSTALL) -m 444 -F /usr/include/namefs $(HFILES)
