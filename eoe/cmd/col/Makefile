#ident "$Revision: 1.3 $"

include $(ROOT)/usr/include/make/commondefs

LLDLIBS=-lw

TARGETS=col

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/bin $(TARGETS)
