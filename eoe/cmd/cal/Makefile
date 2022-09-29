#
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/cal/RCS/Makefile,v 1.4 1992/03/16 02:10:52 wicinski Exp $"
#
include	$(ROOT)/usr/include/make/commondefs

TARGETS=cal
LLDLIBS=-lw

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
