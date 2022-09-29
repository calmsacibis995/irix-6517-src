#ident "$Revision: 1.10 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=sleep
LLDLIBS=$(ROOT)/usr/lib/libgen.a
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(TARGETS)
	$(INSTALL) -F /usr/bin -lns ../../sbin/$(TARGETS) $(TARGETS)
