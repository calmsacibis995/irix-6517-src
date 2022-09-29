#ident "$Revision: 1.5 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=comm
LLDLIBS=-lgen

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/bin $(TARGETS)
