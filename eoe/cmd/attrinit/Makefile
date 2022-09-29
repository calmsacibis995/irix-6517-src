#
# Makefile for attrinit
#
#ident	"$Revision: 1.1 $"

include $(ROOT)/usr/include/make/commondefs
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
LCOPTS=-fullwarn

TARGETS= attrinit

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin $(TARGETS)
