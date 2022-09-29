#ident "$Revision: 1.3 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=sync
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
LLDLIBS=$(ROOT)/usr/lib/libgen.a

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(TARGETS)
	${INSTALL} -F /usr/bin -lns ../../sbin/sync $(TARGETS)
