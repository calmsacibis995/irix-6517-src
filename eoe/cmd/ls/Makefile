include	$(ROOT)/usr/include/make/commondefs

TARGETS=ls
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
LLDLIBS=$(ROOT)/usr/$(LIB_LOCATION)/libgen.a $(ROOT)/usr/$(LIB_LOCATION)/libw.a
LCOPTS=-fullwarn

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin $(TARGETS)
	$(INSTALL) -F /usr/bin -lns ../../sbin/$(TARGETS) $(TARGETS)
