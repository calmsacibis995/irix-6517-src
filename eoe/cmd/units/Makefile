#
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/units/RCS/Makefile,v 1.9 1995/10/03 03:06:06 ack Exp $"
#
include	$(ROOT)/usr/include/make/commondefs

TARGETS=units
LLDLIBS=-lw

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
	$(INSTALL) -F /usr/share/lib -m 444 unittab
