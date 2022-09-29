#
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/newform/RCS/Makefile,v 1.4 1992/05/27 15:39:58 wicinski Exp $"
#
include	$(ROOT)/usr/include/make/commondefs

TARGETS=newform
LLDLIBS=-lw

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
