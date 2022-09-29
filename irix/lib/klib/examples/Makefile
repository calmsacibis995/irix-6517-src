#!smake
#
# Makefile for KLIB example programs
#
#ident "$Header: "

DEPTH?=..

OBJECT_STYLE=n32
include $(ROOT)/usr/include/make/commondefs

TARGETS = klib sym mem alloc ctime

#
# General definitions.
#
VERSION   = 1.0
GCDEFS    = -DKLIB_DEBUG -DALLOC_DEBUG
LCDEFS    = -I. -I -g -DVERSION=\"$(VERSION)\"
LIBS      = -lkern -lsym -lmem -lklib -lutil -lalloc 

COMMONPREF=examples_

#
# override standard rule
#
#SUBDIR_MAKERULE =  echo "\tcd $$d ; ${MAKE} OBJECT_STYLE=32 \
#	VERSION=$(VERSION) ${@:S/$(COMMONPREF)//}"; (cd $$d; \
#	${MAKE} OBJECT_STYLE=n32 VERSION="$(VERSION)" ${@:S/$(COMMONPREF)//})

default: $(COMMONPREF)default $(TARGETS)

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

klib: klib.o $(LIBS)
	$(CCF) -o $@ klib.o $(LDFLAGS) $(LIBS) -ldwarf -lelf

sym: sym.o $(_FORCE)
	$(CCF) -o $@ sym.o $(LDFLAGS) -lsym -lklib -lutil -lalloc -ldwarf -lelf 

mem: mem.o $(_FORCE)
	$(CCF) -o $@ mem.o $(LDFLAGS) -lmem -lklib

alloc: alloc.o  $(_FORCE)
	$(CCF) -o $@ alloc.o $(LDFLAGS) -lalloc

ctime: ctime.o $(_FORCE)
	$(CCF) -o $@ ctime.o $(LDFLAGS) -lklib -lalloc

all: klib sym mem alloc ctime

$(LIBS) $(CMDS): $(COMMONPREF)default

$(COMMONPREF)default: $(_FORCE)
#	$(SUBDIRS_MAKERULE)

#$(COMMONPREF)install: default
#	${INSTALL} -F /usr/bin icrash
#${INSTALL} ln icrash -F /usr/bin fru

