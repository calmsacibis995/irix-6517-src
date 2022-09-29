#	Copyright (c) 1984 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/ul/RCS/Makefile,v 1.8 1996/03/13 23:55:06 wtk Exp $"
#
include	$(ROOT)/usr/include/make/commondefs

TARGETS=ul
LLDLIBS=-lcurses
LLDOPTS=-Wl,-woff,85

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
