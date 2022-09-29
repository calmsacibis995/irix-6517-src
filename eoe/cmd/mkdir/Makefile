#ident "$Revision: 1.12 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=mkdir
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
LLDLIBS=$(ROOT)/usr/$(LIB_LOCATION)/libgen.a

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(TARGETS)
	$(INSTALL) -F /usr/bin -lns ../../sbin/$(TARGETS) $(TARGETS)
