#ident "$Revision: 1.3 $"

include $(ROOT)/usr/include/make/commondefs

CVERSION=-cckr

TARGETS=mt
LLDLIBS=-lrmt
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(TARGETS)
	$(INSTALL) -F /usr/bin -lns ../../sbin/$(TARGETS) $(TARGETS)
