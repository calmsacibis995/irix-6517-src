#ident "$Revision: 1.4 $"

include $(ROOT)/usr/include/make/commondefs

CFILES=strings.c
TARGETS=strings
LLDLIBS=-lw

default:$(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
