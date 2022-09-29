#
# Makefile for env(1) and printenv(1).
#
#ident "$Revision: 1.9 $"

include	$(ROOT)/usr/include/make/commondefs
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

TARGETS=env

default: $(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(TARGETS)
	$(INSTALL) -F /usr/bin -lns ../../sbin/$(TARGETS) $(TARGETS)
	$(INSTALL) -F /usr/bin -lns ../../sbin/$(TARGETS) printenv
