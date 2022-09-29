include	$(ROOT)/usr/include/make/commondefs

TARGETS=newlabel
LLDLIBS=
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -m 4555 -F /sbin $(TARGETS)
