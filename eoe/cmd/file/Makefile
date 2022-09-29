#
include $(ROOT)/usr/include/make/commondefs

TARGETS=file
MAGIC=magic
LLDLIBS=-lw -lcmd -lelf

default:$(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
	$(INSTALL) -F /etc -idb "config(update)" $(MAGIC)
