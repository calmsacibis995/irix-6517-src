#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/chmod/RCS/Makefile,v 1.9 1995/08/28 00:02:27 doucette Exp $"

include	$(ROOT)/usr/include/make/commondefs

TARGETS=chmod
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin $(TARGETS)
	$(INSTALL) -F /usr/bin -lns ../../sbin/chmod chmod
