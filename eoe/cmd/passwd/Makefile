#
# Makefile for passwd(1).
#
#ident "$Revision: 1.19 $"

include	$(ROOT)/usr/include/make/commondefs

# Don't link with -lsun because passwd expects getpw routines to return
#  NIS entries with leading + and not for getpwent to return all NIS entries.
LLDLIBS = -lcrypt -liaf -lcmd 
LCDEFS= -ignore_unresolved -D_SHAREII

CFILES= passwd.c
TARGETS= passwd

LMKDEPFLAGS= $(NULLSUFFIX_MKDEPFLAG)

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin -m 6755 -u root -g sys passwd

