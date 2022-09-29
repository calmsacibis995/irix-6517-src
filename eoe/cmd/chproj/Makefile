#
# Makefile for chproj
#
#ident	"$Revision: 1.1 $"

include	$(ROOT)/usr/include/make/commondefs
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
LCOPTS=-fullwarn

TARGETS= chproj

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin chproj
	$(INSTALL) -F /usr/bin -lns ../../sbin/chproj chproj

