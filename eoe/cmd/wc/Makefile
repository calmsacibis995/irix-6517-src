#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/wc/RCS/Makefile,v 1.11 1996/07/24 18:05:39 doucette Exp $"

include	$(ROOT)/usr/include/make/commondefs
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

TARGETS=wc

default: $(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(TARGETS)
	$(INSTALL) -F /usr/bin -lns ../../sbin/$(TARGETS) $(TARGETS)
