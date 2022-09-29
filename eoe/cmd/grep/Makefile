#!smake
#ident	"$Revision: 1.14 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=grep

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin $(TARGETS)
	$(INSTALL) -F /usr/bin -lns ../../sbin/grep $(TARGETS)
