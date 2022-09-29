
include	$(ROOT)/usr/include/make/commondefs

TARGETS=sort
LCDEFS=-g
LLDLIBS=-lw

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
