#ident "$Revision: 1.1 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=mkmsgs
LLDLIBS=-lgen

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/bin $(TARGETS)
