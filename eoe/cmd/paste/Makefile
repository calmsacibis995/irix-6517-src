#ident "$Revision: 1.1 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=paste
LLDLIBS=-lw

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/bin $(TARGETS)
