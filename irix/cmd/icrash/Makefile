#!smake
#
# Makefile for IRIX crash
#
#ident "$Header: "

DEPTH?=.
include ${DEPTH}/icrash_commondefs

TARGETS = icrash

#
# General definitions.
#
GCDEFS    = -DICRASH_DEBUG -DKLIB_DEBUG -DI_TEMP 
KLIBS	  = -lkern -lhwconfig -lsym -lmem -ltrace -lklib -lalloc -lutil
LLDLIBS   = $(KLIBS) -lds -ldwarf -lelf -lexc

#
# IRIX Crash files.
#
CFILES    = error.c getstrings.c init.c main.c proc.c report.c socket.c \
            stream.c struct.c timer.c util.c
OFILES    = $(CFILES:.c=.o)
LIBS	  = libeval.a libreadline.a libhwreg.a
CMDS      = cmds.a

SUBDIRS = cmds lib 

COMMONPREF=icrash_

#
# override standard rule
#
SUBDIR_MAKERULE =  echo "\tcd $$d ; ${MAKE} OBJECT_STYLE=n32 \
	VERSION=$(VERSION) ${@:S/$(COMMONPREF)//}"; (cd $$d; \
	${MAKE} OBJECT_STYLE=n32 VERSION="$(VERSION)" ${@:S/$(COMMONPREF)//})

default: $(COMMONPREF)default $(TARGETS) 

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

$(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

icrash: $(OFILES) $(LIBS) $(_FORCE)
	$(CCF) -o $@ $(OFILES) $(CMDS) $(LIBS) $(LDFLAGS) -ltermcap -lelfutil

$(LIBS) $(CMDS): $(COMMONPREF)default

$(COMMONPREF)default: $(_FORCE) 
	$(SUBDIRS_MAKERULE)

$(COMMONPREF)install: default
	${INSTALL} -F /usr/bin icrash
