#!smake
#ident "$Revision: 1.5 $"
#
# Makefile for mount
#
BASEVERSION=n32bit

include	$(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=mount
CFILES= mount.c

LLDLIBS+= -ldisk
LCDEFS += -D_BSD_TIME

default: $(TARGETS)

include $(CMDCOMMONRULES)

mount: $(CFILES)
	$(CCF) -o $@ mount.c $(LDFLAGS)

n32bitinstall: default
	$(INSTALL) -F /sbin mount
	$(INSTALL) -F /etc -lns ../sbin/mount mount
