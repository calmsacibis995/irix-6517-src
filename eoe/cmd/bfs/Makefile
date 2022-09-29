#	Copyright (c) 1984 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.
#ident	"cmd/bfs/Makefile: $Revision: 1.7 $"

include	$(ROOT)/usr/include/make/commondefs

TARGETS=bfs
LLDLIBS=-lw -lgen

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
