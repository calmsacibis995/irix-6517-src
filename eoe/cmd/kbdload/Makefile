#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)kbdload:kbdload.mk	1.1"

#ident 	"$Revision: 1.1 $"

include $(ROOT)/usr/include/make/commondefs

DIR = /usr/bin

OFILES = loader.o main.o scrutiny.o util.o

TARGETS = kbdload

default: $(TARGETS)

kbdload: $(OFILES)
	$(CC) $(CFLAGS) $(OFILES) -o $(TARGETS) $(LDFLAGS)

include $(COMMONRULES)

install: default
	$(INSTALL) -f $(DIR) $(TARGETS)
