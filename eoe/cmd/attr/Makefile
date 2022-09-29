#
# Makefile for chown and chgrp.
#
#ident	"$Revision: 1.4 $"

include $(ROOT)/usr/include/make/commondefs
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
LCOPTS=-fullwarn
WOFF=

TARGETS= attr

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin attr
	$(INSTALL) -F /usr/bin -lns ../../sbin/attr attr
