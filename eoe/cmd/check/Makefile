#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

CFILES=check.c rsync.c
TARGETS=check rsync

default:$(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/sbin $(TARGETS)
