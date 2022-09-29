#ident "$Revision: 1.5 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=touch
LLDLIBS=$(ROOT)/usr/$(LIB_LOCATION)/libgen.a
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(TARGETS)
	$(INSTALL) -F /usr/bin -lns ../../sbin/$(TARGETS) $(TARGETS)
	$(INSTALL) -F /usr/bin -lns ../../sbin/$(TARGETS) settime
