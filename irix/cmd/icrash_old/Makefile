#!smake
#
# Makefile for IRIX crash
#
#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/RCS/Makefile,v 1.2 1999/05/25 19:47:37 tjm Exp $"

#
# If you change the OBJECT_STYLE here ... then you should look 
# below for additional OBJECT_STYLE's.
#
OBJECT_STYLE=n32
include $(ROOT)/usr/include/make/commondefs

TARGETS = icrash

#
# General definitions.
#
VERSION   = 3.4
GCDEFS    = -DICRASH_DEBUG -DI_TEMP 
LCDEFS    = -I. -I -g -Iinclude -DIRIX$(OSVERS) -DVERSION=\"$(VERSION)\"
LLDLIBS   = -L. -ldwarf -lelf

#
# IRIX Crash files.
#
CFILES    = callbacks.c get_ra.s init.c main.c
OFILES    = $(CFILES:.c=.o)
LIBS      = libklib.a libsym.a libutil.a libeval.a libreadline.a libhwreg.a
CMDS      = cmds.a

SUBDIRS = cmds lib

COMMONPREF=icrash_

#
# override standard rule
#
SUBDIR_MAKERULE =  echo "\tcd $$d ; ${MAKE} OBJECT_STYLE=32 \
	VERSION=$(VERSION) ${@:S/$(COMMONPREF)//}"; (cd $$d; \
	${MAKE} OBJECT_STYLE=n32 VERSION="$(VERSION)" ${@:S/$(COMMONPREF)//})

default: $(COMMONPREF)default $(TARGETS)

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

$(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

icrash: $(OFILES) $(LIBS)
	$(CCF) -o $@ $(OFILES) $(CMDS) $(LIBS) $(LDFLAGS) -ltermcap -lelfutil

$(LIBS) $(CMDS): $(COMMONPREF)default

$(COMMONPREF)default: $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(COMMONPREF)install: default
#	${INSTALL} -F /usr/bin icrash
#${INSTALL} ln icrash -F /usr/bin fru

