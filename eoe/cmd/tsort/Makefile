#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

CFILES=tsort.c
TARGETS=tsort

default:$(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
