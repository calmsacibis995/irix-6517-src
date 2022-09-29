#ident "$Revision: 1.1 $"

include $(ROOT)/usr/include/make/commondefs

CFILES= sdiff.c
TARGETS= sdiff

default:$(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
