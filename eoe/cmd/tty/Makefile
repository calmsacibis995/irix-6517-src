#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=tty

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/bin $(TARGETS)
