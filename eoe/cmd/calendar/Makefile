#	Copyright (c) 1984 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/calendar/RCS/Makefile,v 1.10 1992/03/17 08:41:33 ism Exp $"
#	calendar make file

include	$(ROOT)/usr/include/make/commondefs

TARGETS=calprog calnames calendar
LLDLIBS=-lw

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin calendar
	$(INSTALL) -F /usr/lib calprog calnames
