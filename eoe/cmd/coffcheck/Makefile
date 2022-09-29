#ident "$Revision: 1.3 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=coffcheck

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/sbin $(TARGETS)
