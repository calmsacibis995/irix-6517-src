#ident "$Revision: 1.1 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=makekey

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/lib $(TARGETS)
