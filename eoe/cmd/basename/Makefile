#ident "$Revision: 1.18 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=basename
LLDLIBS=-lgen

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(TARGETS)
	${INSTALL} -F /usr/bin -lns ../../sbin/$(TARGETS) $(TARGETS)
