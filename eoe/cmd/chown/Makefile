#
# Makefile for chown and chgrp.
#
#ident	"$Revision: 1.20 $"

include	$(ROOT)/usr/include/make/commondefs
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

TARGETS= chown

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin chown
	$(INSTALL) -F /sbin -ln chown chgrp
	$(INSTALL) -F /usr/bin -lns ../../sbin/chown chown
	$(INSTALL) -F /usr/bin -lns ../../sbin/chown chgrp
