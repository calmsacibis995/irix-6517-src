#!smake
#       "$Revision: 1.2 $"

include	$(ROOT)/usr/include/make/commondefs

TARGETS=ctest
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
LCOPTS=-fullwarn

default: $(TARGETS)

include $(COMMONRULES)

install: default
#	$(INSTALL) -F /sbin $(TARGETS)
#	$(INSTALL) -F /usr/bin -lns ../../sbin/$(TARGETS) $(TARGETS)
