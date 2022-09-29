#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)alpq:alpq.mk	1.1"

#ident 	"$Revision: 1.1 $"

include $(ROOT)/usr/include/make/commondefs

CFILES = alpq.c
TARGETS = alpq

DIR = /usr/bin

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -f $(DIR) $(TARGETS)
