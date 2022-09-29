#ident "$Revision: 1.3 $"

include $(ROOT)/usr/include/make/commondefs

CFILES=groups.c
TARGETS=groups

default:$(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
