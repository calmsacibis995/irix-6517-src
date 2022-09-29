#ident "$Revision: 1.9 $"

include $(ROOT)/usr/include/make/commondefs
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

TARGETS=test

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(TARGETS)
	${INSTALL} -F /usr/bin -lns ../../sbin/$(TARGETS) "["
	${INSTALL} -F /usr/bin -lns ../../sbin/$(TARGETS) test
